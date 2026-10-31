# XIUI user guide

Installation and build instructions are in [Development](development.md). For source navigation and focused checks, use [the documentation index](README.md).

## Features

- First-load setup introduces profiles and helps you choose your core modules. Run `/xiui setup` to revisit it.
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
- [XIUI Lua profile import](reference/lua-profile-import.md) through the profile three-dot menu → Import profile → XIUI Lua, with preview and new-profile creation
- Per-element native game HUD visibility controls, independent of XIUI module visibility
- Explicit Direct3D8 state capture and restoration

## Share profiles

Open the three-dot menu beside the profile selector and choose **Share profile → Save file**. Attach the saved `.xiui` file in Discord or another message. It includes your layout and module settings; character bindings, session totals and local controller-device settings stay private.

To use a shared file, choose **Import profile → Choose file**, select the `.xiui` file, choose a name and click **Import**. This creates and selects a separate profile while keeping your existing profiles. **Ctrl+O** opens the file chooser. **Paste old key** or **Ctrl+V** still accepts previously shared keys. XIUI Lua imports remain on the **XIUI Lua** tab.

## Commands

- `/xiui` or `/xiui edit` — open or close settings
- `/xiui show` — restore XIUI and the saved native HUD visibility choices
- `/xiui hide` — temporarily hide XIUI and the native HUD, and leave edit mode
- `/xiui toggleui` — toggle XIUI and the native HUD together
- `/xiui reload` — force settings and theme reload
- `/xiui reset` — reset settings and panel positions
- `/xiui status` — show stable-host, runtime, text path, visibility, and edit status
- `/xiui rings capture` — record a short Target Rings shader compatibility report; the saved file appears under `logs/xiui/`. Optional `stop` finishes early and `status` shows progress or the last saved path. See [Target Rings](reference/target-rings.md).
- `/xiui focus` — open the controller focus picker; `focus chat|chat2|party|enemies|target|buffs|debuffs|config|readycheck|equipment|off` selects or leaves a surface
- `/xiui controller` — open General → Input; `controller status` prints device and mapping observations
- `/xiui chat` — configure the Chat module; `chat show|hide|clear|status` controls it
- `/xiui compass` — configure and move the compass
- `/xiui extras` — configure Compass, World Info and Commands
- `/xiui exp` — configure EXP, session rates, readouts, chains, goals, status and alerts
- `/xiui exp pause|resume|reset|status|history` — control local EXP sessions and inspect saved records
- `/xiui exp mode auto|exp|lp|cp|ep` — select displayed progression without changing the game's earning mode
- `/xiui inventory` — configure storage slot-dot trackers
- `/xiui equipment` — configure the equipped-item icon grid
- `/xiui castbar` — configure the independent player spell/item Cast Bar; `castbar status` reports its live source
- `/xiui castcost` — configure highlighted spell/ability/mount costs and cooldowns; `castcost status` reports native menu support
- `/readycheck` — start a 30-second party/alliance ready check; `yes|no|close|status` answers or controls it
- `/xiui layout defaults` — restore default positions fitted to your game window, keeping appearance, sizes, visibility and actions
- `/xiui readycheck` — configure Ready Check, its text, layout, colors, sound and result announcements
- `/readycheck volume <0-150>` — set the built-in chime volume; `/readycheck test` previews it
- `/xiui world` (or `clock` / `weather`) — configure and move Vana'diel time and weather
- `/xiui notify test` — show sample XP, item, and skill notifications
- `/xiui notify clear` — clear queued notifications
- `/xiui enemylist` — open Enemy list settings and edit samples
- `/xiui enemylist status` — report tracked enemies
- `/xiui party` — configure Party, Alliance 1 and Alliance 2; drag each list independently in edit mode
- `/xiui party status` — report live membership and embedded XIVParty icon availability
- `/xiui preset classic|xiv` — restore the complete captured HUD setup, including positions and settings
- `/xiui party preset classic|xiv` — apply the selected Party preset while keeping other modules
- `/xiui statuses` or `/xiui buffs|debuffs` — configure the independently movable native status groups
- `/xiui notify status` — report screen-space anchoring or the saved manual position
- `/xiui theme` — open Global → Appearance; choose XI Theme or XIV 1.0 Theme under Theme (also available in Basic settings)
- `/xiui theme xiv1` / `/xiui theme xi` — switch XIUI's theme while keeping saved layouts; `theme native` remains an alias for XI Theme
- `/xiui theme 1` through `/xiui theme 8` — change the game's native Window Type
- `/xiui general` — configure native game HUD visibility
- `/xiui gamehud` — configure native game HUD visibility
- `/xiui gamehud show all` — restore every native HUD element
- `/xiui gamehud hide all` — hide every supported native HUD element
- `/xiui gamehud status` — report native visibility support and active hides
- `/xiui help` — show the command list

