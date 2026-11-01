# Enemy List

Tracks relevant hostile entities and renders configurable HP, target, cast, timing and status rows.

Visible enemies share one native container around the measured list bounds. It grows and shrinks with visible rows, casts, statuses and columns, and hides when the list is empty. Individual rows retain their content padding, spacing, pointer targets and controller focus bounds; optional claim outlines and separate target panels remain independent.

**Start in:** [enemy_source.cpp](../../src/runtime/modules/enemylist/enemy_source.cpp), [enemy_list_module.cpp](../../src/runtime/modules/enemylist/enemy_list_module.cpp).

**Keep intact:** Keep identity tied to server ID plus entity index, and expire stale evidence. Reuse shared entity colors and text effects. Browsing must not change the game target until explicit activation.

**Focused check:** `xiui_module_parity_smoke`; enemy layout fixtures are in `tests/enemy_layout_checks.cpp`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
