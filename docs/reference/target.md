# Target and child windows

Main Target owns mob information, cast presentation, the world HP bar and target rings. Subtarget and Target of Target have independent saved display settings.

Shared combat tracking imports spell behavior from the retained database through `scripts/assets/build-status-rules.py`, including damage application, secondary-effect confidence, compound effects and display variants. Dia/Bio/Diaga and Helix damage results map to effect IDs; damage amounts never become status IDs. Blue Magic secondaries inferred from a connecting hit carry a `?` marker and an unconfirmed tooltip until observed application or no-effect evidence confirms them. Failed casts do not create or refresh effects, and possible secondaries cannot replace confirmed records. Known Dia/Bio tiers retain overwrite priority.

Sleep II and Repose use their mapped icon; native Sleep removal and positive HP damage clear the tracked sleep family. Basic/AoE/burst application and removal variants, death and effect theft share the same tracker. An extension notification with no deadline invalidates the old countdown. Target and Enemy List consume these same observations; native player timers remain authoritative where available. Base durations remain estimates. Focused check: `xiui_module_parity_smoke --spell-statuses-only` covers every imported spell and mapped effect; it does not replace gameplay acceptance of server-specific mechanics.

Target status rows measure countdown text at the configured profile size before laying out icons. Wider labels reserve wider cells and wrap; larger text reserves matching row height for below, above and over-icon placement. Rendering, icon hit regions and total Target height use that same layout, preserving text effects and offsets without shrinking each countdown independently. Focused checks: `xiui_module_parity_smoke --status-timers-only` and `--hud-parity-only`.

**Start in:** [target_module.cpp](../../src/runtime/modules/target/target_module.cpp), [target_child_module.cpp](../../src/runtime/modules/target/target_child_module.cpp), [world_target_bar.cpp](../../src/runtime/modules/target/world_target_bar.cpp), [target_rings.cpp](../../src/runtime/modules/target/target_rings.cpp).

**Keep intact:** Revalidate server ID/index and keep unavailable native state explicit. World displays are passive and use the captured scene camera/depth. Rings follow the model's validated render position through `world/entity_render_position.hpp`; movement anchors can run ahead of the mesh. Target casts and information groups must not shift status anchors when activity changes.

Rings default to model-relative sizing through Ashita's model hitbox radius. Keep the band, glow and facing marker proportional, and rebuild when the footprint changes. Fixed yalm sizing remains available in Target → Target Rings → Layout.

Pets with the native Ally flag have health even without Player/Monster flags. Keep HP detection separate from NPC name colors and ring visibility preferences: these pets use their native HP percentage in target displays and ring fill, while ordinary NPCs retain decorative rings. The host uses the same health check for world-bar projection.

World rendering accepts the native fixed-function camera as well as the recognized terrain shader. The latter takes priority when present. Submit only at the matching scene end or scene-copy boundary; `target-ring-camera` covers both paths and frame lifetime.

Opt-in shader diagnostics live in [ring_shader_capture.cpp](../../src/host/ring_shader_capture.cpp). `/xiuiplus rings capture` records rejected and accepted layouts for up to three seconds, then saves a bounded JSON report under the active install's `config/XIUIplus/diagnostics/` directory. Keep its GPU inspection inactive otherwise, and drain its background writer before unload.

Shared meter defaults resolve an automatic Target HP height to 12 pixels; the world bar retains 6 pixels. Classic and XIV apply the same Target text styles and HP height.

XIV 1.0 appearance adds the screenshot-derived name flourish, italic target caption and copper HP gauge. Attached target casts temporarily replace the name with a thin progress bar with themed endcaps; the spell/action name slightly overlaps its upper-left edge. The target name returns when casting ends. They have no icon or medallion, and the HP/status layout stays fixed. Independent target casts use the same compact presentation at their saved position. Text effects and custom cast gradients remain active. The separate player Cast Bar retains its action medallion.

Target of Target defaults below the main Target without a connecting arrow in XIV 1.0, with X/Y offsets of 240/-32 pixels to sit farther right and closer vertically. XI keeps the inline-right default and arrow with zero offsets. Untouched defaults follow theme changes; customized positions and arrow choices remain authoritative. Both shared configuration routes mark explicit layout choices, including controller changes.

Target of Target follows Target's text profiles by default and uses the same ten-pixel HP-edge name padding. The old centered recipient-name default migrates once; later independent text choices remain available. Subtarget retains its independent text defaults.

The XIV 1.0 target header stacks the measured name row, visible flourish and HP gauge, with the flat rail sitting against HP's top edge. Saved text offsets remain active; the name's base position sits close above the rail, the claim badge nests in the left curved loop and the target caption overlays the right edge. Distance follows the target caption with eight pixels between measured text bounds, retaining its own text profile. Caption and distance use the same compact default font size, each at 72% of its saved profile scale. The name uses the full row width. The flourish renderer samples only its visible atlas rows. Its curved cap retains the artwork's proportions and extends left of HP; the first short lower tick meets HP's left edge, with the straight rail continuing to the caption.

