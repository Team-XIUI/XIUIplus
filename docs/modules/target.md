# Target and child windows

Main Target owns mob information, cast presentation, the world HP bar and target rings. Subtarget and Target of Target have independent saved display settings.

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

[Detailed guide](../reference/target-rings.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
