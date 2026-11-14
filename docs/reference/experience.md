# Experience

Shows progression bars/readouts and session rates from native progression data and observed rewards.

**Start in:** [experience_module.cpp](../../src/runtime/modules/experience/experience_module.cpp), [experience_source.cpp](../../src/runtime/modules/experience/experience_source.cpp), [experience_tracker.cpp](../../src/runtime/modules/experience/experience_tracker.cpp).

**Keep intact:** Keep session tracking separate from display settings. Use shared meter layout and transient tabular digits so changing values do not move neighboring text. Preserve independent label/value visibility and anchors.

The XP track defaults to 12 pixels independently of the surrounding readout window. Keep that explicit height when applying appearance presets. The versioned load correction repairs the old automatic 80-pixel result while retaining later custom heights.

**Focused check:** `./scripts/check.ps1 -Name exp-display`; pure tracking uses `experience-model`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
