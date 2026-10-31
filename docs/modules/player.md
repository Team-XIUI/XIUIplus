# Player Bar

Owns local HP/MP/TP presentation, identity text, state colors and optional attack/rest timing, MP-cost and healing overlays.

**Start in:** [player_module.cpp](../../src/runtime/modules/player/player_module.cpp), [combat_timing_source.cpp](../../src/runtime/combat/combat_timing_source.cpp), [mp_cost_preview.hpp](../../src/runtime/modules/castcost/mp_cost_preview.hpp).

**Keep intact:** Preserve independent resource labels/values and their migration keys. Use native maxima, shared meter layout and `TintTextProfile`. Optional previews/feedback must not replace confirmed resource values.

HP/MP/TP default to 12-pixel tracks independently of the enclosing window. Appearance presets apply explicit compact heights. The versioned load correction only replaces the old matching 28-pixel triplet; later and independently customized heights remain authoritative.

The global XIV 1.0 theme resolves 12-pixel copper gauges, salmon HP, olive MP and blue TP, with text centered over the lower rim. TP uses a single 0–3000 fill with 1000/2000 threshold marks and the same bar/value sizing as HP/MP. `ui/xiv1_resource_layout.hpp` shares champagne-gold labels and lining italic values with Party. Saved styles and formats remain intact, and Show Labels still applies. Mini timing anchors use the resolved tracks.

**Focused check:** `./scripts/check.ps1 -Name player-resources`; use `mp-preview` or `heal-prediction` for overlays.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
