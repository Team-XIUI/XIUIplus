# Shared UI foundations

Read the relevant [module note](modules/README.md) for feature ownership. These are cross-module contracts; [development](development.md) owns building, installing and capturing defaults.

## HUD text

Use `module_text_profile` with shared title, meter and caption roles. Render through `module_render_context::DrawText` and `DrawMeter`, expose `TextProfileOption`, and persist through `LoadTextProfile` / `SaveTextProfile`.

- Compact sizing retains normal shadows, stroke, glow, gradients and offsets. Never flatten a profile to make a module smaller.
- Apply game-state colors with `TintTextProfile`; it multiplies colors without losing the profile's effects. Use shared entity/claim helpers.
- Profile visibility controls glyphs and text hit bounds, independently of the surrounding meter/window.
- Use shared meter text layout for labels and values, including native cap insets and explicit placement. Use `ui::FitText` for fitted UTF-8 labels and `design_system::TextHeight` for layout.
- Tabular numbers are a temporary rendering hint, not a font/profile replacement. Measurement, clipping and every effect pass must agree.

Configuration text is separate: `config/config_text` owns its plain presentation and effects. Global typography in `config/global_text` changes only the selected property across HUD profiles; later module edits remain authoritative. Keep new profiles compatible with inherited global settings.

`ui/font_catalog`, `text_rasterizer` and `glyph_atlas` own bundled faces, native-size metrics, Unicode fallback and bounded glyph caches. Flush batches before texture upload/page reuse; release GDI/device resources on shutdown. Do not add per-frame rasterization or duplicate measurement rules.

## Containers

Use `ui::container_renderer` through the render context. It reads the installed native Window Type DATs, caches textures per device and preserves patch geometry, colors and blends. Theme changes select cached skins; rendering performs no file reads or uploads.

The game's Window Type remains authoritative. General's theme control uses the validated native setter; XIUI stores no competing override. Do not reset that client preference when resetting XIUI.

Keep opacity, padding and dark backing independent of module position/size. Tooltips use `DrawTooltip`. To put artwork under a native frame, draw the container without its border, clip the artwork, then use `DrawContainerFrame`. Preserve supported DAT-specific corrections rather than inventing replacement pixels.

## Bars

Use the shared meter renderer, skins, text layout and controls. Native/Modern appearance, gradients, caps, effects and fill direction remain independent preferences. Reuse frame-rate-independent `meter_animation_state`; reset transient interpolation on identity changes. Native cooldown time must not acquire resource-bar smoothing.

## Configuration presentation

`config/config_session.hpp` owns selection, focus, scrolling, search and editors. `hud_runtime_config.cpp`, `hud_runtime_config_input.cpp` and `hud_runtime_config_render.cpp` coordinate pages, pointer actions and drawing. Controller adapters call the same operations and geometry.

- Use `module_option::VisibleWhen` for dependent controls; hidden values persist but leave layout, search and focus. `available` represents capability/action availability.
- Preserve named option IDs and INI keys. Presentation order uses `module_option::Ordered` and `config/option_layout.hpp`.
- Keep dimensions before positions; pair width/height and X/Y rows. Text starts with font and size. Related empty cells must not become focus targets.
- Shared controls own toggles, dropdowns, numeric/text editing, dialogs and resizing. Cancel stale editors before profile, selection or visibility changes.
- Movement adapters own independent HUD elements. Preview changes commit through the same mouse/keyboard/controller actions and roll back on Cancel.

See [configuration](reference/configuration.md) and [settings organization](reference/settings.md) for the current navigation rules.

## Input ownership

Apply [the controller skill](../.agents/skills/xiui-controller-support/SKILL.md) and [input contract](reference/input.md) for interactive changes. Use semantic actions across mouse, keyboard and controller, with deliberate focus and exactly-once activation.

An ordinary native menu must not disable visible pointer controls. Distinguish XIUI's own mouse block from preexisting input blocking. Unclaimed keyboard/controller input stays native; explicitly clicked editors/popups own only their required input. Consume an owned press, repeats and release even when the action closes its surface.

Release/suspend input on foreground loss, hidden/disabled context, zoning, profile/runtime changes and unload. Keep native text entry and subtarget ownership separate from HUD visibility. Presentation preferences must not change native keyboard size, gamepad configuration or Ashita enumeration.

## UI sounds

Use `ui::native_ui_sounds` for native cursor, confirm and menu feedback. Unsupported clients stay silent. Hover does not repeat at rest or while dragging; accepted activation confirms once. Restoration/replay is silent. Chat send feedback follows successful validation/queueing; sound previews do not change preferences or send actions. Embedded clips retain their notices and lifetime-managed playback.

## Settings persistence

Use `settings_store`, normal module load/save hooks and asynchronous writers. Snapshots merge unknown fields/comments and preserve supported encoding; unchanged values avoid rewrites. Keep full snapshots coalesced and retain failed work for retry. Sanitize non-finite values at the shared reader.

Migrate existing defaults narrowly and with a version marker; preserve later customizations. Layout fitting uses the preset's `layout.viewport` and the actual game viewport, preserving moved modules and deferring startup placement until that viewport is valid. Keep the reference resolution with profile positions. Portable profiles use the existing preference visitor, excluding session state, character-local inventory layouts and controller-device data. Do not modify the immutable sharing preset. Profile/dialog operations must drain writers and cancel transient input at their existing lifecycle boundary.

## Shipped defaults and resources

Capture defaults only when requested, using [the maintained workflow](development.md#capture-current-defaults). Ordinary installation preserves the active INI/theme, positions and profiles. Missing user files use embedded defaults; explicit Reset restores the complete captured profile through normal loading/saving.

Keep optional artwork lazy, caches bounded and decode work off the render path. Edit asset generators/source inputs, not generated tables. Share one immutable combat-action decode and timestamp across consumers. Per-frame rendering must not repeat filesystem access, native discovery, or avoidable expensive geometry work.

[Focused checks](../tests/README.md) should cover the boundary changed. Synthetic checks do not establish gameplay or physical-device acceptance.
