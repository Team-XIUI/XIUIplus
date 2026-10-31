# Basic settings and HUD scaling

Status: implemented and installed October 20, 2026.

## Outcome

XIUIN opens with a small Basic Settings page for new users: **Classic**, **XIV**, **Global Scale**, **Edit Layout**, and an **Advanced Settings** switch. Advanced retains the existing configuration and adds scale controls for HUD elements. Edit mode supplies a corner grip to scale enabled elements horizontally and vertically.

Rename the visible **General** category to **Global**. Keep the internal `general` module/page ID, existing settings keys, option IDs, command destinations, and saved navigation compatible.

This is the accepted implementation plan and delivery record. The [current usage and implementation guide](../reference/hud-scaling.md) documents the delivered behavior.

## Evidence and existing foundations

- AgentLink answered `?state`, `?loaded xiuinative`, `/xiui status`, and `?modules`. The loaded executable, Ashita, AgentLink, and XIUIN paths resolve to `C:/Games/PhoenixXI-dev/PhoenixXI`; XIUIN is loaded from `plugins/xiuinative.dll`. The character was Shuu, with edit mode disabled. No settings were changed during inspection.
- [Party presets](../reference/party-presets.md) already provide Classic and XIV, including captured assets in `config/party-classic.ini` and `config/party-xiv.ini`. Their current action applies to Party and both Alliance lists. Whole-HUD Classic/XIV presets still need to be authored.
- [Shared foundations](../foundations.md) own text effects, containers, meters, persistence, and input. Preserve these contracts throughout scaling.
- Configuration already shares layout/control geometry between mouse and controller. Movement already has adapters for hotbars, world-info parts, notification groups, detached target children, and mob-info groups.
- Current edit frames, pointer capture, drag selection, cursor selection, and movement bounds use several different bounds paths. Scaling must consolidate their use of resolved geometry.
- Rendering has passes outside the main module loop: chat bubbles, the world target bar, chat history, Satchel, tooltips, controller focus, and floating Action Bars/other overlays. A transform around the ordinary render loop alone is incomplete.
- `ClampLayout` currently clamps stored dimensions and positions. Introducing scaled bounds there without separating authored and displayed geometry would permanently move or shrink layouts.
- Phantom Roll already has a persisted **Content scale**. Preserve that existing layout preference and compose the new outer scale around its output; label/help must distinguish the two.

Implementation owners:

- Shared geometry/rendering: [module.hpp](../../src/runtime/modules/module.hpp), [module.cpp](../../src/runtime/modules/module.cpp), [design_system.hpp](../../src/runtime/design_system.hpp), [design_system.cpp](../../src/runtime/design_system.cpp), and `src/runtime/ui/`.
- Element selection, edit gestures, bounds, and special render passes: [hud_runtime.cpp](../../src/runtime/hud_runtime.cpp), [hud_runtime_layout.cpp](../../src/runtime/hud_runtime_layout.cpp), and [hud_runtime_controller_movement.cpp](../../src/runtime/hud_runtime_controller_movement.cpp).
- Settings modes and controls: [config_session.hpp](../../src/runtime/config/config_session.hpp), `hud_runtime_config*.cpp`, [hud_runtime_controller_config.cpp](../../src/runtime/hud_runtime_controller_config.cpp), [hud_runtime_search.cpp](../../src/runtime/hud_runtime_search.cpp), and [general_module.hpp](../../src/runtime/modules/general/general_module.hpp).
- Preferences/presets: [settings_store.cpp](../../src/runtime/settings/settings_store.cpp), [settings_profiles.hpp](../../src/runtime/settings/settings_profiles.hpp), [layout_viewport.hpp](../../src/runtime/settings/layout_viewport.hpp), [hud_runtime_profiles.cpp](../../src/runtime/hud_runtime_profiles.cpp), and [party_module.cpp](../../src/runtime/modules/party/party_module.cpp).

## Product decisions

### Basic and Advanced

1. Basic has two large preset buttons, one percentage slider for Global Scale, Edit Layout, and the mode switch. Keep the existing profile utilities accessible.
2. Basic and Advanced edit the same active profile. Switching modes changes presentation only; it never applies a preset or resets settings.
3. New installations start in Basic. Existing installations keep Advanced until the user switches. Persist the choice as a local configuration preference, excluded from portable profiles.
4. Advanced retains existing pages, navigation, search, and all detailed controls. Global Scale appears near the top of Global and edits the same value as Basic.
5. Advanced search remains in Advanced; Ctrl+F from Basic opens Advanced search explicitly. Returning to Basic restores its small page. Preserve the last Advanced selection and scroll where still valid.
6. Mode changes settle/cancel the relevant shared editors and gestures and rebuild focus before hiding controls. Hidden Advanced controls cannot remain input targets in Basic.

