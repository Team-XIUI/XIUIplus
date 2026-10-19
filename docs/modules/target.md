# Target and child windows

Main Target owns mob information, cast presentation, the world HP bar and target rings. Subtarget and Target of Target have independent saved display settings.

**Start in:** [target_module.cpp](../../src/runtime/modules/target/target_module.cpp), [target_child_module.cpp](../../src/runtime/modules/target/target_child_module.cpp), [world_target_bar.cpp](../../src/runtime/modules/target/world_target_bar.cpp), [target_rings.cpp](../../src/runtime/modules/target/target_rings.cpp).

**Keep intact:** Revalidate server ID/index and keep unavailable native state explicit. World displays are passive and use the captured scene camera/depth. Rings follow the model's validated render position through `world/entity_render_position.hpp`; movement anchors can run ahead of the mesh. Target casts and information groups must not shift status anchors when activity changes.

**Focused check:** `./scripts/check.ps1 -Name target-recipient`, `target-rings` or `mob-info`, according to the changed display.

[Detailed guide](../reference/target-rings.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
