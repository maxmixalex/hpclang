# Native notebook

A C application combining the declarative UI, physical keyboard text input,
calculator civil clock and owned native string objects. Build with the installed
SDK as described in its getting-started guide. HP39 models additionally require
the Aplet envelope. Local host storage tests cover both backend choices. The
relocated SDK build covers all six launchers and three HP39 Aplet packages.
Recorded original-ROM emulator acceptance covers primary persistence on all
six models, Port 0 backups on HP48gII/HP49g+/HP50g and SD export on HP49g+/HP50g.
These results do not establish physical-device or complete release acceptance.

The application view and event update functions are shared with tests rather
than duplicated in a test-only screen. Host UI tests exercise both 64- and
80-row displays, the maximum note length with all Port controls present,
controlled edits, Save/Restore and service failures. Storage and clock services
are simulated in that UI test; separate Stock integration fixtures exercise
native persistence across repeated launches.

Use the arrow keys to focus controls, Alpha to enter text and Enter to activate
buttons. Save writes the current note; Reload replaces edits only after a
successful read. Exit or ON returns normally without saving further edits.
The displayed clock is sampled on entry and by Refresh clock, not continuously.

Save, Backup, Restore and Export explain common failures such as full storage,
read-only storage, insufficient memory and unavailable services. Failed operations
retain the current edit buffer. Resolve the reported condition and retry; an
unknown failure includes its numeric error code. Export can leave a partial file
as described below, so inspect it before removing it to retry.

HP39 Stock saves in this Aplet's custom data. Its template persists that value
when the Aplet exits normally. Classic Stock stores a string named `HPCNOTE` in
HOME, preserving the current directory. The string has a versioned application
signature. Foreign data at either destination is rejected without replacement;
the initial empty HP39 Aplet list is treated as an unused destination. There is
no automatic write on startup and no replacement of unrelated calculator data.
Unsupported launch contexts report an error while retaining the editable note.

The bounded note holds at most 191 bytes. Invalid UTF-8 and control characters
are rejected on both load and save without changing the note. UTF-8 bytes are preserved inside the
native string; this does not translate them to the Stock character encoding.
This example's small ASCII font shows its fallback glyph for other characters.

## Native Port backups

For ports whose capability query reports backup reads, the view offers a
Restore button. Backup is enabled when creation or replacement is available.
The backup name is `HPCNOTE` and its value uses the same validated record as
the primary store. Restore changes only the edit buffer; Save explicitly
copies it into the primary Aplet/HOME destination. A failed restore retains
the edit buffer. Backup does not automatically save the primary destination.

Backup checks whether the name exists before choosing creation or replacement.
An existing foreign record is rejected. A port with creation but no replacement
can accept a new backup but cannot overwrite an existing one. There is no
delete-and-recreate fallback. These controls follow the public Port capability
mask; they do not add missing native write support. Port 1 replacement remains
unavailable. HP49g+/HP50g Stock Port 2 supports backup creation and transactional
replacement when reported by the capability query. These operations consume
erased tail space; logically deleted records do not reclaim space, and a full
port returns `ENOSPC`. This does not provide automatic power-on recovery or
compaction. The example preserves the editable note when an operation fails.

Host tests cover operation selection and rejected writes for all three port
indices using simulated service capabilities. The separate `notebook-backup`
Stock fixture verifies Port 0 create, restore without changing the primary save,
replacement, and removal across three launches. Recorded acceptance includes
HP48gII, HP49g+ and HP50g ROM-emulator runs. The UI fixture separately checks primary persistence and pixels,
including all three HP39g+ launches with the model-specific build;
this example does not establish completion of M8.6.

## Build resources

Copy the installed `share/hpclang/examples/notebook` directory to your own
application directory. With ATfE on `PATH`, configure one build per calculator:

```sh
cmake -S notebook -B build/notebook-hp39gs \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/share/hpclang/cmake/toolchains/hpclang-armv4t.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/sdk \
  -DHPCLANG_BUILD_MODEL=hp39gs
cmake --build build/notebook-hp39gs
```

Use the corresponding model name for another calculator. `notebook.hpc`,
the raw `.bin` and `notebook-launchers/memory.json` are not transfer files.

### Install on the calculator

For HP48gII, HP49g+ and HP50g, transfer the matching
`notebook-launchers/<model>.hp` in binary mode. Follow the installed
`docs/getting-started.md` section **Transfer and launch on HP48gII, HP49g+ or
HP50g**: HP48gII uses ARMToolbox48 in Port 0; HP49g+/HP50g use ARMToolbox
in Port 2. Recall the transferred Notebook object in RPN mode and launch
it from **ARMTo → PrRUN**.

For the experimental independent HP48gII HOME launcher, configure the model
as `hp48gii` and build `--target notebook_independent_program`. Transfer
`notebook-independent.hp` in binary mode to HOME, recall it in RPN mode and
press EVAL. The editor reads the existing HOME note. ON returns to Stock
with the user's stack values in place. This HOME launcher supplies a
read-only object store. Use the production launcher above to save edits.

