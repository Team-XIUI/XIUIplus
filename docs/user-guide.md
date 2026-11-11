# XIUI+ user guide

Installation and build instructions are in [Development](development.md). For source navigation and focused checks, use [the documentation index](README.md).

## Features

- First-load setup introduces profiles and helps you choose your core modules. Run `/xiuiplus setup` to revisit it.
- Player HP, MP, and TP meters in one horizontal row with top-edge overlay labels and live FFXI data
- Player HP/MP value formats, independent resource text, configurable HP state gradients and TP-ready/overlay/flash colors
- Shared selected-spell MP cost preview on Player and the actual local Party row; Player Bar → General and Party → Resources have independent switches, both on by default, independent of Cast Cost visibility
- Configurable Subtarget with Attached/Independent positioning, plus independently movable Detached Target of Target; native targeting remains game-owned
- Automatic Player MP hiding when maximum MP is zero, with HP/TP filling the row; empty MP on a caster remains visible
- Separate Cast Bar and Cast Cost modules: live local spell/item progress, plus highlighted-action costs, base cast times, native cooldowns and charge pools
- Cadence-style attack countdown and TP-goal estimates in Player Bar, plus resting ticks, recovery goals and optional completion sounds
- Optional shared enemy attack countdowns in Target Bar and Enemy List, with cast holds and disabling effects
- Target name, HP, and distance
- Independent Target cast styling, display switches, optional shared entity colors/HP gradients and pinned mob-level labels
- Pet HUD with five category styles, native vitals, observed effects, portraits/recasts and snapped or independently movable Pet Target; unavailable lifetimes are labeled explicitly
- Mob information integrated into Target Bar, with optional level labels in Target/Subtarget/Target of Target and additional Target rows for detection, damage adjustments, ranks and immunities from the pinned HeadsUp Phoenix snapshot
- Extras → Blue Magic Learned animated banner/audio and Extras → Phantom Roll with two roll seats, native buff timing and base potency/bust chance
- Global status/job icon themes and per-list status overrides under General → Appearance → Window, with embedded art and native fallbacks
- EXP progress, percentage, and remaining EXP
- Floating item, XP/limit point, and skill-gain notifications with a stable screen-space anchor
- Enemy list with configurable columns, individual native containers, HP/distance/casts, separate target panels and entry/click preferences
- Party and independently movable alliance lists with vitals, jobs/levels, XIVParty job icons, statuses, spell progress, leaders, level sync and distance
- Party status ordering and separate configurable buff/debuff rows; independent native Buffs/Debuffs HUDs with real local countdowns, scaling, movement and game descriptions. See [Status Effects](reference/status-effects.md).
- Shared global/per-module visibility policies for menus/events, macro exceptions, alliance-only hiding and Player hide-in-party
- Ready Check with Yes/No prompts, 18-member results, a 30-second countdown, optional party summaries and built-in/custom PCM WAV audio; compatible with the original XIUI addon
- Native chat with tabs, game filters/colors, its own input, Tab completion, scrollback, and a top-right resize handle
- Chat/Chat 2 → Animation offers Off, Fade + slide up, or Fade + slide right for newly arriving messages, plus a 25–300% speed slider; each window saves its own choices
- Native chat input length cap and a default-on Character counter under Chat → General; paste and completion keep Unicode and auto-translate tokens whole
- Independent per-window timestamp colors under Chat → Colors, enabled by default while native/custom message colors remain separate
- Controller focus and navigation for chat history, party/enemy browsing, config, Ready Check and equipment; chat composition currently uses keyboard typing. See [controller usage and limits](reference/input.md).
- Independently movable Compass and Time / Weather modules, using native DAT artwork and the client's Vana'diel clock
- Reusable nine-slice containers using the original game DAT textures, silver frame, corner fades, and scan lines
- Frameless HUD modules containing only text and meter bars
- Shared bars with Window theme artwork from the native DATs or the existing Modern style, retaining animations and per-bar preferences
- In-client edit mode with native dragging, resizing, configuration, and RGB controls
- GPU-backed text with a fixed-function compatibility fallback
- Programmatic meter bookends, antialiased rounded corners, fill/background gradients, background opacity, outside-only borders, adjustable drop-shadow strength/radius, container and fill-specific glows, shape-clipped animated sheens, and smooth fill transitions
- Per-element module text profiles with placement, size, offsets, gradient colors, letter spacing, shadows, strokes, and glows
- Live theme reload without compilation
- C++ renderer hot reload without unloading the Ashita plugin
- Persisted layout and settings
- [XIUI, tHotBar and tCrossBar action import](reference/actionbars.md#importing) through Action Bars → Import, for macros and hotbar/crossbar palettes only
- Per-element native game HUD visibility controls, independent of XIUI+ module visibility
- Explicit Direct3D8 state capture and restoration

## Language

Open `/xiuiplus edit` and select the globe icon in the top-right corner. Choose **English** or **日本語**. The choice applies immediately and stays selected when switching profiles or presets. Incoming chat and game-supplied names retain the game client's language; custom labels and profile names keep their original text.

## Files and backups

XIUI+ keeps its data in your Ashita install's `config/XIUIplus/` folder: `settings.ini`, `theme.ini`, `profiles/`, `inventories/`, `sounds/` and optional `diagnostics/`. Back up this folder to preserve your setup, character links, imported profiles and saved inventories.

Existing installations migrate automatically when XIUI+ loads. Existing files in the new layout take precedence; older conflicting files are preserved under `legacy/`. An interrupted migration retries on the next load. XIUI-generated sounds move into its own folder; referenced custom WAVs are copied from the shared `config/sounds` folder so other addons can keep using them. Shared Ashita files such as `imgui.ini` stay in place. License notices stay embedded in the DLL; older generated copies are cleaned up automatically.

## Share profiles

Open the three-dot menu beside the profile selector and choose **Share profile → Save file**. Attach the saved `.xiuiplus` file in Discord or another message. It includes your layout and module settings; character bindings, session totals and local controller-device settings stay private.

To use a shared file, choose **Import profile → Choose file**, select the `.xiuiplus` or older `.xiui` file, choose a name and click **Import**. This creates and selects a separate profile while keeping your existing profiles. **Ctrl+O** opens the file chooser. **Paste old key** or **Ctrl+V** still accepts previously shared keys. Import legacy XIUI macros and palettes separately through **Action Bars → Import**.

File selection and processing run in the background. The dialog shows its status and keeps Cancel available; cancelling an import leaves your current profile untouched. File selection uses Windows mouse/keyboard controls. Large profiles use files instead of generating long clipboard keys.

## Commands

- `/xiui` or `/xiuiplus edit` — open or close settings
- `/xiuiplus show` — restore XIUI+ and the saved native HUD visibility choices
- `/xiuiplus hide` — temporarily hide XIUI+ and the native HUD, and leave edit mode
- `/xiuiplus toggleui` — toggle XIUI+ and the native HUD together
- `/xiuiplus reload` — force settings and theme reload
- `/xiuiplus reset` — reset settings and panel positions
- `/xiuiplus status` — show stable-host, runtime, text path, visibility, and edit status
- `/xiuiplus rings capture` — record a short Target Rings shader compatibility report; the saved file appears under `config/XIUIplus/diagnostics/`. Optional `stop` finishes early and `status` shows progress or the last saved path. See [Target Rings](reference/target-rings.md).
- `/xiuiplus focus` — open the controller focus picker; `focus chat|chat2|party|enemies|target|buffs|debuffs|config|readycheck|equipment|off` selects or leaves a surface
- `/xiuiplus controller` — open General → Input; `controller status` prints device and mapping observations
- `/xiuiplus chat` — configure the Chat module; `chat show|hide|clear|status` controls it
- `/xiuiplus chat timestamps follow|off|hhmm|seconds` — set Window 1's timestamps; without a choice it reports the current one
- `/xiuiplus compass` — configure and move the compass
- `/xiuiplus extras` — configure Compass, World Info and other XIUI+ extras
- `/xiuiplus chains` — configure the separate skillchain timing and follow-up display
- `/xiuiplus chains status` — report Chains tracking and available actions
- `/xiuiplus phantomroll status` — report tracked rolls with their potency, including any party job bonus
- `/xiuiplus exp` — configure EXP, session rates, readouts, chains, goals, status and alerts
- `/xiuiplus exp pause|resume|reset|status|history` — control local EXP sessions and inspect saved records
- `/xiuiplus exp mode auto|exp|lp|cp|ep` — select displayed progression without changing the game's earning mode
- `/xiuiplus inventory` — configure storage slot-dot trackers
- `/xiuiplus equipment` — configure the equipped-item icon grid
- `/xiuiplus castbar` — configure the independent player spell/item Cast Bar; `castbar status` reports its live source
- `/xiuiplus castcost` — configure highlighted spell/ability/mount costs and cooldowns; `castcost status` reports native menu support
- `/readycheck` — start a 30-second party/alliance ready check; `yes|no|close|status` answers or controls it
- `/xiuiplus layout defaults` — restore default positions fitted to your game window, keeping appearance, sizes, visibility and actions
- `/xiuiplus readycheck` — configure Ready Check, its text, layout, colors, sound and result announcements
- `/readycheck volume <0-150>` — set the built-in chime volume; `/readycheck test` previews it
- `/xiuiplus world` (or `clock` / `weather`) — configure and move Vana'diel time and weather
- `/xiuiplus notify test` — show sample XP, item, and skill notifications
- `/xiuiplus notify clear` — clear queued notifications
- `/xiuiplus enemylist` — open Enemy list settings and edit samples
- `/xiuiplus enemylist status` — report tracked enemies
- `/xiuiplus party` — configure Party, Alliance 1 and Alliance 2; drag each list independently in edit mode
- `/xiuiplus party status` — report live membership and embedded XIVParty icon availability
- `/xiuiplus preset classic|xiv` — restore the complete captured HUD setup, including positions and settings
- `/xiuiplus party preset classic|xiv` — apply the selected Party preset while keeping other modules
- `/xiuiplus statuses` or `/xiuiplus buffs|debuffs` — configure the independently movable native status groups
- `/xiuiplus notify status` — report screen-space anchoring or the saved manual position
- `/xiuiplus theme` — open Global → Appearance; choose XI Theme or XIV 1.0 Theme under Theme (also available in Basic settings)
- `/xiuiplus theme xiv1` / `/xiuiplus theme xi` — switch XIUI+'s theme while keeping saved layouts; `theme native` remains an alias for XI Theme
- `/xiuiplus theme 1` through `/xiuiplus theme 8` — change Window Type; applies only to XIUI+ when Follow game is off
- `/xiuiplus general` — configure native game HUD visibility
- `/xiuiplus nativeui edit` — edit the actual game-rendered native UI
- `/xiuiplus nativeui reset` — restore all native HUD/menu positions while keeping visibility choices
- `/xiuiplus gamehud` — open Native UI → HUD Windows
- `/xiuiplus gamehud show all` — restore every native HUD element
- `/xiuiplus gamehud hide all` — hide every supported native HUD element
- `/xiuiplus gamehud status` — report native visibility support and active hides
- `/xiuiplus help` — show the command list

`/xiuin`, `/XIUIplus` and `/xiui-native` remain compatibility aliases.

Party job artwork is bundled from [XIVParty by Tylas](https://github.com/Tylas11/XivParty), with its [redistribution notice and pinned source](../assets/jobs/README.md). Windower is not required.

## Edit mode

1. Run `/xiuiplus edit`.
2. Drag any module from anywhere inside its edit outline. Dashed outlines follow the visible text and bars with 8 pixels of padding on each edge.
3. Choose a module in the left navigation, then use its secondary sections. Chat, Party, Inventory and Extras include selectors for their individual windows or modules. Empty categories are hidden.
4. Use **Layout** for module width and **Bar Style** for bar appearance, height and bookends. Player's **All bars / Per bar** and resource selectors appear in its bar sections. Each resource keeps its own fill colors.
5. Settings use a consistent order: dimensions, placement/offsets, spacing and appearance. Width/height and X/Y pairs share rows. Party → **Window** controls the whole list; **Layout** controls its bars. See the [settings organization and audit](reference/settings.md). Use **Text** to choose a text element and edit its font, size, placement, colors, offsets, shadow, stroke and glow. Party/alliance lists have separate **Sync**, **HP values**, **MP values** and **TP values** profiles. **Animation**, **Effects** and **Colors** organize the bar controls.
6. Click a color row to open the hue/saturation wheel, brightness and opacity controls. Scroll within a section when needed.
   Party → **Jobs** provides paired job-icon X/Y offsets for fixed and name-following icons. Icon offsets and **Text → Cast** offsets leave entry height and bar positions unchanged.
   Job text defaults to **After name**, following name offsets and alignment. **Text → Zone** styles out-of-zone labels independently of Jobs.
   General → **Presets** offers two descriptive **Classic** and **XIV** boxes side by side. Apply confirms before replacing Party and both Alliance lists' appearance settings; each keeps its position. See [Party presets](reference/party-presets.md).
   **Classic** restores the complete captured Party setup in [config/party-classic.ini](../config/party-classic.ini), including text, bars, icon offsets, statuses and tickers.
7. Drag the Configuration panel by its header. The **three-dot menu → Reset to defaults** restores all XIUI+ settings and positions, including the default **XIUI+ (Bahnschrift)** font. Its confirmation screen starts on Cancel; choose Reset to apply. Click **x**, or run `/xiuiplus edit`, to save and exit.

Settings start with **Edit Mode OFF**. Live modules stay visible; hover one to edit or drag it. Select a module's settings to preview it, or turn the footer's **Edit Mode** dot ON to preview all enabled modules. Turning it OFF clears previews. Hidden modules have no edit controls.

Settings capture mouse input over XIUI+ panels and throughout UI drags. The game cursor and mouse controls work outside the panels. Mouse input is restored when settings close, the runtime swaps, or the plugin unloads.

Target HP and its percentage share an inset row along the bottom of the bar; the name remains above. **HP label** hides its label. EXP uses the Player label/value styles, with **EXP label** to show or hide its label and **Info position → Above / Below** to inset its info along either bar edge. The EXP label stays at the upper edge independently. Enemy targets share the enemy name row and align right; long names shorten to fit their own space.

Mouse messages are the only source of button transitions. Config presses apply once at their message position, including fast clicks between rendered frames. Frame updates only refresh hover and dragging. UI gestures retain mouse capture until release, and losing focus cancels the gesture. Adjacent control rows use non-overlapping hit bounds.

Edits save in the background. Rapid changes are combined over a short interval, and unchanged values are not rewritten. Settings reload, runtime replacement, and plugin shutdown finish pending saves before continuing. Toggles, choices, sliders, colors, dragging, resets, and configuration commands share this save path.

Notifications preview in edit mode. **Screen anchor** keeps live rewards left of the character area at a stable screen position, with text growing leftward. Movement and camera changes do not move the anchor. Turn it off to use the draggable manual position. Tune row spacing under Layout, plus duration, rise, colors, text size, visible rows, and reward categories in its tabs. Rows rise together to maintain the selected spacing; an active party invitation holds the stack still. Containers exposes preview dimensions, frame size, padding, opacity, and scan lines.

EXP includes native implementations of [Momentum's EXP features](https://github.com/KiplingFFXI/momentum): configurable rates and ETAs, session pause/reset/history, reward and loss statistics, merit tracking, chain bonuses/countdowns, goals, food/Dedication and EXP rings, Level Sync details and optional alerts. **Display** chooses and orders the additional fields and arranges the bar; **Tracking** groups rates, goals and sync planning. **General** includes pause/reset. The job readout shows main job/subjob and both levels. Added readouts use the normal bright HUD text with its gradient, outline and shadow. Rates need one active minute. Chain estimates use `~`; server-only values stay unknown without a validated status report. Existing bar styles, text effects and saved positions are retained. Dynamis remains outside the EXP module.

**Native UI** groups the game's built-in HUD Windows, overhead names, macro palette controls and native menus/dialogs. Menus and Dialogs has separate pages for commands, spells/abilities, item details, merchants, auctions, bazaar comments, equipment/inspection, macro palettes and other dialogs. Each window has an independent position. **HUD Windows → Window Placement → Keep menus above chat** (on by default) opens commands, shop prompts, NPC choices and other menus that normally sit above the game's chat log above XIUI+ chat instead; windows you moved keep their custom positions.

With Edit Mode on, the compact right-edge two-icon selector selects **XIUI Edit Mode** or **Native UI Edit Mode**. Native mode reveals the actual game-rendered HUD and outlines its active native windows. Drag supported outlines and use Show/Hide, or use controller Utilities and the movement picker. A window hidden with the editor keeps its native-position outline so it can be restored. Closed menus appear when the game opens them. Reset position clears custom placement while keeping visibility. Native clock/compass placement is unavailable when the client supplies no supported position control. [Native UI details](reference/game-hud.md).

**Action Bars → Hotbars → Keybinds** and **Crossbar → Keybinds** open a slot list directly from the sidebar. Hotbars places it below Actions; Crossbar places it below Controller Setup. The bar menu and `/xiuiplus keybinds` open it too. Select a hotbar binding and press a key combination, or choose a crossbar slot's controller mapping. Clear removes the binding while retaining its action. See [Keybinds](reference/actionbars.md#keybinds).

Skillchain predictions and Retail/Horizon/Phoenix rules are configured under
**Action Bars → Hotbars / Crossbar → Skillchains** (`/xiuiplus skillchain`).
**Open HUD settings** leads to **Extras → Magic Burst** (`/xiuiplus magicburst`)
for the separate display’s visibility, appearance and text. The HUD shows the selected enemy's observed burst window;
hotbar/crossbar hints predict compatible closers and matching offensive spells.
HUD visibility and action predictions have independent switches, with Retail/Horizon/Phoenix
rules and separate Countdown/Chain name/Elements text profiles. Countdown times
are estimates; edit mode provides a separate movable sample.

**Extras → Chains** (`/xiuiplus chains`) is an independent companion to Magic Burst.
It shows the selected enemy's resonating properties, chain step and Wait/Go
countdown, followed by compatible weapon skills, equipped Blue Magic, Scholar
spells and pet skills. Results are grouped and sorted by skillchain level.
Spell hints without Chain Affinity/Azure Lore/Immanence appear dimmed; **Require
spell ability** hides them instead. Current-avatar, jug-pet and automaton-frame
filters default on. Top-down and bottom-up layouts, property colors, per-group
limits and six text profiles are independent settings. These are planning hints;
TP, MP, recasts and server timing still determine whether an action can succeed.

**Phoenix (75 cap)** shares its pinned ToAU combat profile with Phantom Roll and
debuff estimates. Select it in Skillchains or use `/xiuiplus rules phoenix`;
`/xiuiplus rules retail` and `/xiuiplus rules horizon` remain available. Phoenix predictions
retain primary-property priority and the three-second link delay. Rolls show base
potency, and variable debuffs show base ranges rather than exact deadlines.
See the [Phoenix rules and source provenance](../assets/phoenix/README.md).

**Extras → Treasure Pool** provides native item rows, lots/passes, winning players,
recent history and estimated/unknown timers. `/xiuiplus treasure` opens it; automatic
appearance is configurable. Lot/Pass and Lot All/Pass All share the same validated
actions. Alerts stay in **Notifications**. Enter controller browsing through the
focus picker or `/xiuiplus focus treasure`: Up/Down changes rows, Left/Right chooses an
action, Confirm activates once, Actions toggles details and Cancel returns to gameplay.
See [Treasure Pool](modules/treasure.md).

**Minimap → Markers → Distant party/alliance** supplements nearby native entities
with same-zone map snapshots when needed. Requests are limited to ten-second
intervals while the map is visible; snapshots expire after twenty seconds.

## Credits

The attribution icon beside the config close button opens **General → Attribution**, with separate Projects and Original XIUI contributors sections. Original XIUI separates Founders (Tirem, Shuu), Core Contributors (JQuick, Nerf), and Contributors. GitHub buttons open each profile in your browser.

Kipling created [Cadence](https://github.com/KiplingFFXI/cadence) and [Momentum](https://github.com/KiplingFFXI/momentum), whose timing and EXP features inform the native integrations. Team XIUI / tirem provided the original XIUI and notification artwork, Windower provided the ChatMon sounds, and Phoenix contributors provided the game rules and data. Retained asset notices remain bundled with the plugin.

Cursor artwork is from Kenney's CC0 Cursor Pack. The original notice is retained in `assets/cursors/LICENSE.txt`.

Native command/details menus: `/xiuiplus commands` opens their movable module. It preserves native actions, item/spell details, selections and cursor handling; disabling movement restores the game positions. With **XI Theme**, HUD containers and the configuration UI use all eight installed DAT skins. **Global → Appearance → Window → Follow game** is on by default and follows the game's Window Type. Turn it off, then choose **Window Type** directly above it to style XIUI+ independently. Your separate choice is remembered when you turn following back on. **XIV 1.0 Theme** uses navy fabric, aged-metal frames, copper gauges, serif HUD text, transparent chat and a softly fading minimap. Choose it with **Theme** in Basic settings or **Global → Appearance**. Switching themes preserves saved module styling and placement. `/xiuiplus commands status` and `/xiuiplus theme status` report native support and the selected theme.

Target includes **World Target Bar**, an optional small HP bar beneath the selected world target, and **Mob Info** group sub-tabs for combined rows, separate rows or independent movement. See [World Target Bar](reference/world-target-bar.md) and [Mob Info](modules/mobinfo.md).

Notifications support **2–6 groups** with per-kind routing and independent positions, text, appearance and animation. **Animation → Pin invitations** minimizes party/trade notices; hover or `/xiuiplus focus notifications` reveals sender details. See [Notifications](modules/notifications.md).

**Extras → HP Feedback** controls damage trails, healing segments and flashes, with separate switches, colors and timing. **Global → Visibility → Hide overhead names** hides player/NPC/enemy names; turn it off to restore them.
