# Shared UI foundations

Read the relevant [module note](modules/README.md) for feature ownership. These are cross-module contracts; [development](development.md) owns building, installing and capturing defaults.

## HUD text

Use `module_text_profile` with shared title, meter and caption roles. Render through `module_render_context::DrawText` and `DrawMeter`, expose `TextProfileOption`, and persist through `LoadTextProfile` / `SaveTextProfile`.

- Compact sizing retains normal shadows, stroke, glow, gradients and offsets. Never flatten a profile to make a module smaller.
- Scoped edge fading changes vertex alpha for text effects and textures, splitting quads at fade boundaries so content below the edge retains full opacity. Keep container drawing outside the text fade scope.
- Apply game-state colors with `TintTextProfile`; it multiplies colors without losing the profile's effects. Use shared entity/claim helpers.
- Profile visibility controls glyphs and text hit bounds, independently of the surrounding meter/window.
- Use shared meter text layout for labels and values, including native cap insets and explicit placement. Use `ui::FitText` for fitted UTF-8 labels and `design_system::TextHeight` for layout.
- Native meter values reserve six pixels at the right edge of their text area; labels retain their independent inset. XIV Player/Party resources use ten-pixel label and fourteen-pixel value edge insets through their shared layout, including number-only Party TP.
- Tabular numbers are a temporary rendering hint, not a font/profile replacement. Measurement, clipping and every effect pass must agree.
- Saved text styles carry `weight_choice` (Default, Regular or Bold) and `italic`. `text_style::weight` remains a temporary code-owned run style, such as chat emphasis, that takes precedence. Draw, measure and cache through `text_style::Variant()`; Default keeps the face's tuned weight.
- Medium requests the native 500 face. If a static family maps it to Regular, the rasterizer strengthens supersampled coverage before downsampling and adjusts advances; the result stays cached independently of Regular and Bold.

Configuration text is separate: `config/config_text` owns its plain presentation and effects. Global typography in `config/global_text` changes only the selected property across HUD profiles; later module edits remain authoritative. Keep new profiles compatible with inherited global settings. Theme fonts are defaults: use `text_style::ThemeFont` for theme-specific names, labels and numbers so configured faces survive drawing and measurement. Font controls record `font_explicit`, including selecting Bahnschrift at index zero, and shared text persistence retains it. Existing nonzero font selections remain authoritative without migration.

`ui/font_catalog` lists the seven XIUI+ faces first, then installed TrueType/OpenType families in name order. Built-in faces save their index. Other fonts save `font_family` through `ReadFontChoice`/`WriteFontChoice`, with `font` set to 0 for older builds, so choices survive font installs and shared profiles. A saved family that is not installed keeps its name and draws with Bahnschrift. Runtime font indices are session-local; never persist them for non-built-in fonts. Installed fonts report Bahnschrift's line height and ascent at the same size, and the rasterizer moves their glyphs onto that baseline with enough padding for tall accents and descenders. Layouts tuned to the XIUI+ faces therefore keep their rows when a family such as Noto Sans has much taller vertical metrics. Symbol-charset fonts are not listed.

Shared buttons reveal overflowing labels with a clipped bounce marquee on hover or deliberate controller focus, pausing at each end. Idle labels retain UTF-8-safe ellipses; fitting labels stay still. Keep selection tint separate from marquee activation.

`ui/font_catalog`, `text_rasterizer` and `glyph_atlas` own bundled faces, native-size metrics, Unicode fallback and bounded glyph caches. Flush batches before texture upload/page reuse; release GDI/device resources on shutdown. Do not add per-frame rasterization or duplicate measurement rules.

## Localization

Every new feature and every change to player-facing XIUI+ copy must include English and Japanese catalog entries in the same change. This includes HUD captions, settings labels/descriptions/choices, tooltips, dialogs, errors, placeholders and keyboard/controller help. Follow the [localization contract](../locales/README.md); English fallback protects compatibility and is not a substitute for translating new copy.