For an experimental HP48gII launcher that saves the HOME note, build
`--target notebook_independent_port0_program`. Transfer
`notebook-independent-port0.hp` to HOME under the name `ENTRY48`. In RPN
mode, run `'ENTRY48' RCL 'ENTRY48' PURGE :0:NOTES STO` to install it in
Port 0. Run `:0:NOTES RCL` and press EVAL to open the editor. You can
choose another Port 0 name and use it in both commands.

The default Notebook build does not produce an HP39 Aplet. Prepare the
external HPGCC39 resources as described in the installed
getting-started guide, then wrap the existing launcher:

```sh
python3 /path/to/sdk/share/hpclang/tools/package-aplet.py \
  build/notebook-hp39gs/notebook-launchers/hp39gs.hp \
  build/notebook-hp39gs/notebook-aplet/hp39gs \
  --assets /path/to/prepared-hpgcc39/data --name "Notebook" --author hpclang
```

Use the same model in all paths for HP39g+ or HP40gs. Use the directory
records, Aplet, Reset program and support library as described in the HP39
transfer instructions. Do not send the intermediate `.hp` by itself.
Keep the same installed Aplet across launches: it owns the save. Its
**Reset** view clears its custom data, so use **Exit** or **ON** when you
want to keep the last save.

### Experimental independent HP39/40 Aplet

The installed Notebook example can also build a four-file transfer set that
runs without library 275. In the model-specific build above, configure the
prepared Stock asset directory and build the independent target explicitly.

```sh
cmake -S notebook -B build/notebook-hp39gs \
  -DHPCLANG_NOTEBOOK_APLET_ASSETS=/path/to/prepared-hpgcc39/data
cmake --build build/notebook-hp39gs --target notebook_independent_aplet
```

Transfer the four files in `build/notebook-hp39gs/independent-aplet/` together.
The experimental Aplet appears as **EntryProbe** in the Stock library and
opens with the example note `Seed42`. Use ENTER to open the editor and save
before exiting. The same target works with the hp39gplus and hp40gs model
profiles. Its page layout is qualified for this example.

HP39 Stock provides Aplet-owned persistence, not HOME or Port backups in
Notebook. HP48gII provides HOME and Port 0 but no Port 1/2 or SD export.
HP49g+/HP50g use HOME, expose the native Port operations permitted by the
current launch, and support export to a writable FAT SD card. A successful
USB transfer does not establish media availability or data persistence.

### Independent HP49g+ and HP50g Notebook

Configure the installed example for `hp49gplus` or `hp50g` and build
`--target notebook_independent_program`. The experimental target produces
three binary objects in `notebook-independent/` for the selected model on
Stock 2.15. It defaults to an empty Port 1. For an occupied Port 1, set
`-DHPCLANG_NOTEBOOK_PORT1_OFFSET=<nibbles>` to the known offset of the chain
terminator from the start of Port 1. The offset must be even. Keep that
layout until installation and use a Port without an existing `NBLSE`.

1. Transfer `DATA.HP` in binary mode to HOME as `LEASRC`. In RPN mode run
   `HOME LEASRC :1:NBLSE STO`. After a successful store, run `'LEASRC' PURGE`
   to release the temporary HOME copy.
2. Transfer `USER.HP` as `NBUSER` and `COPY.HP` as `CKCOPY` to HOME.
3. Run `HOME 'NBUSER' RCL CKCOPY EVAL` to open the editor. Save a note, exit
   with ON and repeat the command to reopen it with the caller's stack preserved.

### Application sizing

The application selects picolibc's integer-only formatter because it displays
integer dates, port indices and error codes. Remove that target's
`vfprintf=__i_vfprintf` link option if adding floating-point format conversions.
Its UI arena reserves the controls advertised by the current launch context:
10 base nodes, two per readable Port, and one for file export (17 at most),
with 512 bytes per text buffer. Card removal does not change this reservation.
Use `-DHPCLANG_BUILD_MODEL=hp39gplus` when building for the original HP39g+;
the model-specific SDK build removes services absent on that calculator.

## File export

When stream storage is available, Export creates `/HPCNOTE.TXT` in the active
filesystem, containing the current UTF-8 note followed by a newline. It does
not overwrite an existing file. Move or remove a previous export using the
filesystem tools before exporting again. Service availability does not promise
that a writable SD card is present.

The write loop handles short writes and interruption, always closes the file
and reports write or close errors. A failed export can leave a partial new
file; it is not a transactional file replacement. The application leaves that
file for inspection rather than deleting a path after a possible media change.
The edit buffer and primary native object are unaffected. Host fault tests
cover these cases. The `notebook-export` Stock fixture passes two HP49g+/HP50g
ROM-emulator launches with a raw FAT16 card: UTF-8 output survives relaunch,
repeated export refuses replacement, and independent host reads verify the
output and a retained sentinel. Physical-card acceptance remains separate.
