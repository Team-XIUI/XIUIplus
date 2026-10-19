# P01: Pet HUD

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** with the native-data availability limits below. Gameplay validation: **Pending Cody's check**.

Audit date: October 15, 2026. Priority: High.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

Native now has a Pet HUD and separate Pet Target surface. Both start disabled to preserve existing layouts. Five configuration views cover Avatar/spirit, Jug pet, Charmed pet, Wyvern and Automaton, with a sixth shared Pet Target view. Each pet category has independent visibility, resource meters, gradients, text effects, status layout and cooldown preferences.

## Source evidence

- Lua reference: `XIUI/modules/petbar/{init,data,display,pettarget}.lua`.
- Lua reference: `XIUI/handlers/petbuffhandler.lua`.
- Lua reference: `XIUI/config/petbar.lua`.
- Native integration: [src/runtime/hud_runtime.cpp](../../../src/runtime/hud_runtime.cpp).
- Native integration: [src/runtime/combat/combat_source.hpp](../../../src/runtime/combat/combat_source.hpp).
- Native integration: [src/runtime/modules/recast/recast_source.hpp](../../../src/runtime/modules/recast/recast_source.hpp).
- Native integration: [src/runtime/ui/game_icon_cache.hpp](../../../src/runtime/ui/game_icon_cache.hpp).
- Native source: [src/runtime/modules/pet/pet_source.cpp](../../../src/runtime/modules/pet/pet_source.cpp).
- HUD/configuration: [src/runtime/modules/pet/pet_module.cpp](../../../src/runtime/modules/pet/pet_module.cpp).
- Focused fixture: [tests/pet_hud_checks.cpp](../../../tests/pet_hud_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Pet identity, HP/MP/TP where supplied by the client, status icons, pet target HP/name/distance and separate or snapped target placement.
- SMN avatar/spirit, BST jug/charmed pet, DRG wyvern and PUP automaton presentations; per-type visibility, text, meters, artwork and ability-recast preferences.
- Ready charges and Blood Pact/other pet-job recasts. Jug/charm lifetime information is explicit about unavailable native deadlines, with optional observed elapsed time.

## Dependencies

No new feature dependency. Reuse combat, recast, status icons and entity helpers.

## Implemented native core

`pet_source` samples the SDK at most every 100 ms. The native owner's validated entity index supplies its pet index; pet server ID plus entity index, owner, zone and jobs guard identity. Pet replacement/loss, zoning/logout, unavailable memory and job changes clear the current view and its observed generation. HP is the native percentage. SMN/PUP MP is the generic native pet MP percentage; TP appears only for valid native 0–3000 values. Absolute HP/MP, maxima and levels are not calculated from percentages.

Pet category uses available pet jobs, native monster/pet flags and known avatar/spirit identities. Status icons reuse confirmed combat observations since the current pet was observed. The SDK exposes no complete pet status array: effects that were never observed are not fabricated, and unknown durations stay unknown. Hover and deliberate controller inspection use the same native artwork, names and descriptions. Replacement clears stale inspection and meter animation state.

Ability rows consume the existing `recast_source`: Blood Pact/SMN timers, Ready/Sic/Reward/Call Beast, Wyvern abilities and Automaton abilities remain separated by category. Ready charge counts and next-charge timing come from the shared native recast calculation. Missing recast entries are omitted rather than reported as ready. **Recasts without pet** can retain known cooldowns for an applicable pet job when no pet is active.

Pet Target reads a valid native targeted entity when available, otherwise the pet's confirmed recent action recipient. **Snap to Pet** attaches it below the parent; turning it off exposes its own saved width/position and shared mouse/controller movement controls. Target presentation is shared by the snapped and independent forms and is drawn once. Independent Pet Target can remain visible with Pet disabled; a missing pet/target hides the live surface.

**Pet → Pet Target → Layout** includes **Show background** (enabled by default), background opacity/padding and an optional **Tint background** color wash with its own color and strength. Tint starts disabled. Attached and independent targets share these settings, separately from the main Pet background. The shared container clips the wash inside the native frame and scales it with background opacity; text, HP fill and effects keep their own styling. Tint color is also available in Colors. Preferences persist in `[pet.target]` and profiles.

Per-type settings use `[pet.avatar]`, `[pet.jug]`, `[pet.charm]`, `[pet.wyvern]` and `[pet.automaton]`, with shared text/meter/status persistence. Main bounds use `[layout.pet]`; target preferences use `[pet.target]`, with independent bounds saved only by the target adapter in `[layout.pettarget]`. All pet and target text profiles participate once in global text styling. Native containers/meters and the shared icon cache supply presentation. Edit mode uses empty/unknown placeholders when there is no matching live pet, without invented countdowns.

**Portrait** uses XIUI's original avatar/spirit, wyvern and jug artwork from the embedded atlas. Name matching selects known avatar/spirit art; custom wyvern names retain the wyvern portrait and jug pets share the jug image. The topic exposes portrait visibility, job-icon fallback, base size, Inline/Overlay positioning, scale, opacity, paired X/Y offsets, clipping and **Show background**. Overlay starts at the Pet window's top-left, draws behind its text and bars, and reserves no heading space. Background visibility and image opacity are independent; unclipped artwork can extend outside the window. Clipped artwork uses the complete nine-slice window bounds, without text padding, and the native frame draws over the image.

All 22 avatars/spirits have clipped Overlay presets based on Cody's Carbuncle setup: 126 px base size, 65% opacity and individual scale/offsets for the artwork. Carbuncle retains 1.65× scale and +173/−77 offsets. `[pet] portrait_preset_version=1` migrates untouched old image defaults and the old 48 px Inline avatar layout once; customized image profiles and later changes remain intact. Window positions and other pet-category layouts are preserved.

**Avatar image** selects the avatar or spirit to preview and edit. Each image retains its own scale, opacity, offsets and clipping in `[pet.portrait.<name>]`; live gameplay resolves settings from the actual summoned pet, independently of the preview choice. Other pet types retain their own image settings in `[portrait.pet.<type>]`. Base size and positioning mode remain per category. These controls reuse shared dropdowns/sliders and pointer, keyboard and controller configuration paths. The optional **Job icon fallback** handles unavailable portraits, including automatons/charmed pets without dedicated upstream artwork. Atlas cells preserve image proportions and transparency; artwork is cached through `game_icon_cache::PetPortrait` with the existing device lifetime and bundled notices.

## Native-data availability

**Lifetime information** explicitly displays **Lifetime unavailable** for Jug/Charmed pets because no client deadline is exposed. Optional **Observed elapsed time** can appear alongside it and is precisely time since XIUI saw that pet, not summon age or a remaining-duration estimate. Unknown durations and missing status observations remain explicit limitations of the available data. No `/check` command, outgoing-packet replacement, server-specific lifetime guess or hidden timer scanner is used. This native presentation deliberately supplies truthful unknown values rather than Lua's speculative charm calculation.

## Input and controller behavior

The HUD is passive. Configuration and movement use the existing mouse/keyboard/controller foundations; neither executes pet abilities or changes native targeting. Pet status inspection enters deliberately, navigates actual visible effects with stable pet/effect identity, and returns through Cancel. Native spell/ability subtargeting retains ownership.

## Completion criteria

- [x] Five category presentations use available native percentage/TP fields and shared resource styling.
- [x] Pet identity/session changes clear the view; no-pet cooldown display is configurable.
- [x] Snapped/independent target placement, per-type settings and text effects persist.
- [x] Observed status details support hover/controller inspection without fabricated effect durations.
- [x] Original available pet portraits and configurable job fallback share the embedded icon cache.
- [x] Jug/charm lifetime availability is explicit, with optional observed elapsed time.
- [ ] Cody's in-game checks across the five job presentations.

## Focused validation

`RunPetHudChecks` in [tests/pet_hud_checks.cpp](../../../tests/pet_hud_checks.cpp) passed through `xiui_config_input_smoke --new-features-only`. It covers category/recast filtering, snapshot generations, replacement/loss, stale inspection, Ready charges, no-pet cooldowns, truthful lifetime text, portraits, defaults, target persistence and exactly-once global text visiting. A regression verifies registry-order reset with themed target text; the full embedded-default reset check also passed. These fixtures do not establish live native data or gameplay behavior. Cody checks the five job presentations, native pet flags and artwork in game.

`xiui_config_input_smoke --pet-hud-only` focuses on Pet behavior, per-avatar image persistence/resolution, overlay placement without resource-layout changes, bounded imported values and exclusive mouse/controller tab selection.

