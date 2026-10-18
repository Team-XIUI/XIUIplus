# XIUI

XIUI is an in-client Final Fantasy XI HUD for Ashita v4. It renders directly into the game back buffer with Direct3D8 and does not launch Electron, Vite, a browser window, a taskbar item, or a local server.

The shareable release is one DLL: [dist/xiuinative.dll](dist/xiuinative.dll). Copy it into Ashita's `plugins` folder and run `/load xiuinative`. Settings, theme, icons, cursors and notices are embedded; existing settings are preserved. See [the changelog](CHANGELOG.md) for release changes.

## Features

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
- Party status ordering and separate configurable buff/debuff rows; independent native Buffs/Debuffs HUDs with real local countdowns, scaling, movement and game descriptions. See [Status Effects](docs/status-effects.md).
- Shared global/per-module visibility policies for menus/events, macro exceptions, alliance-only hiding and Player hide-in-party
- Ready Check with Yes/No prompts, 18-member results, a 30-second countdown, optional party summaries and built-in/custom PCM WAV audio; compatible with the original XIUI addon
- Native chat with tabs, game filters/colors, its own input, Tab completion, scrollback, and a top-right resize handle
- Chat/Chat 2 → Animation offers Off, Fade + slide up, or Fade + slide right for newly arriving messages, plus a 25–300% speed slider; each window saves its own choices
- Native chat input length cap and a default-on Character counter under Chat → General; paste and completion keep Unicode and auto-translate tokens whole
- Independent per-window timestamp colors under Chat → Colors, enabled by default while native/custom message colors remain separate
- Controller focus and navigation for chat history, party/enemy browsing, config, Ready Check and equipment; chat composition currently uses keyboard typing. See [controller usage and limits](docs/controller-implementation.md).
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
- Per-element native game HUD visibility controls, independent of XIUI module visibility
- Explicit Direct3D8 state capture and restoration

## XIUI feature parity

Browse the [combined HTML audit and all 27 implementation records](docs/feature-parity.html): all records have Native implementations, with remaining gameplay/hardware acceptance recorded explicitly. It includes Treasure Pool, Phoenix rules, module improvements and source findings. Record 26 was removed; icon themes exclude backgrounds/cursors.

See the [source-based comparison with Lua XIUI](docs/feature-parity.md) for missing features and partial parity, with a [separate implementation plan for each work package](docs/implementation/README.md). The audit includes the current Native working tree and distinguishes existing functionality from proposed work.

## Architecture

Releases embed the host and renderer in one DLL. The development hot-reload setup uses two native DLLs:

- `xiuinative.dll` is the small, stable Ashita host. It owns Ashita callbacks, game-data snapshots, command routing, and runtime swapping.
- `xiuinative-runtime.dll` contains the HUD, design system, interaction logic, font atlas, and Direct3D8 renderer.

The host never loads the published runtime directly. It copies each published generation to a unique temporary path, validates its ABI, and swaps it between frames. A malformed or unloadable generation is rejected while the previous runtime stays active. Visibility and edit state are preserved across successful swaps.

Theme values live in `config/xiuinative-theme.ini`. The runtime checks the installed theme periodically and applies a changed, stable file without rebuilding either DLL. The `[meter]` section supplies defaults for new per-bar profiles; bar-specific changes made in edit mode persist independently in `xiuinative.ini`.

Source code is separated by responsibility:

- `src/host/` — stable Ashita integration and runtime loader
- `src/runtime/` — hot-swappable HUD and Direct3D8 renderer
- `src/runtime/config/` — reusable config controls and shared drawing/input geometry
- `src/runtime/modules/` — independently configured HUD modules, including Party and both alliance lists
- `src/runtime/notifications/` — reward sources, bounded notification queue, and presentation
- `src/runtime/ui/` — reusable nine-slice and container renderers
- `src/shared/` — versioned host/runtime ABI

Each HUD module has a stable ID and owns its enabled state, bounds, options, persistence hooks, and rendering. `hud_runtime` coordinates the registry, edit mode, dragging, and shared configuration UI. Container and bar dimensions are adjusted through config sliders.

See [the foundation guide](docs/foundations.md) for container and notification APIs.

## Requirements

- Windows
- Ashita v4
- Visual Studio 2022 C++ toolchain with x86 support
- CMake and Ninja
- Ashita v4 plugin SDK