Use the shared localized configuration controls or resolve explicit XIUI+ messages with `i18n::Text` / `i18n::Format` before measurement, fitting, wrapping and drawing. Translate complete format templates before inserting values. Keep stable option IDs, INI keys, command syntax and selection logic independent of translated text. Preserve incoming chat, client resources, names and user-authored content verbatim.

Update both UTF-8 catalogs and run `python scripts/build-locales.py` when adding or editing localized text. Preserve the active language across profiles, presets and resets, and keep translated settings discoverable through the shared search path. Use existing Unicode font fallback and UTF-8-aware layout helpers.

## Containers

Use `ui::container_renderer` through the render context. It reads the installed native Window Type DATs, caches textures per device and preserves patch geometry, colors and blends. Theme changes select cached skins; rendering performs no file reads or uploads.

For XI Theme, Global → Appearance → Window places **Follow game** directly below **Window Type**, enabled by default. Following uses the validated native setting/setter. Turning it off selects XIUI+'s shared container and bar skin independently, with no native writes; the first independent selection starts from the current type and is remembered while following is reenabled. Both preferences participate in profiles. The separate Theme control selects XI Theme or XIV 1.0 Theme without changing the client preference. XIV 1.0 uses its own embedded artwork. Resetting XIUI+ restores Follow game without resetting the game's Window Type.

Keep opacity, padding and dark backing independent of module position/size. Tooltips use `DrawTooltip`. To put artwork under a native frame, draw the container without its border, clip the artwork, then use `DrawContainerFrame`. Preserve supported DAT-specific corrections rather than inventing replacement pixels.

XIV 1.0 tooltips add a subtle dark interior fill for readability, independently of ordinary window styling and HUD text.

XIV 1.0 window backgrounds default to 40% through Global → Appearance → Window. This adjustable theme opacity combines with each module's existing opacity and leaves frame/text settings intact. XI Theme keeps its original opacity behavior.

## Bars

Use the shared meter renderer, skins, text layout and controls. Native/Modern appearance, gradients, caps, effects and fill direction remain independent preferences. Reuse frame-rate-independent `meter_animation_state`; reset transient interpolation on identity changes. Native cooldown time must not acquire resource-bar smoothing.

Window-theme meter bodies use the fill artwork across the full authored height. Do not reintroduce the native gauge's top/bottom frame strips or vertical fill insets; optional side bookends retain their own geometry. Fills and resource overlays share the same full-height content bounds.

XIV 1.0 gauges have authored copper rails and caps; their content inset comes from `LayoutMeterSkin`. All fills, predictions and feedback use that same content geometry. Theme typography resolves through shared draw/measurement paths without changing saved profiles. Configuration text opts out with `theme_text = false` through shared controls.

XIV 1.0 meter values use the shared lining-number face, avoiding Georgia's old-style digit ascenders and descenders. Player and Party use `ui/xiv1_resource_layout.hpp` for taller HP/MP tracks and text centered on the lower rim. Preserve shared measurement and hit geometry when adjusting overlap.

## Configuration presentation

`config/config_session.hpp` owns selection, focus, scrolling, search and editors. `hud/hud_runtime_config.cpp`, `hud/hud_runtime_config_input.cpp` and `hud/hud_runtime_config_render.cpp` coordinate pages, pointer actions and drawing. Controller adapters call the same operations and geometry.

- Use `module_option::VisibleWhen` for dependent controls; hidden values persist but leave layout, search and focus. `available` represents capability/action availability.
- Preserve named option IDs and INI keys. Presentation order uses `module_option::Ordered` and `config/option_layout.hpp`.
- Use `config/settings_topics.hpp` for the shared Appearance hierarchy and visible topic paths. Use the shared sidebar rows for animated module destinations; drawing, hit targets, controller focus and scrolling must use those rows together.
- Keep dimensions before positions; pair width/height and X/Y rows. Text starts with font and size. Related empty cells must not become focus targets.
- Shared controls own toggles, dropdowns, numeric/text editing, dialogs and resizing. Cancel stale editors before profile, selection or visibility changes.
- Movement adapters own independent HUD elements. Preview changes commit through the same mouse/keyboard/controller actions and roll back on Cancel.

