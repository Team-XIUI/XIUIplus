# Settings organization

Use the [generated settings inventory](settings-inventory.md) for current pages and controls. The module registry and option metadata own the actual category/view/topic combinations; avoid copying their counts into hand-maintained guidance.

## Presentation rules

- Keep enable, preview and inheritance controls before the settings they govern.
- Put dimensions near the top of visual settings. Keep width and height on the same row when both are editable.
- Keep each object's X and Y offsets on the same row. Separate the offsets of bars, arrows, target panels, text and shadows.
- Follow dimensions with placement/alignment, offsets, spacing, padding and opacity.
- Start text styling with **Font / Size**, then **Placement / Letter spacing**, **X offset / Y offset**, gradient colors and each text effect with its own controls.
- Keep gradient top/bottom colors together. Keep shadow, stroke, glow and animation controls with their own related settings.
- Keep available and unavailable controls in consistent positions. An empty cell can keep a related pair on the next row; it is never an input target.
- Preserve the native ordering of message types, filters, sounds and other domain lists that already have a meaningful order.

## Group changes

**General → Game HUD** is a left-side settings topic containing the existing native-window visibility switches and bulk actions. It shares the original Game HUD settings and callbacks; the command shortcut and saved navigation open this destination.

**Inventory → Treasure Pool** contains the pool's visibility, lot/pass controls and appearance settings. Appearance separately toggles the title, details and timers. Text offers independent item, detail, timer and button profiles, including each profile's shared Show text control. A shaded title bar matches Satchel and holds Lot all, Pass all, a Collapse/Expand chevron and a Details eye icon; controls wrap when needed. Icon help follows hover and controller focus. Hiding the title frees header width while retaining its actions; hiding both details and timers releases secondary rows. Equal-width Lot/Pass buttons use centered labels; smaller detail text fits the available width beside a separately aligned countdown. It uses the shared configuration navigation and retains its existing saved settings independently of the storage trackers.

**Pet → each pet type → Portrait** groups image visibility, background visibility, base size/scale, Inline/Overlay positioning, paired X/Y offsets, opacity and clipping. Avatar / spirit adds an image selector with independent settings per avatar or spirit. Overlay moves only the artwork; Layout retains a second entry for the shared Show background setting.

**Party → General** includes Classic and XIV preset boxes below its regular controls, side by side when space permits and stacked in a narrow configuration window. Their descriptions stay visible, the entire card is clickable, and the shared confirmation applies the style to all three lists while retaining each position. See [Party presets](party-presets.md).

**Party/alliance → Layout** now contains the resource arrangement and selected bar's width, height, position and offsets. **Window** contains the overall list width, entry spacing, reserved rows, fitting, padding and opacity. Window width is labeled separately from bar width. Heading visibility lives in General; Text edits its styling.

**Party/alliance → Jobs** pairs job-icon X/Y offsets below icon size and name-following placement. These offsets and **Text → Cast** offsets move their artwork without changing entry height or bar positions.

Party/alliance **Text** has separate **Jobs** and **Zone** elements. Jobs defaults to following the rendered name; Zone defaults to centered text inside its native panel. Placement, offsets and typography remain independent.

**Enemy List → Layout** contains card width/bar height, columns/rows and column/row spacing. **Target Panel** contains the separate panel's visibility, width, X/Y offsets, padding, opacity and border. **Appearance** contains the enemy card's padding, opacity and borders.

**Target Bar → Mob Info** has Common and ten information-group sub-tabs. Common contains enable, level, row spacing and shared text. Each group has visibility, Together/Own row/Independent placement, width/icon size, spacing, whole-group X/Y offsets and optional independent text styling. Removed the duplicate Mob info entry in Target → Text. Detached groups reuse Movement and centering; attached groups retain the existing default layout. Subtarget and Target of Target keep their own level switches.

**Target → World Target Bar** has General, Layout, Bar Style, Animation, Effects, Colors and Text topics. Width/height and X/Y controls share rows. It follows the native world target rather than exposing a draggable screen position; name and HP-percent profiles remain separate.

**Target of Target → Layout** pairs its width and primary bar height. The fallback automatic height for an independent style lives in Bar Style.

**Target → Subtarget** has its own topics and text/status controls. Layout offers Attached/Independent. **Target of Target → Layout → Position** adds Detached. Separate windows use the shared movement controls; their linked placement preferences remain available when reattached.

**Player Bar → General** contains HP/MP value formats; **Colors** contains HP state thresholds/gradients and TP-ready/overlay/flash preferences. **Text** separates HP/MP/TP labels and values. **Player Bar → General** and **Party → Resources** each contain **Spell MP cost preview**, enabled by default and saved independently.

**Buffs/Debuffs → Layout** contains icon sizing, wrapping, limits, overflow, alignment, offsets and spacing. Timer placement lives with the timer switches in General; timer text styling stays in Text. Party's combined and split status modes use the same ordering rules.

Shared text, meter, status, container, inventory, equipment, chat, notification, timing, compass, world-info, Ready Check and General controls use these rules. Reordering preserves existing values, defaults, saved keys and option IDs.

## Maintaining the order

`module_option::Ordered` supplies presentation priority without changing an option's ID or callback. Shared controls declare their own order. Priorities put primary controls before geometry, placement, offsets, spacing, padding/opacity, colors and effects; unassigned controls retain their original relative order. `pair_next` keeps consecutive priorities on one visual row.

`config/option_layout.hpp` arranges the filtered controls for the available one- or two-column layout. Rendering, pointer input, scrollbar sizing and controller focus all use those same slots. Controller directions use displayed rows and columns, skipping unavailable controls and empty cells; stable focus IDs still use the original option IDs. See [Configuration window](configuration.md) for resizing and reflow behavior.

The focused check audits section/selector coverage, retained settings and positions, paired geometry and pointer/controller activation. It writes the inventory when passed a path:

```powershell
bin/tests/xiui_config_input_smoke.exe --settings-presentation-only docs/reference/settings-inventory.md
```

The inventory comes from isolated module instances; native filter labels require the live client. This is a software input/layout check. Cody checks visual and gameplay behavior.