`ASHITA4_SDK_PATH` must point to the SDK directory containing `Ashita.h`.

## Development build and installation

Run this once while XIUI is unloaded:

```powershell
.\scripts\build_all.ps1
```

The script:

1. Configures and builds the x86 stable host and runtime.
2. Runs the native safety smoke tests.
3. Installs or updates the stable host.
4. Atomically publishes `xiuinative-runtime.dll`.
5. Installs the default theme when one does not already exist.
6. Verifies SHA-256 hashes.

Load the stable host:

```text
/load xiuinative
```

## Development hot reload

After the stable host is installed, start the watcher in a separate PowerShell window:

```powershell
.\scripts\dev.ps1
```

The watcher performs an initial runtime publish and then remains active:

- Changes under `src/runtime/` rebuild and atomically publish only the runtime. The game swaps it automatically.
- Changes to `config/xiuinative-theme.ini` publish immediately with no compilation. The runtime applies them automatically.
- Changes under `src/host/` or `src/shared/` produce a warning because those uncommon changes require one manual unload and full build.

A successful runtime update prints `Runtime hot-reloaded` in the Ashita chat log. Build failures and invalid runtime DLLs leave the previous generation running.

`/xiui startup` reports the current runtime's initialization phases, device setup, settings reload and first HUD frame in milliseconds. Initialization totals include the individual startup phases; they do not measure the entire host swap.

To publish one runtime generation without starting the watcher:

```powershell
.\scripts\build_all.ps1 -RuntimeOnly
```

To build and test without installing or publishing:

```powershell
.\scripts\build_all.ps1 -SkipInstall
```

## Theme editing

Edit the repository theme while `scripts/dev.ps1` is running:

```text
config\xiuinative-theme.ini
```

You can also edit the installed theme directly:

```text
<Ashita directory>\config\xiuinative-theme.ini
```

Colors use `AARRGGBB`. Text alignment uses `0` for left, `1` for center, and `2` for right. Values are parsed into a complete temporary theme and committed together, so rendering never observes a partially updated theme.

## Commands

- `/xiui` or `/xiui edit` — toggle edit mode
- `/xiui show` — restore XIUI and the saved native HUD visibility choices
- `/xiui hide` — temporarily hide XIUI and the native HUD, and leave edit mode
- `/xiui toggleui` — toggle XIUI and the native HUD together
- `/xiui reload` — force settings and theme reload
- `/xiui reset` — reset settings and panel positions
- `/xiui status` — show stable-host, runtime, text path, visibility, and edit status
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
- `/xiui readycheck` — configure Ready Check, its text, layout, colors, sound and result announcements
- `/readycheck volume <0-150>` — set the built-in chime volume; `/readycheck test` previews it
- `/xiui world` (or `clock` / `weather`) — configure and move Vana'diel time and weather
- `/xiui notify test` — show sample XP, item, and skill notifications
- `/xiui notify clear` — clear queued notifications
- `/xiui enemylist` — open Enemy list settings and edit samples
- `/xiui enemylist status` — report tracked enemies
- `/xiui party` — configure Party, Alliance 1 and Alliance 2; drag each list independently in edit mode
- `/xiui party status` — report live membership and embedded XIVParty icon availability
- `/xiui statuses` or `/xiui buffs|debuffs` — configure the independently movable native status groups
- `/xiui notify status` — report whether the player world anchor is available
- `/xiui theme` — control the game's Window Type (1–8) from General → Appearance
- `/xiui general` — configure native game HUD visibility
- `/xiui gamehud` — configure native game HUD visibility
- `/xiui gamehud show all` — restore every native HUD element
- `/xiui gamehud hide all` — hide every supported native HUD element
- `/xiui gamehud status` — report native visibility support and active hides
- `/xiui help` — show the command list

`/xiuin`, `/xiuinative` and `/xiui-native` remain compatibility aliases.

