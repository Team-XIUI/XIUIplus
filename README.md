# XIUI Native

XIUI Native is an in-client Final Fantasy XI HUD for Ashita v4. It renders directly into the game back buffer with Direct3D8 and does not launch Electron, Vite, a browser window, a taskbar item, or a local server.

The shareable release is one DLL: [dist/xiuinative.dll](dist/xiuinative.dll). Copy it into Ashita's `plugins` folder and run `/load xiuinative`. Settings, theme, icons, cursors and notices are embedded; existing settings are preserved. See [the changelog](CHANGELOG.md) for release changes.

## Features

- Player HP, MP, and TP meters in one horizontal row with top-edge overlay labels and live FFXI data
- Automatic Player MP hiding when maximum MP is zero, with HP/TP filling the row; empty MP on a caster remains visible
- Separate Cast Bar and Cast Cost modules: live local spell/item progress, plus highlighted-action costs, base cast times, native cooldowns and charge pools
- Cadence-style attack countdown and TP-goal estimates in Player Bar, plus resting ticks, recovery goals and optional completion sounds
- Optional shared enemy attack countdowns in Target Bar and Enemy List, with cast holds and disabling effects
- Target name, HP, and distance
- EXP progress, percentage, and remaining EXP
- Floating item, XP/limit point, and skill-gain notifications with a stable screen-space anchor
- Enemy list with an individual native container per enemy, HP, distance, target names, and cast progress
- Party and independently movable alliance lists with vitals, jobs/levels, XIVParty job icons, statuses, spell progress, leaders, level sync and distance
- Ready Check with Yes/No prompts, 18-member results, a 30-second countdown, optional party summaries and a built-in chime; compatible with the original XIUI addon
- Native chat with tabs, game filters/colors, its own input, Tab completion, scrollback, and a top-right resize handle
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

- `/xiuin` or `/xiuin edit` — toggle edit mode
- `/xiuin show` — show the HUD
- `/xiuin hide` — hide the HUD and leave edit mode
- `/xiuin reload` — force settings and theme reload
- `/xiuin reset` — reset settings and panel positions
- `/xiuin status` — show stable-host, runtime, text path, visibility, and edit status
- `/xiuin chat` — configure the Chat module; `chat show|hide|clear|status` controls it
- `/xiuin compass` — configure and move the compass
- `/xiuin extras` — configure Compass, World Info and Commands
- `/xiuin exp` — configure EXP, session rates, readouts, chains, goals, status and alerts
- `/xiuin exp pause|resume|reset|status|history` — control local EXP sessions and inspect saved records
- `/xiuin exp mode auto|exp|lp|cp|ep` — select displayed progression without changing the game's earning mode
- `/xiuin inventory` — configure storage slot-dot trackers
- `/xiuin equipment` — configure the equipped-item icon grid
- `/xiuin castbar` — configure the independent player spell/item Cast Bar; `castbar status` reports its live source
- `/xiuin castcost` — configure highlighted spell/ability/mount costs and cooldowns; `castcost status` reports native menu support
- `/readycheck` — start a 30-second party/alliance ready check; `yes|no|close|status` answers or controls it
- `/xiuin readycheck` — configure Ready Check, its text, layout, colors, sound and result announcements
- `/readycheck volume <0-150>` — set the built-in chime volume; `/readycheck test` previews it
- `/xiuin world` (or `clock` / `weather`) — configure and move Vana'diel time and weather
- `/xiuin notify test` — show sample XP, item, and skill notifications
- `/xiuin notify clear` — clear queued notifications
- `/xiuin enemylist` — open Enemy list settings and edit samples
- `/xiuin enemylist status` — report tracked enemies
- `/xiuin party` — configure Party, Alliance 1 and Alliance 2; drag each list independently in edit mode
- `/xiuin party status` — report live membership and embedded XIVParty icon availability
- `/xiuin notify status` — report whether the player world anchor is available
- `/xiuin theme` — control the game's Window Type (1–8) from General → Appearance
- `/xiuin general` — configure native game HUD visibility
- `/xiuin gamehud` — configure native game HUD visibility
- `/xiuin gamehud show all` — restore every native HUD element
- `/xiuin gamehud status` — report native visibility support and active hides
- `/xiuin help` — show the command list

`/xiuinative` and `/xiui-native` remain compatibility aliases.

