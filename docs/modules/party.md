# Party and Alliance

One module family renders Party and both Alliance groups, with resource bars, jobs, casts, status effects and optional world anchors.

**Start in:** [party_source.cpp](../../src/runtime/modules/party/party_source.cpp), [party_module.cpp](../../src/runtime/modules/party/party_module.cpp), [party_timing_source.cpp](../../src/runtime/combat/party_timing_source.cpp).

**Keep intact:** Revalidate member identity at activation and keep out-of-zone data explicit. Gameplay Up/Down remains native targeting. Jobs text belongs under Text → Jobs; job icons do not shrink the bar canvas. Movement clamps and snaps the party panel; external status strips and timers do not reserve screen-edge space. Presets preserve saved positions.

**Focused check:** `./scripts/check.ps1 -Name party-entry`; choose `party-presets` or `party-name-cast` for those boundaries.

[Detailed guide](../reference/party-jobs.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
