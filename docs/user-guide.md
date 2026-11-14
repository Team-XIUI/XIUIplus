# XIUI+ user guide

How to use XIUI+ in game. To install it, see the [README](../README.md#install).

## Getting started

- A setup guide opens the first time XIUI+ loads. It introduces profiles and helps you pick your core modules. Run `/xiuiplus setup` to see it again.
- `/xiuiplus` (or `/xiuiplus edit`) opens and closes settings.
- To change language, open settings and select the globe icon in the top-right corner, then choose **English** or **日本語**. Chat and game-supplied names stay in the game client's language.

## What's included

**Player and target**
- Player HP, MP and TP bars, with value formats, colors and an MP cost preview for the selected spell.
- Attack countdown, TP-goal estimates and resting ticks on the Player bar.
- Target bar with name, HP, distance, cast progress and status timers, plus Subtarget and Target of Target.
- Mob information on the Target bar: level, detection, damage adjustments and immunities (Phoenix data).
- [World Target Bar](reference/target.md#world-target-bar), a small HP bar under the selected target, and [Target Rings](reference/target.md#target-rings).
- Cast Bar for your own spells and items, and Cast Cost for the highlighted action's cost and cooldown.

**Party and enemies**
- Party and two alliance lists with HP/MP/TP, jobs, statuses, cast progress, leaders, level sync and distance. Each list moves independently.
- Enemy list with HP, distance and casts.
- Pet HUD with vitals, effects, portraits and recasts.
- Buffs and Debuffs windows with real countdowns. See [Status Effects](reference/status.md#behavior).
- [Heal prediction](reference/healing.md#heal-bar-prediction) on Party, Alliance and Player HP bars.
- Ready Check with Yes/No prompts, results for all 18 members and optional sounds. It works with players using the original XIUI addon.

**Chat**
- Chat windows with tabs, game filters and colors, their own input line, Tab completion, scrollback and timestamps.
- Message animations, a character counter and chat bubbles.
- See [chat windows](reference/chat.md#chat-window-behavior), [typing](reference/chat.md#chat-input-limit-and-counter) and [filters](reference/chat.md#native-chat-filters).

**Action Bars**
- Hotbars and a controller crossbar with macros, palettes and keybinds.
- Skillchain hints on your actions, with Retail, Horizon and Phoenix rules.
- Import macros and palettes from XIUI, tHotBar and tCrossBar.
- See [Action Bars](reference/actionbars.md).

**Inventory**
- [Satchel](reference/satchel.md): browse your bags, other characters' saved inventories and storage slips.
- Inventory slot trackers, an equipment grid and a gil display.
- Treasure Pool with lots, passes, winners and history.

**Extras**
- Minimap, Compass, and Time / Weather.
- EXP bar with rates, goals, chains and session history.
- Recast timers, Chains and Magic Burst displays, Phantom Roll, and a Blue Magic Learned alert.
- Notifications for items, EXP, skill gains, invitations and trades.
- Fishing stamina meter for a hooked fish.

**Appearance**
- **XI Theme** uses the game's own window skins and can follow the game's Window Type. **XIV 1.0 Theme** is an alternative look.
- Every module has its own text, bar, color and layout settings.
- Status and job icon themes under **Global → Appearance**.

**Native UI**
- Show, hide and move the game's built-in HUD windows and menus. See [Native UI](reference/game-hud.md).

## Editing your HUD

1. Run `/xiuiplus edit`.
2. Hover a module and drag it to move it.
3. Pick a module in the left sidebar to change its settings. Sections follow the same order everywhere: size, placement, spacing, then appearance.
4. Use **Text** to style a text element, and **Bar Style**, **Colors**, **Animation** and **Effects** for bars.
5. Click a color row to open the color picker.
6. Click **x** or run `/xiuiplus edit` again to save and close.

Things worth knowing:

- **Edit Mode** (the dot in the footer) previews every enabled module at once. With it off, only the module you are editing is previewed.
- **Basic and Advanced**: Basic settings offer the full HUD presets (Classic, XIV, XIVClassic), the theme and a global scale. Advanced has every module's settings. Switch with the settings icon in the footer. See [Basic settings and HUD scale](reference/hud-scaling.md).
- **Search** (Ctrl+F, in Advanced) finds any setting by name.
- **Party presets**: Party has its own [Classic and XIV presets](reference/party.md#party-presets), which keep each list's position.
- **Reset**: the three-dot menu → **Reset to defaults** restores all settings and positions. `/xiuiplus layout defaults` resets positions only.
- **Native UI Edit Mode**: with Edit Mode on, the two-icon selector on the right edge switches to editing the game's own windows.
- Changes save automatically in the background.

## Profiles and sharing

Profiles keep separate layouts and settings. Use the profile selector at the top of settings to switch.

- **Share**: open the three-dot menu beside the profile selector and choose **Share profile → Save file**. Send the `.xiuiplus` file to a friend. It includes your layout and module settings, not your character bindings or controller-device settings.
- **Import**: choose **Import profile → Choose file**, select a `.xiuiplus` or older `.xiui` file, name it and click **Import**. It is added as a new profile; your existing profiles are kept. **Paste old key** still accepts previously shared keys.
- Macros and palettes from the original XIUI import separately through **Action Bars → Import**.

## Controller

XIUI+ can be driven with a controller: settings, chat history, party and enemy lists, Treasure Pool, Ready Check and equipment. Chat typing uses the keyboard.

- `/xiuiplus focus` opens the focus picker; `/xiuiplus focus off` returns to gameplay.
- `/xiuiplus controller` opens **Global → Input**.

See [controller usage and limits](reference/input.md).

## Commands

`/xiuiplus` is the main command. `/xiui` and earlier aliases still work. `/xiuiplus help` lists commands in game.

**General**

| Command | What it does |
|---|---|
| `/xiuiplus edit` | Open or close settings |
| `/xiuiplus setup` | Reopen the setup guide |
| `/xiuiplus show` / `hide` / `toggleui` | Show, hide or toggle XIUI+ together with the native HUD |
| `/xiuiplus reload` | Reload settings and theme |
| `/xiuiplus reset` | Reset settings and positions |
| `/xiuiplus layout defaults` | Restore default positions, keeping appearance and sizes |
| `/xiuiplus preset classic\|xiv` | Apply a complete HUD preset |
| `/xiuiplus status` | Show plugin status |

**Appearance**

| Command | What it does |
|---|---|
| `/xiuiplus theme` | Open **Global → Appearance** |
| `/xiuiplus theme xi` / `theme xiv1` | Switch theme, keeping your layout |
| `/xiuiplus theme 1` … `8` | Change Window Type (XIUI+ only when Follow game is off) |
| `/xiuiplus theme status` | Report the selected theme |

**Modules**

Each of these opens that module's settings. Many also accept `status` to print what the module is tracking.

| Command | Module |
|---|---|
| `/xiuiplus party` | Party and alliance lists; `party preset classic\|xiv` applies a Party preset |
| `/xiuiplus enemylist` | Enemy list |
| `/xiuiplus statuses` (or `buffs`, `debuffs`) | Buffs and Debuffs windows |
| `/xiuiplus castbar` / `castcost` | Cast Bar and Cast Cost |
| `/xiuiplus chat` | Chat; `chat show\|hide\|clear\|status`, and `chat timestamps follow\|off\|hhmm\|seconds` |
| `/xiuiplus keybinds` | Action Bars keybinds |
| `/xiuiplus skillchain` / `magicburst` / `chains` | Skillchain hints, Magic Burst and Chains |
| `/xiuiplus rules retail\|horizon\|phoenix` | Choose the combat rule set |
| `/xiuiplus phantomroll status` | Report tracked rolls and potency |
| `/xiuiplus exp` | EXP; `exp pause\|resume\|reset\|status\|history` and `exp mode auto\|exp\|lp\|cp\|ep` |
| `/xiuiplus satchel` | Satchel |
| `/xiuiplus inventory` / `equipment` | Inventory trackers and the equipment grid |
| `/xiuiplus treasure` | Treasure Pool |
| `/xiuiplus compass` / `world` / `extras` | Compass, Time / Weather and other extras |
| `/xiuiplus recast status` / `minimap status` | Report recast timers or minimap state |
| `/xiuiplus notify test\|clear\|status` | Preview, clear or report notifications |
| `/xiuiplus readycheck` | Ready Check settings |

**Ready Check**

| Command | What it does |
|---|---|
| `/readycheck` | Start a 30-second ready check |
| `/readycheck yes\|no\|close\|status` | Answer or control it |
| `/readycheck volume <0-150>` / `test` | Set or preview the chime |

**Native UI**

| Command | What it does |
|---|---|
| `/xiuiplus general` or `gamehud` | Native HUD visibility settings |
| `/xiuiplus gamehud show all` / `hide all` / `status` | Show or hide every native HUD element, or report support |
| `/xiuiplus nativeui edit` / `reset` | Move the game's own windows, or restore their positions |
| `/xiuiplus commands` | Movable native command and detail menus; `commands status` reports support |

**Controller and diagnostics**

| Command | What it does |
|---|---|
| `/xiuiplus focus [area\|off]` | Open the focus picker, or enter or leave an area |
| `/xiuiplus controller` | Controller settings; `controller status` prints device details |
| `/xiuiplus rings capture` | Record a Target Rings compatibility report under `config/XIUIplus/diagnostics/` |

## Files and backups

XIUI+ keeps everything in your Ashita install's `config/XIUIplus/` folder: `settings.ini`, `theme.ini`, `profiles/`, `inventories/`, `sounds/` and optional `diagnostics/`. Back up this folder to keep your setup.

Data from older versions moves into this folder automatically the first time XIUI+ loads. If a file already exists in the new location it is kept, and the older copy is saved under `legacy/`.

## Credits

The attribution icon beside the settings close button opens **Global → Attribution**, which lists the projects XIUI+ builds on and the original XIUI contributors.

XIUI+ is based on the original [XIUI](https://github.com/tirem/XIUI). Timing and EXP features follow Kipling's [Cadence](https://github.com/KiplingFFXI/cadence) and [Momentum](https://github.com/KiplingFFXI/momentum). Bundled artwork, sounds and data are credited in [third-party notices](third-party-notices.md).
