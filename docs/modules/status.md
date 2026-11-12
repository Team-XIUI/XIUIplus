# Status Effects

Renders independent local Buffs/Debuffs and shared status strips/tooltips for other HUD modules.

When Debuffs follows Buffs, inherited Appearance and Text controls remain visible and read-only. The text profile selector stays available for inspection; disable Follow Buffs settings under General to edit the retained Debuffs styles independently.

F → F selects Buffs; Enter shares controller Confirm's native cancellation request. Non-cancellable effects remain intact, with a chat message for keyboard attempts because keyboard browsing does not show the controller footer.

**Start in:** [status_effects_module.cpp](../../src/runtime/modules/status/status_effects_module.cpp), [status_strip.hpp](../../src/runtime/ui/status_strip.hpp), [status_durations.hpp](../../src/runtime/combat/status_durations.hpp).

**Keep intact:** Distinguish native timers from observed duration estimates. Do not fabricate arbitrary remote remaining time or unseen effects. Stable effect identities preserve inspection focus; hover/browsing never cancels an effect implicitly. Explicit local Confirm uses native cancellation flags and rechecks player/effect identity; remote inspection remains read-only.

Countdown formatting is shared: whole hours (`5h`) from one hour, `MM:SS` under an hour and seconds alone at 60 and below, including estimated bounds. Buffs/Debuffs use font-aware strip measurement for drawing, height/wrapping, hover and controller inspection. Keep timer profiles and their effects intact for long durations; never substitute a smaller hour-specific style.

**Focused check:** `./scripts/check.ps1 -Name status-snapshots`; display fixtures are in `tests/status_display_checks.cpp`.

Cancellation: `xiui_chat_smoke --status-cancel-only` checks native requests with a fake packet manager and never removes live effects.

[Detailed guide](../reference/status-effects.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
