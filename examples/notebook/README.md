# Native notebook

A C application combining the declarative UI, physical keyboard text input,
calculator civil clock and owned native string objects. Build with the installed
SDK as described in its getting-started guide. HP39 models additionally require
the Aplet envelope. Local host storage tests cover both backend choices. The
relocated SDK build passes all six launchers and three HP39 Aplet packages;
device and release acceptance are still pending.

The application view and event update functions are shared with tests rather
than duplicated in a test-only screen. Host UI tests exercise both 64- and
80-row displays, the maximum note length with all Port controls present,
controlled edits, Save/Restore and service failures. Storage and clock services
are simulated in that UI test; native persistence still needs device evidence.

Use the arrow keys to focus controls, Alpha to enter text and Enter to activate
buttons. Save writes the current note; Reload replaces edits only after a
successful read. Exit or ON returns normally without saving further edits.
The displayed clock is sampled on entry and by Refresh clock, not continuously.

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
mask; they do not add missing native write support (currently Port 1 replacement
and Port 2 writes remain unavailable).

Host tests cover operation selection and rejected writes for all three port
indices using simulated service capabilities. The separate `notebook-backup`
Stock fixture verifies Port 0 create, restore without changing the primary save,
replacement, and removal across three launches. HP48gII and HP50g ROM-emulator
runs pass. The UI fixture separately checks primary persistence and pixels,
including all three HP39g+ launches with the model-specific build;
this example does not establish completion of M8.6.

## Build resources

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
