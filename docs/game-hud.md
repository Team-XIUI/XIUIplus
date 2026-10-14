# Native game HUD visibility

Open `/xiuin gamehud`, or select **Game HUD** in the config sidebar. Each element has a visibility toggle: on leaves it available to the game, off hides it. All elements start on. **Enable controls** pauses or resumes the selected hides without clearing them. The reset button restores all visibility choices for this section.

The section includes player HP/MP/TP bars, party, both alliance lists, target box, cast bar, status icons, compass, clock, connection information, menu title, help bar, both chat windows, and expanded chat. **Chat window 1** hides or restores both the main log and its typing input. Unavailable controls are labeled and cannot be changed. These settings affect the game's built-in HUD; XIUI modules retain their own Enabled options. Hiding the target box preserves the native main-target and sub-target arrows above entities.

XIUI's target snapshot reads the selected target index and entity name, HP percentage, and distance directly from game state. It never reads the native target window's loaded flag or cached text/HP. Player vitals and XP likewise come from the game memory interfaces, so native HUD visibility does not control XIUI module data. Entity indices are checked against the current entity map before access.

Preferences save under `[game_hud]` in `xiuinative.ini`. Native windows still follow the game's normal lifetime, menu, and login rules when their visibility option is on.

Development commands:

- `/xiuin gamehud` opens this config section.
- `/xiuin gamehud show all` restores every element.
- `/xiuin gamehud hide <element>` hides one supported element.
- `/xiuin gamehud show <element>` restores one element.
- `/xiuin gamehud status` reports supported controls, active hides, and suppressed draws.

Element keys are `player`, `party`, `alliance1`, `alliance2`, `target`, `castbar`, `status`, `compass`, `clock`, `connection`, `title`, `help`, `chat1`, `chat2`, and `chatlog`.

## Implementation

The stable host owns native suppression. The hot-swappable runtime supplies only a requested visibility mask, so runtime swaps retain the native draw guard. A faulted runtime supplies an empty mask, restoring native rendering.

The host resolves menu-owner slots in the loaded game module. One validated x86 primitive-draw guard skips only objects whose owner still matches a selected native HUD slot. This covers windows created during the current scene. Other windows, XIUI rendering, and the game window-update logic continue normally. The target box is handled separately at the same draw boundary: only its frame artwork is disabled and its documented box/icon coordinates are moved off-screen. Its native callback and both arrow anchors remain active. Restoration checks the current owner/primitive and changes only fields still carrying XIUI's hidden values; native position updates made while hidden are retained. Compass and connection controls use separately validated one-byte changes, restored only while their current values still match XIUI's changes. The clock uses the game's `/clock off` and `/clock on` commands; restoring a clock hidden by XIUI turns it on.

Party suppression pauses while fishing to retain the native fish HP display. Unloading removes the draw hook, restores target-box fields and owned byte changes, and restores a clock hidden by XIUI. Other addons controlling the same native windows should be unloaded before enabling overlapping controls.

Main chat suppression includes the separately resolved `inline` menu owner, using the same guarded draw path as `logwindo`. Input contents, command processing, and native state are not modified. Both owner slots must resolve before this control is available; showing chat, disabling HUD controls, or unloading restores drawing. Secondary and expanded log toggles do not affect input visibility.

Host/runtime ABI 5 adds the availability and requested-visibility masks. Updating from an earlier ABI requires replacing both DLLs with the host unloaded once.

## Research and dependency notices

Native menu layouts and signatures were checked against the running client and these public references:

- [Ashita hideparty](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/hideparty/hideparty.lua): party/alliance owner locations and native primitive layout.
- [HXIUIBegone](https://github.com/ryuutatsuo23-max/HXIUIBegone): individual native menu records, fishing behavior, and the connection dispatcher.
- [HudBeGone](https://github.com/johndelker/hudbegone): solo/status menu identities and native primitive draw entry/epilogue signatures.
- [Fenestra menu table layout](https://github.com/Windower/Fenestra/blob/e5bfb6442f49bdb4d31859bff6218bbeb1bea620/core/src/hooks/ffximain.cpp#L82-L90).

Those Lua addons are not installed or loaded as dependencies. The host's native implementation uses [MinHook](https://github.com/TsudaKageyu/minhook); its source notices and [license](../third_party/minhook/LICENSE.txt) are retained in `third_party/minhook`.

`xiui_native_hud_target_smoke` checks box-only suppression, unchanged arrow/callback fields, restoration, native coordinate updates, replaced owners, and the regular draw guard for other HUD elements. It also checks combined chat log/input hiding, unchanged input state, independent secondary-log settings, owner replacement, and restoration.