Party job artwork is bundled from [XIVParty by Tylas](https://github.com/Tylas11/XivParty), with its [redistribution notice and pinned source](assets/jobs/README.md). Windower is not required.

## Edit mode

1. Run `/xiuin edit`.
2. Drag any module from anywhere inside its edit outline. Dashed outlines follow the visible text and bars with 8 pixels of padding on each edge.
3. Choose a module in the left navigation, then use its secondary sections. Chat, Party, Inventory and Extras include selectors for their individual windows or modules. Empty categories are hidden.
4. Use **Layout** for module width and **Bar Style** for bar appearance, height and bookends. Player's **All bars / Per bar** and resource selectors appear in its bar sections. Each resource keeps its own fill colors.
5. Use **Text** to choose a text element and edit its placement, size, colors, offsets, shadow, stroke and glow. **Animation**, **Effects** and **Colors** organize the bar controls.
6. Click a color row to open the hue/saturation wheel, brightness and opacity controls. Scroll within a section when needed.
7. Drag the Configuration panel by its header. **Reset defaults** restores the selected module while preserving its bounds; a second click confirms. Click **x**, or run `/xiuin edit`, to save and exit.

Edit mode captures mouse input over XIUI panels and throughout UI drags. The game cursor and mouse controls work outside the panels. Mouse input is restored when edit mode closes, the runtime swaps, or the plugin unloads.

Target HP and its percentage share an inset row along the bottom of the bar; the name remains above. **HP label** hides its label. EXP uses the Player label/value styles, with **EXP label** to show or hide its label and **Info position → Above / Below** to inset its info along either bar edge. The EXP label stays at the upper edge independently. Enemy targets share the enemy name row and align right; long names shorten to fit their own space.

Mouse messages are the only source of button transitions. Config presses apply once at their message position, including fast clicks between rendered frames. Frame updates only refresh hover and dragging. UI gestures retain mouse capture until release, and losing focus cancels the gesture. Adjacent control rows use non-overlapping hit bounds.

Edits save in the background. Rapid changes are combined over a short interval, and unchanged values are not rewritten. Settings reload, runtime replacement, and plugin shutdown finish pending saves before continuing. Toggles, choices, sliders, colors, dragging, resets, and configuration commands share this save path.

Notifications preview in edit mode. **Screen anchor** keeps live rewards left of the character area at a stable screen position, with text growing leftward. Movement and camera changes do not move the anchor. Turn it off to use the draggable manual position. Tune row spacing under Layout, plus duration, rise, colors, text size, visible rows, and reward categories in its tabs. Rows rise together to maintain the selected spacing; an active party invitation holds the stack still. Containers exposes preview dimensions, frame size, padding, opacity, and scan lines.

EXP includes native implementations of [Momentum's EXP features](https://github.com/KiplingFFXI/momentum): configurable rates and ETAs, session pause/reset/history, reward and loss statistics, merit tracking, chain bonuses/countdowns, goals, food/Dedication and EXP rings, Level Sync details and optional alerts. **Display** chooses and orders the additional fields and arranges the bar; **Tracking** groups rates, goals and sync planning. **General** includes pause/reset. The job readout shows main job/subjob and both levels. Added readouts use the normal bright HUD text with its gradient, outline and shadow. Rates need one active minute. Chain estimates use `~`; server-only values stay unknown without a validated status report. Existing bar styles, text effects and saved positions are retained. Dynamis remains outside the EXP module.

**Extras → Commands → General → Window anchor** offers Top or Bottom for the native command window and its connected spell, ability and detail panels. Each panel keeps that edge aligned as its height changes. Existing profiles default to Bottom.

## Files

To cut a release, update `VERSION` and run `./scripts/package.ps1`. It builds
the standalone x86 plugin and produces **`dist/xiuinative.dll`**. Copy that
one DLL into Ashita's `plugins` folder and run `/load xiuinative`. There is
no installer or companion DLL. `-SkipBuild` publishes an already completed,
checked standalone build.

The captured profile in `config/xiuinative-defaults.ini` is embedded in the
runtime. Fresh installs and **Reset defaults** use that profile, including
module visibility, layout, bars, text, chat and selection settings. The
theme, icons, cursors and license notices are embedded too. First use
creates the config files and notices; existing settings and themes remain
authoritative. `/xiuin save` queues a complete settings save. The host/runtime
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

Native command/details menus: `/xiuin commands` opens their movable module. It preserves native actions, item/spell details, selections and cursor handling; disabling movement restores the game positions. HUD containers and the configuration UI follow **Menu → Config → Windows → Shared → Window Type**, using all eight installed DAT skins. `/xiuin commands status` and `/xiuin theme status` report native support and the selected skin.