`/xiuin`, `/xiuinative` and `/xiui-native` remain compatibility aliases.

Party job artwork is bundled from [XIVParty by Tylas](https://github.com/Tylas11/XivParty), with its [redistribution notice and pinned source](../assets/jobs/README.md). Windower is not required.

## Edit mode

1. Run `/xiui edit`.
2. Drag any module from anywhere inside its edit outline. Dashed outlines follow the visible text and bars with 8 pixels of padding on each edge.
3. Choose a module in the left navigation, then use its secondary sections. Chat, Party, Inventory and Extras include selectors for their individual windows or modules. Empty categories are hidden.
4. Use **Layout** for module width and **Bar Style** for bar appearance, height and bookends. Player's **All bars / Per bar** and resource selectors appear in its bar sections. Each resource keeps its own fill colors.
5. Settings use a consistent order: dimensions, placement/offsets, spacing and appearance. Width/height and X/Y pairs share rows. Party → **Window** controls the whole list; **Layout** controls its bars. See the [settings organization and audit](reference/settings.md). Use **Text** to choose a text element and edit its font, size, placement, colors, offsets, shadow, stroke and glow. Party/alliance lists have separate **Sync**, **HP values**, **MP values** and **TP values** profiles. **Animation**, **Effects** and **Colors** organize the bar controls.
6. Click a color row to open the hue/saturation wheel, brightness and opacity controls. Scroll within a section when needed.
   Party → **Jobs** provides paired job-icon X/Y offsets for fixed and name-following icons. Icon offsets and **Text → Cast** offsets leave entry height and bar positions unchanged.
   Job text defaults to **After name**, following name offsets and alignment. **Text → Zone** styles out-of-zone labels independently of Jobs.
   General → **Presets** offers two descriptive **Classic** and **XIV** boxes side by side. Apply confirms before replacing Party and both Alliance lists' appearance settings; each keeps its position. See [Party presets](reference/party-presets.md).
   **Classic** restores the complete captured Party setup in [config/party-classic.ini](../config/party-classic.ini), including text, bars, icon offsets, statuses and tickers.
7. Drag the Configuration panel by its header. The **three-dot menu → Reset to defaults** restores all XIUI settings and positions, including the default **XIUI (Bahnschrift)** font. Its confirmation screen starts on Cancel; choose Reset to apply. Click **x**, or run `/xiui edit`, to save and exit.

Settings start with **Edit Mode OFF**. Live modules stay visible; hover one to edit or drag it. Select a module's settings to preview it, or turn the footer's **Edit Mode** dot ON to preview all enabled modules. Turning it OFF clears previews. Hidden modules have no edit controls.

Settings capture mouse input over XIUI panels and throughout UI drags. The game cursor and mouse controls work outside the panels. Mouse input is restored when settings close, the runtime swaps, or the plugin unloads.

Target HP and its percentage share an inset row along the bottom of the bar; the name remains above. **HP label** hides its label. EXP uses the Player label/value styles, with **EXP label** to show or hide its label and **Info position → Above / Below** to inset its info along either bar edge. The EXP label stays at the upper edge independently. Enemy targets share the enemy name row and align right; long names shorten to fit their own space.

Mouse messages are the only source of button transitions. Config presses apply once at their message position, including fast clicks between rendered frames. Frame updates only refresh hover and dragging. UI gestures retain mouse capture until release, and losing focus cancels the gesture. Adjacent control rows use non-overlapping hit bounds.

Edits save in the background. Rapid changes are combined over a short interval, and unchanged values are not rewritten. Settings reload, runtime replacement, and plugin shutdown finish pending saves before continuing. Toggles, choices, sliders, colors, dragging, resets, and configuration commands share this save path.

Notifications preview in edit mode. **Screen anchor** keeps live rewards left of the character area at a stable screen position, with text growing leftward. Movement and camera changes do not move the anchor. Turn it off to use the draggable manual position. Tune row spacing under Layout, plus duration, rise, colors, text size, visible rows, and reward categories in its tabs. Rows rise together to maintain the selected spacing; an active party invitation holds the stack still. Containers exposes preview dimensions, frame size, padding, opacity, and scan lines.

EXP includes native implementations of [Momentum's EXP features](https://github.com/KiplingFFXI/momentum): configurable rates and ETAs, session pause/reset/history, reward and loss statistics, merit tracking, chain bonuses/countdowns, goals, food/Dedication and EXP rings, Level Sync details and optional alerts. **Display** chooses and orders the additional fields and arranges the bar; **Tracking** groups rates, goals and sync planning. **General** includes pause/reset. The job readout shows main job/subjob and both levels. Added readouts use the normal bright HUD text with its gradient, outline and shadow. Rates need one active minute. Chain estimates use `~`; server-only values stay unknown without a validated status report. Existing bar styles, text effects and saved positions are retained. Dynamis remains outside the EXP module.

**Extras → Commands → General → Window anchor** offers Top or Bottom for the native command window and its connected spell, ability and detail panels. Each panel keeps that edge aligned as its height changes. Existing profiles default to Bottom.

**Action Bars → Hotbars → Keybinds** and **Crossbar → Keybinds** open a slot list directly from the sidebar. Hotbars places it below Actions; Crossbar places it below Controller Setup. The bar menu and `/xiui keybinds` open it too. Select a hotbar binding and press a key combination, or choose a crossbar slot's controller mapping. Clear removes the binding while retaining its action. See [Keybinds](reference/actionbars.md#keybinds).

Skillchain predictions and Retail/Horizon/Phoenix rules are configured under
**Action Bars → Hotbars / Crossbar → Skillchains** (`/xiui skillchain`).
**Open HUD settings** leads to **Extras → Magic Burst** (`/xiui magicburst`)
for the separate display’s visibility, appearance and text. The HUD shows the selected enemy's observed burst window;
hotbar/crossbar hints predict compatible closers and matching offensive spells.
HUD visibility and action predictions have independent switches, with Retail/Horizon/Phoenix
rules and separate Countdown/Chain name/Elements text profiles. Countdown times
are estimates; edit mode provides a separate movable sample.

**Phoenix (75 cap)** shares its pinned ToAU combat profile with Phantom Roll and
debuff estimates. Select it in Skillchains or use `/xiui rules phoenix`;
`/xiui rules retail` and `/xiui rules horizon` remain available. Phoenix predictions
retain primary-property priority and the three-second link delay. Rolls show base
potency, and variable debuffs show base ranges rather than exact deadlines.
See the [Phoenix source audit](archive/phoenix-source-audit.md).

**Extras → Treasure Pool** provides native item rows, lots/passes, winning players,
recent history and estimated/unknown timers. `/xiui treasure` opens it; automatic
appearance is configurable. Lot/Pass and Lot All/Pass All share the same validated
actions. Alerts stay in **Notifications**. Enter controller browsing through the
focus picker or `/xiui focus treasure`: Up/Down changes rows, Left/Right chooses an
action, Confirm activates once, Actions toggles details and Cancel returns to gameplay.
See [Treasure Pool](archive/implementation/07-treasure-pool.md).

**Minimap → Markers → Distant party/alliance** supplements nearby native entities
with same-zone map snapshots when needed. Requests are limited to ten-second
intervals while the map is visible; snapshots expire after twenty seconds.

## Credits

The attribution icon beside the config close button opens **General → Attribution**, with separate Projects and Original XIUI contributors sections. Original XIUI separates Founders (Tirem, Shuu), Core Contributors (JQuick, Nerf), and Contributors. GitHub buttons open each profile in your browser.

Kipling created [Cadence](https://github.com/KiplingFFXI/cadence) and [Momentum](https://github.com/KiplingFFXI/momentum), whose timing and EXP features inform the native integrations. Team XIUI / tirem provided the original XIUI and notification artwork, Windower provided the ChatMon sounds, and Phoenix contributors provided the game rules and data. Retained asset notices remain bundled with the plugin.

Cursor artwork is from Kenney's CC0 Cursor Pack. The original notice is retained in `assets/cursors/LICENSE.txt`.

Native command/details menus: `/xiui commands` opens their movable module. It preserves native actions, item/spell details, selections and cursor handling; disabling movement restores the game positions. With **XI Theme**, HUD containers and the configuration UI follow **Menu → Config → Windows → Shared → Window Type**, using all eight installed DAT skins. **XIV 1.0 Theme** uses navy fabric, aged-metal frames, copper gauges, serif HUD text, transparent chat and a softly fading minimap. Choose it with **Theme** in Basic settings or **Global → Appearance**. Switching themes preserves saved module styling and placement. `/xiui commands status` and `/xiui theme status` report native support and the selected theme.

Target includes **World Target Bar**, an optional small HP bar beneath the selected world target, and **Mob Info** group sub-tabs for combined rows, separate rows or independent movement. See [World Target Bar](reference/world-target-bar.md) and [Mob Info](archive/implementation/06-mob-information.md).

Notifications support **2–6 groups** with per-kind routing and independent positions, text, appearance and animation. **Animation → Pin invitations** minimizes party/trade notices; hover or `/xiui focus notifications` reveals sender details. See [Notification groups](archive/implementation/22-notification-groups-and-pinning.md).

**Extras → HP Feedback** controls damage trails, healing segments and flashes, with separate switches, colors and timing. **Global → Visibility → Hide overhead names** hides player/NPC/enemy names; turn it off to restore them.