See [configuration](reference/configuration.md) and [settings organization](reference/settings.md) for the current navigation rules.

## HUD scale

[HUD scaling](reference/hud-scaling.md) owns Basic/Advanced presentation, complete captured HUD presets, element transforms and edit grips. Keep authored dimensions/positions separate from resolved screen bounds. Every HUD primitive uses the shared scoped transform; pointer entry points invert it once and screen-space overlays map their anchors once. Apply global scale once, retain text effects, and serialize committed values while gestures are pending.

Shared module controls append scale options after existing option IDs. Preserve each independent element's stable scale identity, including disabled and inactive children. Configuration, native windows and world-space target rings keep their own sizing.

## Input ownership

Follow [controller support](reference/controller-support.md) and the [input contract](reference/input.md) for interactive changes. Use semantic actions across mouse, keyboard and controller, with deliberate focus and exactly-once activation.

An ordinary native menu must not disable visible pointer controls. Distinguish XIUI+'s own mouse block from preexisting input blocking. Unclaimed keyboard/controller input stays native; explicitly clicked editors/popups own only their required input. Consume an owned press, repeats and release even when the action closes its surface.

Release/suspend input on foreground loss, hidden/disabled context, zoning, profile/runtime changes and unload. Keep native text entry and subtarget ownership separate from HUD visibility. Presentation preferences must not change native keyboard size, gamepad configuration or Ashita enumeration.

## UI sounds

Use `ui::native_ui_sounds` for native cursor, confirm and menu feedback. Unsupported clients stay silent. Hover does not repeat at rest or while dragging; accepted activation confirms once. Restoration/replay is silent. Chat send feedback follows successful validation/queueing; sound previews do not change preferences or send actions. Embedded clips retain their notices and lifetime-managed playback.

## Settings persistence

Installed data belongs under `config/XIUIplus/`, resolved by `settings/storage_paths.*`. Migrate paths before starting writers; preserve current destination files and archive conflicting legacy files. Derive profile/resource directories from the settings file's parent rather than recreating top-level plugin folders.

Use `settings_store`, normal module load/save hooks and asynchronous writers. Snapshots merge unknown fields/comments and preserve supported encoding; unchanged values avoid rewrites. Keep full snapshots coalesced and retain failed work for retry. Sanitize non-finite values at the shared reader.

Migrate existing defaults narrowly and with a version marker; preserve later customizations. Layout fitting uses the preset's `layout.viewport` and the actual game viewport, preserving moved modules and deferring startup placement until that viewport is valid. Retain the complete indexed settings store during that handoff; serialized pending writes do not include disk-loaded values. Keep the reference resolution with profile positions. Portable profiles use the existing preference visitor, excluding session state, character-local inventory layouts and controller-device data. Do not modify the immutable sharing preset. Profile/dialog operations must drain writers and cancel transient input at their existing lifecycle boundary.

## Shipped defaults and resources

Capture defaults only when requested, using [the maintained workflow](development.md#capture-current-defaults). Ordinary installation preserves the active INI/theme, positions and profiles. Missing user files use embedded defaults; explicit Reset restores the complete captured profile through normal loading/saving.

Keep optional artwork lazy, caches bounded and decode work off the render path. Edit asset generators/source inputs, not generated tables. Share one immutable combat-action decode and timestamp across consumers. Per-frame rendering must not repeat filesystem access, native discovery, or avoidable expensive geometry work.

[Focused checks](../tests/README.md) should cover the boundary changed. Synthetic checks do not establish gameplay or physical-device acceptance.
