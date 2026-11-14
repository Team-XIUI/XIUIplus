# First-load setup

Fresh installs open a short setup guide after the game is ready. Existing settings keep their layout and do not trigger it automatically. Run `/xiuiplus setup` to open it again.

The guide introduces XIUI+ and profiles, then walks through Player Bar, Chat, Target Bar, Party List, Minimap and Enemy List. Each module's dialog sits beside its element, choosing another side when needed to stay on screen and avoid covering it. Enabled modules are highlighted with their normal renderer, using a preview when there is no live data. Use XIUI+ / Use game HUD chooses between replacements and their native counterparts; Minimap and Enemy List have On / Off choices. Choosing Use game HUD or Off hides the XIUI+ preview and highlight immediately; enabling it brings them back.

The last page offers Cast Bar, Recast Timers, Buffs / Debuffs, Pet Bar, Experience and Hotbars, plus a button to open Advanced settings. Choices save to the current profile as they are made. Next accepts the current selection even if its button was already selected. Accepting XIUI+ saves the matching native hides; accepting the game HUD restores its windows. These preferences also save when native controls are not ready yet. Back revisits a page; Skip and Finish apply accepted choices and leave unvisited native settings alone. Completion and progress live under `session.setup`, excluded from profile sharing and presets. Reloading an unfinished setup resumes its page.

Setup starts with other XIUI+ modules and native HUD windows temporarily hidden. The current module's preview appears on its page; accepted modules remain visible as you continue. Native alternatives appear only for visited modules that use the game HUD, and optional modules appear when chosen. This display filter leaves untouched saved settings intact and ends on Skip or Finish. Its reveal progress is saved locally with the setup page.

Choosing Use game HUD for Player Bar also restores the native party/status panel in the bottom right. It stays available while XIUI+ Player Bar is off, including after choosing XIUI+ Party, so the local character's resources remain visible.

Mouse buttons activate on a matching release. Tab / Shift+Tab and arrows move focus; Enter or Space selects; Escape goes back, or skips from the first page. Controllers use the same actions with directions, Confirm and Cancel. Confirmation does not repeat while held. Setup suspends input and preview during native text entry, targeting, events, zoning and foreground loss, keeping the page for later.

The Chat page offers **Follow game windows** and **One combined window** when using XIUI+. Follow uses the game's one- or two-window count and sends each native window's messages to the corresponding XIUI+ log. Combined merges messages admitted to either native window into one XIUI+ log and disables the second XIUI+ window. Both choices retain native filters and HUD positions; they save through the existing Messages and Second window settings. Back and reopening setup show the current choice, and the preview includes both logs when enabled. Use game HUD keeps the original chat windows instead.

If native settings are unavailable, Follow waits for a validated count instead of guessing two windows. Combined prevents deferred native initialization from re-enabling the second log. Later chat edits and existing saved layouts remain intact until a setup choice is selected or accepted with Next. Change the choice later through Chat → Messages and General, or revisit `/xiuiplus setup`.

The native window count uses validated config record 180, documented by the [official Ashita config addon](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/config/README.md). Reactive sizing and independent-window behavior are covered in [Chat windows](chat.md#chat-window-behavior).

Source: `hud/hud_runtime_setup.cpp`, `config/setup_wizard.hpp`, `ui/native_window_config.cpp` and `modules/chat/chat_module.cpp`. Focused check: `./scripts/check.ps1 -Name setup-wizard`.
