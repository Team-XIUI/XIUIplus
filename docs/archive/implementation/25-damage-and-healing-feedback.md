# P25: Damage and healing feedback

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree, October 16, 2026. Visual feel and gameplay acceptance remain with Cody.

[Parity audit](../feature-parity.md) · [Plan index](README.md)

## Usage

**General → Appearance → HP Feedback** controls health effects across XIUI. **HP feedback** is the master switch; **Damage trail**, **Healing segment** and **Health flashes** also work independently. **HP warning colors** shifts HP fills through yellow, orange and red below 75/50/25%, retaining gradient contrast and alpha. Explicit Player custom HP state colors take priority. Shared mouse, keyboard and controller controls configure timing, damage/healing colors and flash strength. Preferences persist in `health.feedback` and portable profiles.

Defaults enable feedback: 0.2-second hold, 0.6-second decay, 0.22-second flash at 25% strength. The lost-HP segment uses bright white; healing uses mint. Damage flashes use a warning tint on the remaining fill. Existing untouched red trail defaults migrate once; later custom colors are retained. Disabling feedback preserves normal smooth fills, gradients, sheen/glow and text effects.

The shared path covers Player, Party and both alliances, Target, Subtarget, Target of Target, Enemy List, Pet, Pet Target and World Target Bar. MP, TP, casts and progression retain their existing behavior.

## Behavior

Observed losses leave a bright bounded trailing segment; gains highlight restored health. Repeated changes retain the earlier extent and restart the hold. Direction reversal clears the opposite effect, and segments/flashes expire. Live numeric values remain authoritative.

Warning hues use current observed HP, independently of smooth fill interpolation. Healthy fills retain saved colors; switching off warning colors restores them at all HP levels. The feedback master switch disables warning colors with the other effects.

First snapshots, actor replacement, known local maximum changes and long observation gaps establish a baseline. Window theme and Modern use the existing clipped range geometry. Frame/caps, base fill, prediction overlays and text retain the normal renderer path. No packet damage estimates or fabricated resource values are used.

## Source evidence

Lua reference: `XIUI/modules/targetbar.lua`, `XIUI/modules/playerbar.lua`, `XIUI/modules/partylist/display.lua`, Health Bar Flash Effects in `XIUI/config/global.lua`, and defaults in `XIUI/core/settings/modules.lua`, relative to the reference repository pinned in the audit.

Native integration: [feedback state](../../../src/runtime/ui/health_feedback.hpp), [settings](../../../src/runtime/config/health_feedback.cpp), [shared meter rendering](../../../src/runtime/modules/module.cpp) and [bar range geometry](../../../src/runtime/design_system.cpp).

## Completion criteria

- [x] Loss/gain segments stay bounded and expire after configured hold/decay.
- [x] First observations and actor replacement baseline without fake hits; long hidden/zoning observation gaps reset feedback.
- [x] Disabling feedback retains normal fills, gradients, sheens and text effects.

## Focused validation

`xiui_config_input_smoke --notification-feedback-only` covers first samples, repeated loss, healing reversal, expiry, disabling, actor replacement and timing/color-alpha round trips, white-trail default migration, explicit color retention, native warning hues and gradient/alpha preservation. Rendering reuses the shared range path for both bar styles. Cody checks appearance and gameplay.