### Classic and XIV

- **Classic:** FFXI-style presentation using existing native window/meter foundations and the captured Classic Party/Alliance layout.
- **XIV:** the captured XIV Party/Alliance presentation plus a coherent selection of existing modern meter, typography, spacing, and icon treatments elsewhere.
- Treat these as explicit appearance presets. Preserve module enable states, screen positions, global/local scale, actions/macros, keybindings, chat filters/channels, native HUD visibility, device preferences, character bindings, and character-local inventory layouts.
- A preset may change visual dimensions, padding, text styles, meter appearance, icon presentation, and internal arrangement. Describe that scope in its existing shared confirmation dialog, with Cancel selected initially.
- Build a complete module coverage manifest: every visual module is either explicitly styled or intentionally shares the same presentation in both presets. Include subordinate views and pet types. Do not label the Party-only preset as a finished global preset.
- Reuse Party's captured preference data with a presentation-only overlay. Its current `ApplyPreset` also assigns enable/preview preferences, so invoking that action unchanged is insufficient for the global preserve-enablement contract.
- Author new embedded preset fragments from reviewed settings and existing assets. Do not capture the live character's configuration or reinterpret current mixed defaults as Classic. Leave the immutable XIUI2 sharing baseline intact.
- Apply a validated preset as one settings transaction through normal module hooks and persistence. Preserve the active profile identity, drain pending writers, and close stale interaction state. The two buttons are actions; do not claim a preset is still selected after arbitrary Advanced edits unless its owned fields actually match.

### Global Scale

- Default 100%; initial range **50–200%**, in 1% steps, with a reset to 100% and direct numeric entry through existing controls.
- Scale all XIUIN HUD presentation: containers, bars, text and effects, icons, padding, spacing, borders, and corresponding interactive bounds. Disabled elements retain their local scale and adopt the current global value when enabled.
- Leave configuration, edit handles, cursor, management dialogs, and tooltip typography at their existing configuration size. Their anchors and focus outlines must follow the scaled HUD geometry.
- Native FFXI windows and world-space gameplay geometry, such as target rings, are outside HUD scaling. World-anchored XIUIN labels/bars/bubbles scale in size around their projected anchor; never scale the projected screen position.
- Keep saved screen positions as placement intent. Scaling changes the element's size about its placement origin, with a derived on-screen offset when needed to keep it reachable. Returning to the original scale restores placement without accumulated drift.
- Existing attachment/stack rules remain authoritative. Resolve attached offsets and chat-stack spacing with the effective scale rather than treating every attached item as an unrelated window.

### Element scale and edit grips

- Each independent HUD element has **Scale X** and **Scale Y**, default 100%, initially **25–300%**, with 1% steps and Reset Scale. Width and height remain existing layout controls; outer scale does not rewrite them.
- Present these as one **Scale** group in the module's Layout/Window settings, using the same resolved element as movement selection. Action-bar instances, both chat windows, Alliance lists, inventory dots, notification groups, and independent world-info/target parts retain distinct values.
- Include **Lock proportions** beside the pair. Default unlocked to allow the requested horizontal/vertical adjustment; when locked, use the drag-start ratio for both pointer and directional adjustments. Unequal X/Y scales intentionally stretch the whole element, including text and icons.
- While editing, show a small lower-right grip on each enabled, available independent element's preview. The outline follows stable layout bounds; neither animation nor changing live text can move the grip during a drag.
- A grip wins hit testing over the element's move surface. Dragging the body still moves it. Clicking the grip selects that element and previews scale from the original pointer/bounds snapshot. Keep the opposite corner fixed unless viewport reachability requires a derived offset.
- Show live percentages. Release commits once; Escape/Cancel restores the original scale and placement. Leaving edit mode normally commits a completed gesture. Foreground loss, zoning, disabling/removing the element, profile changes, and runtime replacement cancel an unfinished gesture and release input ownership.
- An enabled conditional module gets its existing edit preview even when live data is unavailable. Add a deterministic preview where one is missing; do not fabricate live game state.
- Attached visual parts normally scale with their owning element. Keep their existing attached/independent behavior; detaching exposes the part's independent scale and grip, and reattaching preserves that saved independent value. Settings explain the inherited scale while attached. Ordinary bars/text within a module remain its detailed layout controls.
- World-anchored presentation gets a stationary edit-preview sample with a scale grip and numeric settings. It keeps its world attachment and gains no fake movable gameplay position. Categories/services with no rendered element do not get a meaningless Scale control.

