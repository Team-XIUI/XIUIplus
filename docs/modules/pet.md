# Pet and Pet Target

Shows pet-type resources, timers, recasts, effects and optional portraits. Pet Target has independent placement within the same family.

**Start in:** [pet_source.cpp](../../src/runtime/modules/pet/pet_source.cpp), [pet_module.cpp](../../src/runtime/modules/pet/pet_module.cpp).

**Keep intact:** Use pet generation/identity to clear stale observations on replacement or loss. Unsupported native effects remain unavailable. Preserve selected portrait clipping, frame ordering and text effects; pet-type timing uses shared server rules.

**Focused check:** `./scripts/check.ps1 -Name pet-hud` or `pet-tabs`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