The decoration is flipped vertically within its existing bounds; the claim badge sits in its lower loop. The badge is one claim-colored ring using shared `design_system::DrawRing` coverage ramps measured in screen pixels, preserving anti-aliasing through HUD scaling.

Distance text defaults to a brighter ivory gradient. A versioned correction replaces only the old matching gray gradient; later color choices and existing text effects remain authoritative.

Together-mode Behavior, Links and Detection icons form a right-aligned strip just above the Target caption, with two-pixel default spacing. Own-row and independent groups retain their placement. The versioned header migration replaces only the old matching common offsets and icon spacing; later customizations persist.

The main target name anchors ten pixels inside HP's left edge. A versioned migration removes only the old matching 12-pixel name offset; later text offsets remain authoritative. XIV 1.0 text refinements move the name row four pixels down, the target caption eight pixels left and one higher and HP percentage one lower. The HP track is 12 pixels tall, keeping its top edge against the rail as the gauge grows downward.

**Focused check:** `./scripts/check.ps1 -Name target-recipient`, `target-rings` or `mob-info`, according to the changed display.

## Target Rings

Target Rings belong to the **Target** category, whose settings and positions use `target` identifiers.

Open **Target → Target Rings**. Rings start enabled for the selected living monster, with a band matching its current name/claim color, thin inner/outer rims, soft edge glow and a triangle that marks the enemy's front and points along its native facing direction. They follow native target position and the world camera without affecting the normal Target window or its edit bounds.

- **General:** Enabled, HP fill, Show for NPCs and Show for players. NPC/player rings start off; enable each independently. NPCs without a health resource receive a full decorative band. HP fill smoothly interpolates toward the client's actual enemy HP percentage for both damage and healing; disable it for a complete decorative band. New targets start at their own current HP without carrying over the previous target's fill.
- **Layout:** Scale with model size starts on. Inner radius and Outer radius are multiples of the target's native model hitbox radius, so large targets receive larger rings. The defaults are 1.15 and 1.40; a target with a 4.2-yalm hitbox receives a band from 4.83 to 5.88 yalms. The glow, rims and facing marker scale with the band. Turn scaling off to use the existing fixed radii in yalms. Changing either radius keeps the outer radius greater than the inner radius.
- **Colors:** Follow target name color starts on and reuses the shared native name/claim colors, including configured entity-name overrides. The ring, triangle, trim and glow update as claim/name state changes. Ring color retains its opacity in either mode; its custom RGB applies with Follow target name color off.

The renderer reads installed zone DAT geometry in the background, using the terrain decoding already used by PhoenixLighting. It clips each part of the ring onto actual floor triangles, preserving slopes and separate stair levels. Nearby overlapping floors select the surface closest to the target's native ground height. Missing surfaces remain gaps; there is no invented flat-plane fallback. Cutout foliage, blended effects, walls and authored moving objects are excluded. Only static zone floors are supported; moving elevators/platforms are not mapped.

The ring uses the actual world scene surface, authoritative terrain camera and matching depth buffer, including clients that render the world offscreen before copying it to the back buffer. It draws at the verified scene-copy boundary after the remaining world passes; early screen labels are not treated as the end of world rendering. The draw scope unbinds all eight texture stages before temporarily binding the retained scene surfaces, then restores the interrupted copy's targets, all texture bindings, native vertex/index buffers, transforms and viewport. Depth writes remain disabled. The host observes native draws before replacement renderers such as PhoenixLighting consume them. Terrain stays in memory for the current zone; the mesh rebuilds only when the target, position, facing, animated HP fill or appearance changes. Zoning, hidden/unrendered entities, dead health-bearing targets and unavailable terrain draw nothing. Target's enabled and contextual visibility controls also apply.

Native fixed-function world rendering supplies a usable camera without the recognized terrain vertex shader. It follows the same scene-end/copy boundaries and matching depth-buffer checks; a recognized terrain camera takes priority when present. Ring submission does not require the terrain shader. Host diagnostics include fixed-function frames and their scene/depth information. `./scripts/check.ps1 -Name target-ring-camera` checks both camera paths on an isolated Direct3D device.

Settings use ordinary mouse, keyboard and controller controls. The ring is a passive world display and captures no targeting or movement input. Preferences persist in `[target.rings]` and participate in global defaults, reset and profiles.

For shader compatibility reports, select a living target, run `/xiuiplus rings capture`, and move the camera normally for three seconds. The capture stops automatically (or after 600 frames) and prints the saved JSON path under your Ashita install's `config/XIUIplus/diagnostics/` directory. Send that file from the affected setup; a matching capture from a working setup helps compare the two. `/xiuiplus rings capture stop` finishes early; `/xiuiplus rings capture status` shows recording/saving state or the last result.

