# Magic Burst and skillchains

Displays observed skillchain/burst windows; Action Bars can consume the same tracker for predictions.

**Start in:** [skillchain_source.cpp](../../src/runtime/combat/skillchain_source.cpp), [magic_burst_module.cpp](../../src/runtime/modules/magicburst/magic_burst_module.cpp), [combat_settings.hpp](../../src/runtime/combat/combat_settings.hpp).

**Keep intact:** Use the shared Retail/Horizon/Phoenix rules and immutable action decode. Clear observations on identity, rules and lifecycle changes. Prediction is a hint and must not execute actions or invent confirmed chains.

**Focused check:** `./scripts/check.ps1 -Name skillchain`.

Edit-mode scaling uses the shared top-right gesture: the bottom-left corner stays fixed as the module grows upward and right. `hud-scaling` covers its drag, cancellation and committed placement.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
