# Native notebook

A C application combining the declarative UI, physical keyboard text input,
calculator civil clock and owned native string objects. Build with the installed
SDK as described in its getting-started guide. HP39 models additionally require
the Aplet envelope. Local host storage tests cover both backend choices. The
relocated SDK build passes all six launchers and three HP39 Aplet packages;
device and release acceptance are still pending.

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
indices using simulated service capabilities. Device backup/persistence tests
and file export remain follow-up work; this example does not establish
completion of M8.6.
