# Heal bar prediction

Implemented for Party and Alliance HP bars, Player Bar HP, and World Target Bar. Enabled by default on each surface. A green segment shows the approximate HP gained from your selected or currently casting Cure I–VI or Curaga I–V, capped at full HP. A pink segment with dark diagonal stripes overlays existing HP for the predicted overheal. Both are enabled by default; HP text continues to show actual HP.

## Settings

- **Party / Alliance → Resources:** Heal prediction and Healing potency bonus (%).
- **Player Bar → General:** the same controls for your own HP bar.
- **Target Bar → World Target Bar → General:** the same controls for the world bar. World Target Bar must also be enabled.
- **Overheal prediction** appears beside Heal prediction and can be turned off separately. Turning Heal prediction off hides both segments.
- **Colors** on each surface: Heal prediction color and Overheal color, independent of the ordinary HP fill.

Healing potency bonus is one shared setting, displayed in each relevant settings page. Enter your combined Cure Potency and Cure Potency II gear bonus; the client does not expose these modifiers directly. The default is zero. Changing the shared bonus updates every preview. Visibility and color are separate per surface. Existing settings, text profiles, bar dimensions and row heights are preserved.

## Behavior

Native spell-menu selection previews the current party recipient, including native subtarget selection. Once casting starts, the action packet's recipient is locked: moving the hard target does not move the preview. Interrupted, finished, unavailable, dead or out-of-zone targets clear it. A selected spell with insufficient MP or an active known recast does not produce a preview.

Curaga previews living members of the recipient's six-person party within its ten-yalm radius when their native entity positions are available. It does not spread across alliance groups. The Player Bar also receives the Curaga forecast when you are in that area.

During healing, World Target Bar follows the intended recipient even while your hard target remains an enemy. It uses the same native ground projection as the ordinary world bar, and hides when the recipient's model cannot be projected or the bar would leave the screen. This does not change the game's targeting or take input ownership.

The overheal segment extends backward from current HP into the already-filled portion of the bar, leaving the useful green segment visible. Its length is excess healing divided by maximum HP, bounded by the available bar. For example, a 600-HP heal with only 100 HP missing shows 100 HP of useful green healing and 500 HP of pink overheal. At full HP only the pink forecast appears. The default is fully opaque, saturated pink for contrast over the HP fill. A one-time migration updates the former translucent pink default while preserving customized colors and later edits. Both directions use the same meter-range layout, gradients and alpha; MP cost preview clipping is unchanged.

## Estimate and limits

The estimate follows the local Phoenix server's modern Cure formula and its Curaga formula, using live MND, VIT and Healing Magic skill. Actual native Divine Seal and Rapture effects modify it. Formula references are `Phoenix/scripts/globals/magic.lua`, `Phoenix/scripts/actions/spells/white/cure*.lua`, and `curaga*.lua`; Curaga radii come from `Phoenix/sql/spell_list.sql`.

Remote maximum HP is inferred from the party's native current HP and HP percentage when an exact maximum is unavailable. Unknown maximum HP suppresses the segment. Weather/day bonuses, received-healing bonuses, enhanced Rapture equipment, server-specific adjustments and other hidden modifiers can change actual healing. Blue Magic heals, Cura, Waltzes, Regen, items and predictions for other casters are outside this first implementation. No fabricated status durations or combat results are used.

## Implementation

`modules/healing/heal_prediction` owns the shared estimate, target resolution and optional potency adjustment. Existing meter ranges now explicitly support segments beyond current fill; MP cost ranges retain their current-fill clipping. Overheal uses the shared range's striped presentation: a direct gradient bypasses the native gauge texture's color/alpha multiplication, with diagonal stripes batched in one draw and clipped inside the segment. Rounded ends retain the base gradient. All three surfaces reuse this source and their normal meter geometry, text profiles and native settings controls, including controller navigation.

Host/runtime ABI 13 adds validated party world anchors to the snapshot so native subtargets remain projectable through the cast. No additional HUD module is registered. Settings use `healing_prediction.bonus` plus `heal_prediction`, `heal_prediction_color`, `heal_prediction_overheal` and `heal_prediction_overheal_color` in each existing module section. Reset and profile loading use the same persistence path as other preferences.

Focused checks cover formula examples, potency and native buff multipliers, cast recipient locking, interruption/completion, full/unknown HP, useful-heal and overheal separation, exact-fill/large/dead-target cases, retained alpha, reversed bars, unchanged MP range clipping, settings placement and persistence. Visual and gameplay behavior are checked by the maintainer.
