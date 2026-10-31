# Target and child windows

Main Target owns mob information, cast presentation, the world HP bar and target rings. Subtarget and Target of Target have independent saved display settings.

**Start in:** [target_module.cpp](../../src/runtime/modules/target/target_module.cpp), [target_child_module.cpp](../../src/runtime/modules/target/target_child_module.cpp), [world_target_bar.cpp](../../src/runtime/modules/target/world_target_bar.cpp), [target_rings.cpp](../../src/runtime/modules/target/target_rings.cpp).

**Keep intact:** Revalidate server ID/index and keep unavailable native state explicit. World displays are passive and use the captured scene camera/depth. Rings follow the model's validated render position through `world/entity_render_position.hpp`; movement anchors can run ahead of the mesh. Target casts and information groups must not shift status anchors when activity changes.

Rings default to model-relative sizing through Ashita's model hitbox radius. Keep the band, glow and facing marker proportional, and rebuild when the footprint changes. Fixed yalm sizing remains available in Target → Target Rings → Layout.

World rendering accepts the native fixed-function camera as well as the recognized terrain shader. The latter takes priority when present. Submit only at the matching scene end or scene-copy boundary; `target-ring-camera` covers both paths and frame lifetime.

Opt-in shader diagnostics live in [ring_shader_capture.cpp](../../src/host/ring_shader_capture.cpp). `/xiui rings capture` records rejected and accepted layouts for up to three seconds, then saves a bounded JSON report under the active install's `logs/xiui/` directory. Keep its GPU inspection inactive otherwise, and drain its background writer before unload.

Shared meter defaults resolve an automatic Target HP height to 12 pixels; the world bar retains 6 pixels. Classic and XIV apply the same Target text styles and HP height.

XIV 1.0 appearance adds the screenshot-derived name flourish, italic target caption, copper HP gauge and cast medallion. Attached casts appear above the target; independent casts retain their saved position. The shared cast chrome also serves the player Cast Bar, whose medallion uses the actual action icon when available.

**Focused check:** `./scripts/check.ps1 -Name target-recipient`, `target-rings` or `mob-info`, according to the changed display.

[Detailed guide](../reference/target-rings.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
