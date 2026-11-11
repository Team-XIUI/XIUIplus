# Pet and Pet Target

Shows pet-type resources, timers, recasts, effects and optional portraits. Pet Target has independent placement within the same family.

**Start in:** [pet_source.cpp](../../src/runtime/modules/pet/pet_source.cpp), [pet_module.cpp](../../src/runtime/modules/pet/pet_module.cpp).

**Keep intact:** Use pet generation/identity to clear stale observations on replacement or loss. Unsupported native effects remain unavailable. Preserve selected portrait clipping, frame ordering and text effects; pet-type timing uses shared server rules.

BST pets use the retained XIUI Lua jug-name database and monster flags to separate jug and charmed pets, including the native Ally flag. A confirmed Charm target overrides ambiguous flags for mixed pet jobs. Charmed pets use their own HUD style and Sic/Reward recasts.

Observe outgoing Charm before it resolves, capture player level/CHR/Charm+ gear, and send one targeted level check. Only a successful Charm starts the estimate; matching check replies supply the actual level. Suppress only XIUI+'s pending matching check output, and retain manual `/check` output. The shared runtime ABI carries outgoing observation and incoming consumption. Familiar uses ability 24 and its successful result, retaining the 25-minute extension even if the level reply arrives later. Pet loss, character/job changes and zoning clear stale tracking. Loading with an already-charmed pet cannot reconstruct its earlier start time and leaves the lifetime unavailable.

Charm estimates default on. A versioned Charm-only migration updates the former off default; later explicit opt-outs persist. Lifetime visibility, jug preferences, placement and text profiles remain independent.

**Focused check:** `./scripts/check.ps1 -Name pet-hud` covers classification, Charm/check results, timing, Familiar, stale state, migration and shared Pet presentation. Use `pet-tabs` for configuration selection alone.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