## Architecture

### Shared element geometry

Introduce a small shared scale/geometry foundation next to `module_state` and the existing layout helpers. Each real layout element exposes its stable ID, persisted layout section, local X/Y scale, layout/placement bounds, attachment owner, and capabilities. Reuse the current movement adapters; extend their contract rather than deriving ownership from display strings or maintaining separate pointer/controller lists.

For a root element, effective scale is:

```text
effective_x = global_scale * element_scale_x
effective_y = global_scale * element_scale_y
screen_point = resolved_origin + effective_scale * (layout_point - placement_origin)
```

The inverse is the single route from screen coordinates to element coordinates. Attached content inherits the owner's resolved transform. A detached child resolves as its own root. Apply global scale exactly once.

Provide explicit operations for points, rectangles, vectors, forward/inverse mapping, and screen bounds. Distinguish authored coordinates from displayed coordinates at API boundaries. Never pass a mixture of screen pointer positions and unscaled rectangles into a hit test.

Keep three bounds concepts separate:

- Stable layout/interaction bounds for scaling, snapping, clamping, and grip placement.
- Painted content bounds, including normal text effects, for visual outlines and overflow.
- Screen-space overlay bounds for configuration, popups, tooltips, and viewport constraints.

Cache resolved geometry before input routing and refresh it when layout, scale, viewport, or conditional visibility changes. A stale preceding-frame unscaled rectangle cannot decide capture after a scale change. Capture the drag-start geometry so content updates do not change the scale denominator.

### Rendering and text

- Add a scoped transform to the shared design/render foundation. Restore it on every exit path, including exceptions; end-frame state must be clean. Every primitive path must honor the same mapping, including batched geometry, texture quads, circles, contours, meter effects, glyphs, and software clipping.
- Preserve the existing module render context and native DAT/meter/text renderers. Direct `context.Design()` drawing must participate too. Do not render a module into a bitmap and stretch that bitmap.
- Define measurement and clipping in element coordinates, transform output exactly once, and convert screen viewport constraints through the inverse mapping. Audit returns such as container inner bounds and text bounds to keep their coordinate contract consistent.
- Glyph measurement, wrapping, alignment, and effect extents must agree with drawing. Use the existing glyph cache at an appropriate effective raster size and normalize its metrics back into layout units; do not permanently multiply text profiles or repeatedly rasterize during a drag. Retain bounded caches and all shadow/stroke/glow/gradient passes.
- Check combined global/local sizes against renderer and glyph-atlas limits before fixing the public ranges. Support the documented range consistently for all primitives and text; avoid separate silent font caps or unbounded cache growth at high scale.
- Wrap every HUD render pass with its actual element owner. In particular, expanded chat and floating windows must use the same transform for their layout and input. Floating management dialogs can retain configuration sizing while their HUD anchors resolve to screen space.
- For screen-anchored overlays, map the anchor once and render in the overlay's defined coordinate space. Focus frames use scaled bounds but retain a readable screen-space stroke.

### Input and gestures

- Use the resolved element geometry for hover, clicks, wheel/scroll regions, drag/drop, status removal, slot selection, tooltip anchors, focus frames, and all native suppression decisions. Convert pointer coordinates exactly once at the owning module boundary; map returned rectangles back when the caller expects screen coordinates.
- Add a shared scale gesture with begin/update/commit/cancel operations. Mouse grips, module numeric controls, and keyboard/controller adjustment call those operations.
- Extend the existing Movement utility to **Move / Scale** for the selected element. Directions adjust X/Y scale, Actions switches fine/coarse increments, Confirm commits once, and Cancel restores. Keyboard uses the same focused utility and operations. Keep explicit focus, help, scrolling, and stable IDs.
- Preserve existing locked-chat resizing as a separate reflow action. In edit mode, the new grip owns whole-element scaling; do not let one press also begin chat reflow or module movement.
- Preserve native keyboard/controller ownership and the existing press/held/release lifetime. Reuse suspension/lifecycle boundaries; no synthetic clicks, injected Enter/Escape, new device settings, or temporary diagnostic input handlers are needed.

### Persistence and migration

