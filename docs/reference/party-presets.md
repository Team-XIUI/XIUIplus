# Party presets

Open **Party → General** and scroll below the regular settings to choose one of two clickable boxes. They appear side by side when space permits and stack in a narrow configuration window:

- **Classic:** an FFXI-style party setup, with HP above MP, small TP text, SYNC text off, and the Classic text, job-icon and status layout. It is stored in `config/party-classic.ini`.
- **XIV:** horizontal HP/MP/TP bars, custom name and HP-anchored job text, job-icon offsets, status rows and all other Party presentation preferences. It is embedded from `config/party-xiv.ini`.

Each box describes its layout. Clicking anywhere in a box opens a confirmation dialog with **Cancel** selected. Explicit **Apply** replaces the custom appearance settings of Party and both Alliance lists. Every list retains its own position. Other modules and the shared healing potency adjustment keep their settings.

Saved navigation to the earlier separate Presets pages opens Party → General; saved Window navigation opens Window. Opening this page does not change existing configurations.

Mouse, keyboard and controller use the same action and confirmation paths. The cards use the normal native config theme, text settings, focus and scrolling. Results save through ordinary settings and profiles; a preset is applied only through explicit **Apply**.

Updating a preset's file changes the preset only; it is not applied to existing Party/Alliance configurations on reload. Focused check: `xiui_config_input_smoke --party-presets-only config/party-classic.ini config/party-xiv.ini`.
