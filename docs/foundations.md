# Foundations

Rules shared by every HUD module. Read the relevant [module note](modules/README.md) for a feature's own files; building and source layout are in the [developer docs](README.md).

## HUD text

Use `module_text_profile` with the shared title, meter and caption roles. Draw through `module_render_context::DrawText` and `DrawMeter`, expose `TextProfileOption`, and persist with `LoadTextProfile` / `SaveTextProfile`.

- Compact sizing keeps normal shadows, stroke, glow, gradients and offsets. Never flatten a profile to make a module smaller.
- Apply game-state colors with `TintTextProfile`; it multiplies colors without losing the profile's effects. Use the shared entity and claim color helpers.
- A profile's visibility controls its glyphs and text hit bounds, independently of the surrounding meter or window.
- Use the shared meter text layout for labels and values, `ui::FitText` for fitted UTF-8 labels and `design_system::TextHeight` for layout.
- Edge fading changes vertex alpha for text and textures. Keep container drawing outside the text fade scope.
- Tabular numbers are a rendering hint, not a font or profile replacement. Measurement, clipping and every effect pass must agree.
- Saved text styles carry `weight_choice` (Default, Regular or Bold) and `italic`. Draw, measure and cache through `text_style::Variant()`.

Fonts:

- Theme fonts are defaults. Use `text_style::ThemeFont` for theme-specific names, labels and numbers so a font the player chose survives drawing and measurement.
- Built-in faces save their index. Installed fonts save `font_family` through `ReadFontChoice` / `WriteFontChoice`; never persist a runtime font index for them. A saved family that is not installed keeps its name and draws with Bahnschrift.
- `ui/font_catalog`, `text_rasterizer` and `glyph_atlas` own bundled faces, metrics, Unicode fallback and the glyph caches. Do not add per-frame rasterization or duplicate measurement rules.

Configuration text is separate: `config/config_text` owns its plain presentation. Global typography in `config/global_text` changes only the selected property across HUD profiles; later module edits stay authoritative.

## Localization

Every new or changed piece of player-facing text needs English and Japanese catalog entries in the same change: HUD captions, settings labels, descriptions, choices, tooltips, dialogs, errors, placeholders and controller help. English fallback is not a substitute for a translation. Follow the [localization guide](../locales/README.md).

- Resolve text with `i18n::Text` / `i18n::Format` before measuring, fitting, wrapping or drawing. Translate complete format templates before inserting values.
- Keep option IDs, INI keys, command syntax and selection logic independent of translated text.
- Leave incoming chat, client resources, names and user-written content untouched.
- Run `python scripts/build-locales.py` after editing localized text.

## Containers

Use `ui::container_renderer` through the render context. It reads the game's Window Type skins, caches textures per device and keeps patch geometry, colors and blends. Rendering performs no file reads or uploads.

- Keep opacity, padding and dark backing independent of a module's position and size.
- Tooltips use `DrawTooltip`.
- To put artwork under a native frame, draw the container without its border, clip the artwork, then call `DrawContainerFrame`.
- XI Theme can follow the game's Window Type or use its own; XIV 1.0 Theme uses embedded artwork. Neither writes to the game's setting unless Follow game is on.

## Bars

Use the shared meter renderer, skins, text layout and controls.

- Appearance, gradients, caps, effects and fill direction are independent preferences.
- Reuse the frame-rate-independent `meter_animation_state`, and reset interpolation when the tracked identity changes. Native cooldown time must not be smoothed.
- Fills, predictions, overlays and feedback share the same content bounds. For XIV 1.0 gauges that inset comes from `LayoutMeterSkin`.
- XIV 1.0 Player and Party resources use `ui/xiv1_resource_layout.hpp`. Keep shared measurement and hit geometry when adjusting overlap.

## Settings window

`config/config_session.hpp` owns selection, focus, scrolling, search and editors. `hud/hud_runtime_config.cpp`, `hud/hud_runtime_config_input.cpp` and `hud/hud_runtime_config_render.cpp` coordinate pages, pointer actions and drawing. Controller adapters call the same operations and geometry.

- Use `module_option::VisibleWhen` for dependent controls. Hidden values still persist but leave layout, search and focus. `available` means the capability or action exists.
- Keep named option IDs and INI keys stable. Order options with `module_option::Ordered` and `config/option_layout.hpp`.
- Put dimensions before positions, and pair width/height and X/Y on one row. Text sections start with font and size.
- Shared controls own toggles, dropdowns, numeric and text editing, dialogs and resizing. Cancel open editors before profile, selection or visibility changes.
- Preview changes commit through the same mouse, keyboard and controller actions, and roll back on Cancel.

See [configuration](reference/configuration.md) and [settings organization](reference/settings.md).

## HUD scale

[HUD scaling](reference/hud-scaling.md) covers Basic/Advanced, the full HUD presets and per-element scale.

- Keep authored sizes and positions separate from resolved screen bounds.
- Every HUD primitive uses the shared scoped transform; pointer entry points invert it once.
- Apply global scale once, and keep text effects.
- Add scale options after a module's existing option IDs, and keep each element's scale identity stable.
- The settings window, native windows and target rings keep their own sizing.

## Input

Follow [controller support](reference/controller-support.md) and [input ownership](reference/input.md) for anything interactive. Use the same semantic actions for mouse, keyboard and controller, with deliberate focus and exactly-once activation.

- An ordinary native menu must not disable visible pointer controls.
- Input XIUI+ does not claim stays with the game. An editor or popup owns only the input it needs.
- Consume an owned press, its repeats and its release, even when the action closes the surface.
- Release input on focus loss, hidden or disabled context, zoning, profile or runtime changes, and unload.
- Presentation preferences must not change the game's keyboard or gamepad configuration.

## UI sounds

Use `ui::native_ui_sounds` for cursor, confirm and menu feedback. Unsupported clients stay silent. Hover does not repeat at rest or while dragging, and an accepted activation confirms once. Sound previews do not change preferences or send actions.

## Persistence

Installed data lives under `config/XIUIplus/`, resolved by `settings/storage_paths.*`.

- Use `settings_store`, the normal module load/save hooks and the asynchronous writers. Unchanged values are not rewritten, and unknown fields and comments are preserved.
- Migrate paths before starting writers. Existing destination files win; conflicting older files are archived.
- Migrate existing defaults narrowly, with a version marker, and keep later customizations.
- Layout fitting uses the preset's `layout.viewport` and the real game viewport, and keeps modules the player moved.
- Portable profiles use the shared preference visitor, which excludes session state, character-local inventory layouts and controller-device data. Do not modify the embedded sharing preset.
- Installing a build never overwrites a player's settings, theme, positions or profiles. Missing files use embedded defaults.

## Performance and resources

- Keep optional artwork lazy, caches bounded and decoding off the render path.
- Per-frame rendering must not repeat filesystem access, native discovery or avoidable geometry work.
- Share one immutable combat-action decode and timestamp across consumers.
- Edit asset generators and their source inputs (`scripts/assets/`), not the generated tables.

## Checks

[Focused checks](../tests/README.md) should cover the boundary you changed. Automated checks do not establish in-game or physical-device behavior.