Party job artwork is bundled from [XIVParty by Tylas](https://github.com/Tylas11/XivParty), with its [redistribution notice and pinned source](assets/jobs/README.md). Windower is not required.

## Edit mode

1. Run `/xiui edit`.
2. Drag any module from anywhere inside its edit outline. Dashed outlines follow the visible text and bars with 8 pixels of padding on each edge.
3. Choose a module in the left navigation, then use its secondary sections. Chat, Party, Inventory and Extras include selectors for their individual windows or modules. Empty categories are hidden.
4. Use **Layout** for module width and **Bar Style** for bar appearance, height and bookends. Player's **All bars / Per bar** and resource selectors appear in its bar sections. Each resource keeps its own fill colors.
5. Settings use a consistent order: dimensions, placement/offsets, spacing and appearance. Width/height and X/Y pairs share rows. Party → **Window** controls the whole list; **Layout** controls its bars. See the [settings organization and audit](docs/settings-organization.md). Use **Text** to choose a text element and edit its font, size, placement, colors, offsets, shadow, stroke and glow. Party/alliance lists have separate **Sync**, **HP values**, **MP values** and **TP values** profiles. **Animation**, **Effects** and **Colors** organize the bar controls.
6. Click a color row to open the hue/saturation wheel, brightness and opacity controls. Scroll within a section when needed.
   Party → **Jobs** provides paired job-icon X/Y offsets for fixed and name-following icons. Icon offsets and **Text → Cast** offsets leave entry height and bar positions unchanged.
   Job text defaults to **After name**, following name offsets and alignment. **Text → Zone** styles out-of-zone labels independently of Jobs.
   General → **Presets** offers two descriptive **Classic** and **XIV** boxes side by side. Apply confirms before replacing Party and both Alliance lists' appearance settings; each keeps its position. See [Party presets](docs/presets.md).
   **Classic** restores the complete captured Party setup in [config/party-classic.ini](config/party-classic.ini), including text, bars, icon offsets, statuses and tickers.
7. Drag the Configuration panel by its header. The **three-dot menu → Reset to defaults** restores all XIUI settings and positions, including the default **XIUI (Bahnschrift)** font. Its confirmation screen starts on Cancel; choose Reset to apply. Click **x**, or run `/xiui edit`, to save and exit.

Edit mode captures mouse input over XIUI panels and throughout UI drags. The game cursor and mouse controls work outside the panels. Mouse input is restored when edit mode closes, the runtime swaps, or the plugin unloads.

Target HP and its percentage share an inset row along the bottom of the bar; the name remains above. **HP label** hides its label. EXP uses the Player label/value styles, with **EXP label** to show or hide its label and **Info position → Above / Below** to inset its info along either bar edge. The EXP label stays at the upper edge independently. Enemy targets share the enemy name row and align right; long names shorten to fit their own space.

Mouse messages are the only source of button transitions. Config presses apply once at their message position, including fast clicks between rendered frames. Frame updates only refresh hover and dragging. UI gestures retain mouse capture until release, and losing focus cancels the gesture. Adjacent control rows use non-overlapping hit bounds.

Edits save in the background. Rapid changes are combined over a short interval, and unchanged values are not rewritten. Settings reload, runtime replacement, and plugin shutdown finish pending saves before continuing. Toggles, choices, sliders, colors, dragging, resets, and configuration commands share this save path.

Notifications preview in edit mode. **Screen anchor** keeps live rewards left of the character area at a stable screen position, with text growing leftward. Movement and camera changes do not move the anchor. Turn it off to use the draggable manual position. Tune row spacing under Layout, plus duration, rise, colors, text size, visible rows, and reward categories in its tabs. Rows rise together to maintain the selected spacing; an active party invitation holds the stack still. Containers exposes preview dimensions, frame size, padding, opacity, and scan lines.

EXP includes native implementations of [Momentum's EXP features](https://github.com/KiplingFFXI/momentum): configurable rates and ETAs, session pause/reset/history, reward and loss statistics, merit tracking, chain bonuses/countdowns, goals, food/Dedication and EXP rings, Level Sync details and optional alerts. **Display** chooses and orders the additional fields and arranges the bar; **Tracking** groups rates, goals and sync planning. **General** includes pause/reset. The job readout shows main job/subjob and both levels. Added readouts use the normal bright HUD text with its gradient, outline and shadow. Rates need one active minute. Chain estimates use `~`; server-only values stay unknown without a validated status report. Existing bar styles, text effects and saved positions are retained. Dynamis remains outside the EXP module.

**Extras → Commands → General → Window anchor** offers Top or Bottom for the native command window and its connected spell, ability and detail panels. Each panel keeps that edge aligned as its height changes. Existing profiles default to Bottom.

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
See the [Phoenix source audit](docs/phoenix-source-audit.md).

**Extras → Treasure Pool** provides native item rows, lots/passes, winning players,
recent history and estimated/unknown timers. `/xiui treasure` opens it; automatic
appearance is configurable. Lot/Pass and Lot All/Pass All share the same validated
actions. Alerts stay in **Notifications**. Enter controller browsing through the
focus picker or `/xiui focus treasure`: Up/Down changes rows, Left/Right chooses an
action, Confirm activates once, Actions toggles details and Cancel returns to gameplay.
See [Treasure Pool](docs/implementation/07-treasure-pool.md).

**Minimap → Markers → Distant party/alliance** supplements nearby native entities
with same-zone map snapshots when needed. Requests are limited to ten-second
intervals while the map is visible; snapshots expire after twenty seconds.

## Files

To cut a release, update `VERSION` and run `./scripts/package.ps1`. It builds
the standalone x86 plugin and produces **`dist/xiuinative.dll`**. Copy that
one DLL into Ashita's `plugins` folder and run `/load xiuinative`. There is
no installer or companion DLL. `-SkipBuild` publishes an already completed,
checked standalone build.

Maps use embedded WebP quality 90 images with a statically linked decoder.
Icon atlases, cursors and mob data use lossless compression and expand when
loaded; no extra codec, companion DLL or external asset files are needed.

The captured profile in `config/xiuinative-defaults.ini` is embedded in the
runtime. It contains Cody's complete October 15 configuration: 7,301 saved UI
preferences across 409 sections, with the current layout and theme.
Fresh installs and `/xiui reset` use that profile, including
module visibility, layout, bars, text, chat and selection settings. The
theme, icons, cursors and license notices are embedded too. First use
creates the config files and notices; existing settings and themes remain
authoritative. The default HUD font is **XIUI (Bahnschrift)**. The three-dot
menu's **Reset to defaults** confirmation restores the complete captured
configuration, including every module's text, appearance and position. `/xiui save` queues a complete settings save. The host/runtime
DLL pair below is the development hot-reload setup, separate from releases.

See [native game HUD controls](docs/game-hud.md) for the individual elements and implementation notes. Visibility toggles in **Game HUD** are on to show and off to hide; unavailable controls identify unsupported client signatures.

- Layout/settings: `<Ashita directory>\config\xiuinative.ini`
- Live theme: `<Ashita directory>\config\xiuinative-theme.ini`
- Stable host: `<Ashita directory>\plugins\xiuinative.dll`
- Published runtime: `<Ashita directory>\plugins\xiuinative-runtime.dll`

## Credits

The attribution icon beside the config close button opens **General → Attribution**, with separate Projects and Original XIUI contributors sections. Original XIUI separates Founders (Tirem, Shuu), Core Contributors (JQuick, Nerf), and Contributors. GitHub buttons open each profile in your browser.

Kipling created [Cadence](https://github.com/KiplingFFXI/cadence) and [Momentum](https://github.com/KiplingFFXI/momentum), whose timing and EXP features inform the native integrations. Team XIUI / tirem provided the original XIUI and notification artwork, Windower provided the ChatMon sounds, and Phoenix contributors provided the game rules and data. Retained asset notices remain bundled with the plugin.

Cursor artwork is from Kenney's CC0 Cursor Pack. The original notice is retained in `assets/cursors/LICENSE.txt`.

Native command/details menus: `/xiui commands` opens their movable module. It preserves native actions, item/spell details, selections and cursor handling; disabling movement restores the game positions. HUD containers and the configuration UI follow **Menu → Config → Windows → Shared → Window Type**, using all eight installed DAT skins. `/xiui commands status` and `/xiui theme status` report native support and the selected skin.

Target includes **World Target Bar**, an optional small HP bar beneath the selected world target, and **Mob Info** group sub-tabs for combined rows, separate rows or independent movement. See [World Target Bar](docs/world-target-bar.md) and [Mob Info](docs/implementation/06-mob-information.md).

Notifications support **2–6 groups** with per-kind routing and independent positions, text, appearance and animation. **Animation → Pin invitations** minimizes party/trade notices; hover or `/xiui focus notifications` reveals sender details. See [Notification groups](docs/implementation/22-notification-groups-and-pinning.md).

**General → Appearance → HP Feedback** controls damage trails, healing segments and flashes, with separate switches, colors and timing. **General → Visibility → Hide overhead names** hides player/NPC/enemy names; turn it off to restore them.
