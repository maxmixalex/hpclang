# Native notebook

A C application combining the declarative UI, physical keyboard text input,
calculator civil clock and owned native string objects. Build with the installed
SDK as described in its getting-started guide. HP39 models additionally require
the Aplet envelope. Local host storage tests cover both backend choices; device
and release acceptance are still pending.

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

The bounded note holds at most 191 bytes. UTF-8 bytes are preserved inside the
native string; this does not translate them to the Stock character encoding.
This example's small ASCII font shows its fallback glyph for other characters.
Files, Port backups and multi-session device persistence tests remain follow-up
work for the integrated application; this initial example does not establish
completion of M8.6.
