# Settings organization

The general settings category is labeled **Global**. [Basic settings and HUD scale](hud-scaling.md) documents the two settings modes, whole-HUD presets and shared scale controls.

Use the [generated settings inventory](settings-inventory.md) for current pages and controls. The module registry and option metadata own the actual category/view/topic combinations; avoid copying their counts into hand-maintained guidance.

## Presentation rules

- Use one **Appearance** entry for visual settings. Nested categories follow a consistent order: **Size & Position**, **Window**, **Bar Style / Slots**, object artwork, **Colors**, **Effects**, **Animation**. Show only categories a module has; a single visual category needs no extra navigation level.
- Use **Content** instead of Display for choosing what is shown. Keep **Text** separate for typography and text effects. Keep functional topics such as Actions, Filters, Tracking and Alerts where they help the module.
- Appearance expands beneath its parent in the topic list. Returning restores the last category for that module/view. Mouse, keyboard/controller focus, help and sounds share the same rows.
- Keep original option IDs, topic IDs, saved keys and callbacks. Search and the generated inventory use the visible hierarchy; saved navigation still selects the original topic directly.
- Keep enable, preview and inheritance controls before the settings they govern.
- Put dimensions near the top of visual settings. Keep width and height on the same row when both are editable.
- Keep each object's X and Y offsets on the same row. Separate the offsets of bars, arrows, target panels, text and shadows.
- Follow dimensions with placement/alignment, offsets, spacing, padding and opacity.
- Start text styling with **Font / Size**, then **Font weight / Italic**, **Placement / Letter spacing**, **X offset / Y offset**, gradient colors and each text effect with its own controls.
- Keep gradient top/bottom colors together. Keep shadow, stroke, glow and animation controls with their own related settings.
- Keep available and unavailable controls in consistent positions. An empty cell can keep a related pair on the next row; it is never an input target.
- Preserve the native ordering of message types, filters, sounds and other domain lists that already have a meaningful order.

## Group changes

**Native UI** is a main category. **HUD Windows** contains native visibility, overhead names, independent window placement and native macro controls. **Menus and Dialogs** separates commands, spells/abilities, item details, merchants, auctions, bazaar comments, equipment/inspection, macro palettes and other dialogs. Existing settings, callbacks, `/xiuiplus gamehud` and saved navigation stay compatible. **Controller Focus** and **HP Feedback** remain direct Extras destinations.

**Inventory → Treasure Pool** contains the pool's visibility, lot/pass controls and appearance settings. General contains the title, detail and timer visibility switches. Text offers independent item, detail, timer and button profiles, including each profile's shared Show text control. A shaded title bar matches Satchel and holds Lot all, Pass all, a Collapse/Expand chevron and a Details eye icon; controls wrap when needed. Icon help follows hover and controller focus. Hiding the title frees header width while retaining its actions; hiding both details and timers releases secondary rows. Equal-width Lot/Pass buttons use centered labels; smaller detail text fits the available width beside a separately aligned countdown. It uses the shared configuration navigation and retains its existing saved settings independently of the storage trackers.

**Pet → each pet type → Portrait** groups image visibility, background visibility, base size/scale, Inline/Overlay positioning, paired X/Y offsets, opacity and clipping. Avatar / spirit adds an image selector with independent settings per avatar or spirit. Overlay moves only the artwork; Appearance categories retain the existing artwork and window controls.

**Party → General** includes Classic and XIV preset boxes below its regular controls, side by side when space permits and stacked in a narrow configuration window. Their descriptions stay visible, the entire card is clickable, and the shared confirmation applies the style to all three lists while retaining each position. See [Party presets](party.md#party-presets).

**Party/alliance → Appearance → Size & Position** contains the resource arrangement and selected bar's width, height, position and offsets. **Appearance → Window** contains the overall list width, entry spacing, reserved rows, fitting, padding and opacity. Window width is labeled separately from bar width. Heading visibility lives in General; Text edits its styling.

**Party/alliance → Appearance → Job Icons** pairs job-icon X/Y offsets below icon size and name-following placement. These offsets and **Text → Cast** offsets move their artwork without changing entry height or bar positions.

Party/alliance **Text** has separate **Jobs** and **Zone** elements. Jobs defaults to following the rendered name; Zone defaults to centered text inside its native panel. Placement, offsets and typography remain independent.

**Enemy List → Appearance → Size & Position** contains card width/bar height, columns/rows and column/row spacing. **Target Panel** contains the separate panel's visibility, width, X/Y offsets, padding, opacity and border. **Appearance → Window** contains the enemy card's padding, opacity and borders.

**Target Bar → Mob Info** has Common and ten information-group sub-tabs. Common contains enable, level, row spacing and shared text. Each group has visibility, Together/Own row/Independent placement, width/icon size, spacing, whole-group X/Y offsets and optional independent text styling. Detached groups reuse Movement and centering; attached groups retain the existing default layout. Subtarget and Target of Target keep their own level switches.

**Target → World Target Bar** has General and Text topics plus Appearance categories for size/position, bar style, colors, effects and animation. Width/height and X/Y controls share rows. It follows the native world target rather than exposing a draggable screen position; name and HP-percent profiles remain separate.

**Target of Target → Appearance → Size & Position** pairs its width and primary bar height. The fallback automatic height for an independent style lives in Bar Style.

**Target → Subtarget** has its own topics and text/status controls. Appearance → Size & Position offers Attached/Independent. **Target of Target → Appearance → Size & Position → Position** adds Detached. Separate windows use the shared movement controls; their linked placement preferences remain available when reattached.

**Player Bar → General** contains HP/MP value formats; **Colors** contains HP state thresholds/gradients and TP-ready/overlay/flash preferences. **Text** separates HP/MP/TP labels and values. **Player Bar → General** and **Party → Resources** each contain **Spell MP cost preview**, enabled by default and saved independently.

**Buffs/Debuffs → Appearance → Size & Position** contains icon sizing, wrapping, limits, overflow, alignment, offsets and spacing. Timer placement lives with the timer switches in General; timer text styling stays in Text. Party's combined and split status modes use the same ordering rules.

Shared text, meter, status, container, inventory, equipment, chat, notification, timing, compass, world-info, Ready Check and General controls use these rules. Reordering preserves existing values, defaults, saved keys and option IDs.

## Maintaining the order

`module_option::Ordered` supplies presentation priority without changing an option's ID or callback. Shared controls declare their own order. Priorities put primary controls before geometry, placement, offsets, spacing, padding/opacity, colors and effects; unassigned controls retain their original relative order. `pair_next` keeps consecutive priorities on one visual row.

`config/option_layout.hpp` arranges the filtered controls for the available one- or two-column layout. Rendering, pointer input, scrollbar sizing and controller focus all use those same slots. Controller directions use displayed rows and columns, skipping unavailable controls and empty cells; stable focus IDs still use the original option IDs. See [Configuration window](configuration.md) for resizing and reflow behavior.

The focused check writes the generated inventory when passed a path:

```powershell
bin/tests/xiui_config_input_smoke.exe --settings-presentation-only docs/reference/settings-inventory.md
```

The inventory comes from isolated module instances; native filter labels require the live client. This is a software input/layout check; it does not establish visual or gameplay behavior.