Captures include every observed native vertex-shader/FVF binding, raw vertex declaration words, shader-bytecode FNV-1a fingerprints, draw counts, actual camera-check outcomes, depth/blend state, render/depth surface dimensions, sample transforms, frame camera/overlay totals, adapter information and the XIUI+ host build. They record unknown layouts even when another camera check rejects the draw first. Repeated draw states are grouped; storage is capped at 128 bindings and 512 groups, with at most 256 declaration words per binding and explicit dropped/truncated counts. Shader/FVF handles and surface IDs are capture-local; compare declarations/fingerprints across machines. Inspection never changes device state or detector acceptance. Commands queue work for the render thread; only an active capture inspects extra GPU state, JSON/file writing runs on a worker, and unload drains that worker. Reports contain no chat or profile snapshots.

Ring placement uses the native model draw's interpolated root position, which can differ from the entity movement coordinates while walking. `world/entity_render_position.hpp` verifies the actor/entity ownership and the client position getter before reading its vector; unavailable layouts hide the ring. There is no velocity offset or additional position smoothing.

Model sizing reads Ashita's `GetModelHitboxSize`, the native footprint radius already expressed in yalms. It does not use the separate model scale override, which can be unset and does not describe a model's footprint. Zero, non-finite or out-of-range hitboxes fall back to a one-yalm reference radius. A changed hitbox invalidates the cached ring mesh, including when the target identity stays the same. Existing radius preferences are retained; `[target.rings] scale_with_model=false` selects fixed sizing. `/xiuiplus rings status` reports the current native radius and effective scale. This follows the game's hitbox, rather than measuring animated wings or other silhouette details.

Focused checks: `xiui_config_input_smoke --target-rings-only` and `xiui_config_input_smoke --target-ring-state-only`. `/xiuiplus rings status` provides read-only rendering diagnostics.

The camera is captured on actual world draws, and the terrain-clipped vertices are projected directly into scene coordinates each frame, retaining depth and reciprocal W. Near/side/far clipping prevents projection spikes. This avoids inheriting fixed-function transform state from another native draw.

The mesh rebuilds every frame while its target moves. Each rebuild considers only floors within the ring's extent and floor-selection height, and selects the floor from the triangles already beneath each ring piece. `/xiuiplus rings status` reports the target's mesh build count and last/max build time.

Projection applies a slope-aware depth offset, computed in double precision and capped at a two-centimeter approach toward the camera. Depth testing stays enabled and floor geometry and ring XY positions are retained. GPU device setup waits for a render callback instead of running on Ashita's loader thread.

## World Target Bar

World Target Bar is a native Target extension.

Open **Target → World Target Bar → General → Enabled**. The default is a 120-pixel bar with a six-pixel HP track, centered ten pixels below the target's ground position. Shipped defaults enable it; existing installations retain their own enable state. Target must remain enabled.

Layout controls width/height and paired X/Y offsets. Bar Style, Animation, Effects and Colors use the normal meter controls. Colors follows Target's gradient and optional entity colors by default; turn that off for a custom gradient. Optional Name and HP percent each have their own text profile. The world bar needs no fixed screen-position setting: it follows the selected target and camera.

The host reuses its captured world view/projection and viewport, projecting the native target ground coordinates. It validates target identity, health-bearing spawn flags, rendered/hidden flags and zoning state. Invalid, offscreen or behind-camera anchors draw nothing. Native HP percent supplies the fill; no HP maximum is estimated. Target changes clear the previous meter animation. Standard Target visibility rules also hide this overlay.

The bar is a screen overlay anchored in the world. It does not depth-test terrain or other models, so it can remain visible through geometry when the client still renders that entity. Flying/large-model placement can be adjusted with Y offset; it is anchored to the entity's native ground position rather than an estimated mesh bounding box.

Rendering occurs outside Target's content-bounds measurement. Moving the camera or target therefore does not enlarge the normal Target edit frame, change its height or move attached Target of Target/Subtarget windows. The display captures no mouse, keyboard or controller input. Its settings use shared config navigation, including the Target view selector.

Implementation: [host projection](../../src/host/plugin.cpp), [versioned snapshot](../../src/shared/runtime_api.hpp), [settings/rendering](../../src/runtime/modules/target/world_target_bar.cpp) and [Target integration](../../src/runtime/modules/target/target_module.cpp). The development host/runtime pair must be replaced together; the standalone distribution is one DLL.

Focused check: `xiui_config_input_smoke --mob-info-only`. See the [focused checks](../../tests/README.md) for shared settings/defaults and standalone startup validation. Automated checks do not cover in-game appearance or gameplay.

### Healing recipient

[Heal prediction](healing.md#heal-bar-prediction) can temporarily place this bar under the intended party recipient during Cure / Curaga selection and casting, including native subtargets while the hard target remains an enemy. Its preview toggle and shared potency adjustment are under General; preview color is under Colors. The normal target returns when the healing preview ends.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
