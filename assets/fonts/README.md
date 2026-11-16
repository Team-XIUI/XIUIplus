# HUD fonts

Bahnschrift, Segoe UI and Tahoma use installed Windows font faces. Where one is
not installed, as under Wine on Linux, the HUD draws it with Tahoma or Arial
instead; copying the Windows font into the Wine prefix restores the tuned look.
Grammara Normal is embedded from the supplied `grammara-font.zip`; its original
`info.txt` is retained with the font and embedded alongside it.

The embedded font is registered privately in the game process and released on
runtime shutdown. It does not install a Windows font or require external files.
