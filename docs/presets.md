# Party presets

Open **Party → General** and scroll below the regular settings to choose one of two clickable boxes. They appear side by side when space permits and stack in a narrow configuration window:

- **Classic:** the captured FFXI-style party setup, with HP above MP, small TP text, SYNC text off, and the saved Classic text, job-icon and status layout.
- **XIV:** Cody's October 16 captured Party settings: horizontal HP/MP/TP bars, custom name and HP-anchored job text, job-icon offsets, status rows and all other Party presentation preferences. The 593-preference snapshot is embedded from `config/party-xiv.ini`.

Each box describes its layout. Clicking anywhere in a box opens the existing confirmation dialog with **Cancel** selected. Explicit **Apply** replaces the custom appearance settings of Party and both Alliance lists. Every list retains its own position. Other modules and the shared healing potency adjustment keep their settings.

The separate Presets pages are removed. Saved navigation to either old preset page opens Party → General; saved Window navigation still opens Window. Existing configurations are not changed by opening this page.

Mouse, keyboard and controller use the same action and confirmation paths. The cards retain the normal native config theme, text settings, focus and scrolling. Results save through ordinary settings and profiles; no preset is applied by this UI migration or by loading the update.

Refreshing XIV changes the saved preset only; it does not apply it to existing Party/Alliance configurations during reload. Classic and global first-install defaults are unchanged. Focused verification: `xiui_config_input_smoke --party-presets-only config/party-classic.ini config/party-xiv.ini` compares the complete captured preferences for Party and both Alliance lists, including confirmation handling and preserved positions.
