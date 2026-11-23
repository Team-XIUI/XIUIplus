# HUD fonts

Bahnschrift, Segoe UI and Tahoma use installed Windows font faces. Where one is
not installed, as under Wine on Linux, the HUD draws it with Tahoma or Arial
instead; copying the Windows font into the Wine prefix restores the tuned look.
Grammara Normal is embedded from the supplied `grammara-font.zip`; its original
`info.txt` is retained with the font and embedded alongside it. Highwind Regular
(Twicolabs Fontdation) is embedded the same way for the Zone Name module; its
`info.txt` records the copyright notice from the font file.

Embedded fonts are registered privately in the game process and released on
runtime shutdown. They do not install a Windows font or require external files.