- Store global scale in a new portable HUD appearance section, and local X/Y scales in dedicated `hud.scale.<stable-element-id>` sections so existing layout keys stay compatible. Keep values at 1.0 when keys are absent. Persist proportion-lock with the element.
- Load/save scale through the element's real owner, including inactive pet views or subordinate elements that ordinary rendering does not visit. Shared state/proxy modules must not overwrite each other's scale. Unknown fields remain preserved.
- Store Basic/Advanced under an explicitly local `session.*` section so the existing preference visitor excludes it. Detect existing installations before introducing the default mode. Profile activation must preserve this local preference.
- Normal capture/export includes committed HUD scales. Like existing controller move/resize snapshots, capture must serialize the original committed values while a scale gesture is pending. An unrelated autosave must not persist a canceled preview.
- Keep original scale values through preset application and element enable/disable. Reset Scale resets local scale only; Global Scale's reset changes global only. The existing full reset retains its normal full-reset scope.
- Update default viewport fitting and `ClampLayout` to operate on resolved bounds without destructive rescaling of stored dimensions. First-load placement must use effective element size; loading the same profile repeatedly or changing scale back and forth must not compound changes.
- Do not change existing option indices to insert scale controls. Add stable IDs through the shared adapter/control path, and include them in search, navigation, presentation inventory, and profile operations.

## Implementation order

### 1. Define element ownership, preset scope, and storage

Inventory real render/input passes and independent elements. Specify their layout sections, attachment ownership, preview support, and overlay behavior. Complete the Classic/XIV presentation-field manifest and embedded fragments. Add shared scale values and geometry operations with compatibility defaults. No automatic preset application.

Exit: every visual module has a defined scale owner and preset policy; legacy settings resolve to 100% with unchanged appearance.

### 2. Implement global rendering and input geometry together

Add scoped shared rendering, effective text metrics, clipping, and inverse pointer routing. Convert bounds, focus, viewport fitting, and clamping. Integrate ordinary modules first, then the special render/input passes before exposing Global Scale publicly.

Exit: Global Scale affects every HUD element once; interactive regions and returned anchors agree with the painted output; scale round trips do not alter authored layout.

### 3. Add element controls and edit gestures

Expose Scale X/Y, lock, and reset using shared controls and movement adapters. Add grips, live preview, stable drag bounds, rollback, and Move/Scale keyboard/controller routes. Cover independent children, conditional previews, world-anchored samples, and existing chat reflow precedence.

Exit: every enabled supported visual element is reachable for scaling; cancel and lifecycle interruption restore committed state; adjustments persist through normal profile save/load.

### 4. Add Basic/Advanced and Global presentation

Implement the compact Basic page, shared Global Scale, Edit Layout, mode persistence, advanced navigation restoration, search transitions, and the visible Global rename. Use existing native controls, UI sounds, and controller focus operations.

Exit: switching modes preserves settings; hidden controls cannot retain focus/capture; existing saved navigation still resolves.

### 5. Wire the two complete presets and deliver

Apply the authored preset fragments through the existing confirmation/transaction infrastructure. Regenerate affected settings documentation from metadata. Update the foundations, configuration guide, General/Global module note, and relevant module notes for the new geometry contract.

Build/package once the implementation is ready, install the standalone DLL into the AgentLink-confirmed active install, and confirm loaded/status after reload. Preserve current settings and positions. Leave visual/gameplay review to Cody.

## Focused acceptance

Use small deterministic checks for the shared boundaries, with representative modules rather than a broad visual sweep:

- Identity transform at 100%; forward/inverse round trips; unequal X/Y scaling; global/local composition; attached versus detached ownership; clipping and content bounds. Include one text-effect path and one textured/batched primitive path.
- A display module plus interactive Chat, Action Bars, and Satchel exercise hit regions, wheel/scroll, anchors, and overlay behavior at non-100% scale. A click outside the painted region must retain native input ownership.
- Grip priority over move/chat reflow; stable drag snapshots; one save on commit; no preview leakage into unrelated autosaves; Cancel, disable, profile switch, and foreground loss restore committed state.
- Save/load/profile-share round trip, missing scale keys, non-finite/range sanitization, inactive subordinate settings, local mode exclusion, and no drift after 100% → 150% → 100%.
- Basic/Advanced focus and search transitions use shared mouse/controller operations. The Global rename retains IDs and old navigation. Presets preserve functional settings, enable states, positions, and scales while applying every owned presentation field.

Extend the relevant existing focused executable/catalog entries for these new boundaries. Run only the applicable focused checks and the two required standalone packaging checks. AgentLink verifies installation and readiness; it does not establish physical-controller, visual, or gameplay acceptance.

## Completion criteria

The work is complete when Basic and Advanced share one settings model; both presets cover the whole HUD; Global Scale reaches all XIUIN HUD passes; enabled elements offer usable scaling controls/previews; pointer/controller geometry and persistence agree; and the standalone build is installed and confirmed through AgentLink. Preserve the existing unrelated work already present in this checkout.

## Delivery record

October 20 Basic refinements: Global Scale now uses one compact row with a percentage field and aligned Reset button. Removed the redundant Edit Layout section, preset descriptions and Profile label. Classic/XIV samples reuse isolated party-entry renderers, matching typography and values while showing their compact/three-bar layouts. The Advanced button moved to the footer, and Basic no longer draws or captures the Advanced search control. Focused input/layout/hover checks and standalone packaging passed; AgentLink confirmed the new DLL loaded and edit mode reopened with settings preserved. Installed SHA-256: `D86707B8275D4BFEEB901500F20F62F9419D50AFA9CC9C13EFDB9B6C6A71F6B5`.

October 20 Basic redesign: replaced stretched option rows with preset samples and native Apply buttons, an aligned scale/reset row, a layout action and a profile footer. Shared geometry fits minimum and normal window sizes. Basic controller navigation excludes hidden Advanced controls and supports percentage entry. Configuration tooltips reset before both settings views; modal overlays and focus loss suppress underlying help. Focused layout/input/hover checks and standalone packaging passed. AgentLink confirmed the standalone replacement loaded in the active Phoenix install with saved preferences retained. Installed SHA-256: `AB8B23E8D33234B84EE9F3EEF283FA6271FAEA271BC12B7320ECA914CDA3549F`.

October 20 bar-height correction: both appearance presets and fresh/reset defaults use explicit 12px XP and player HP/MP/TP tracks. Window height no longer selects track thickness. A narrow versioned migration corrects the old 80px XP and matching 28px player tracks; incoming profiles retain their own migration version. Focused scaling/defaults and standalone packaging checks passed. AgentLink confirmed the replacement loaded in the active Phoenix install; the completed save contained 12px player tracks and retained the custom 14px XP track. Installed SHA-256: `D7873272284ECDBB36B6CDC3376BB4EF2E7E485350C054F63328607784E8D1B1`.

- Implemented Basic/Advanced, Global naming, the two presentation-scoped presets, shared global/local transforms, edit grips, proportion locking, keyboard/controller scale adjustment, world-anchored preview samples and committed-value persistence. Preset specifications reuse the embedded Party captures and shared theme roles; the explicit field policy lives in `src/runtime/settings/hud_presets.hpp`.
- Passed the focused `hud-scaling` check, defaults compatibility check, generated settings presentation checks, and documentation links. The scale check includes inverse input geometry, interrupted/pending gestures, stable grips, Basic/Advanced state, nonvisual Action Bars views, preset preservation and scale save/load.
- Packaged version 1.0.6 as one `dist/xiuinative.dll`; both required standalone resource/startup checks passed. The version was not bumped.
- AgentLink completed `/xiui save sync`, confirmed unload, loaded the replacement standalone DLL in `C:/Games/PhoenixXI-dev/PhoenixXI/plugins`, and separately confirmed loaded/status. Runtime reported `Oct 20 2026 13:17:54`, HUD visible and edit mode disabled. Packaged and installed SHA-256 values matched.
- Existing settings, profiles, positions and unrelated checkout changes were retained. No appearance preset was applied to the live character. Visual/gameplay review and physical-controller acceptance remain with Cody.

October 20 refinement: scaling now defaults to locked proportions and one Scale control. Font measurement and all effect passes use the final native pixel size with screen-grid placement. Larger glyph pages stay within the existing texture budget; XIUI icons select higher-resolution masks and vector antialias edges remain one screen pixel. World target rings independently follow the native model hitbox radius, with fixed yalm sizing available. Focused HUD scaling, text rasterization, target rings, defaults and generated settings checks passed. Standalone packaging checks passed; AgentLink confirmed the replacement active with runtime `Oct 20 2026 17:54:09`. Installed and packaged SHA-256 matched `6173135B739C17D93026DBDF01BACE9CBCB2063C4994709C4C3F652E656DA297`.

Chat/grip follow-up: Chat now reuses its existing top-right handle for viewport resizing, preserving fonts and the bottom-left anchor. Its earlier unequal scales convert to width/height, and whole-chat scaling stays uniform. Other edit scale grips moved to the top right, with shared hover feedback and full hit regions for thin elements. Focused resizing, migration, hover deduplication, defaults and settings-presentation checks passed. AgentLink confirmed the installed runtime `Oct 20 2026 18:03:42`; saved Chat 2 scale is uniform at 100%. Installed/package SHA-256: `4167F8811D22ACA9023063CCA4DB6D88FBA2B8D2D73DC9C1C70080AE473EAE41`.
