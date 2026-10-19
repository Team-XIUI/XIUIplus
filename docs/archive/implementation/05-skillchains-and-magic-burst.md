# P05: Skillchain prediction and Magic Burst

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree, October 17, 2026. Live combat timing, artwork and hardware acceptance remain with Cody.

Audit date: October 15, 2026. Priority: Medium; foundational for hotbar combat hints.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current behavior

Open **Action Bars → Hotbars / Crossbar → Skillchains** (`/xiui skillchain`) for the shared prediction switch and Retail/Horizon/Phoenix rules. **Open HUD settings** leads to **Extras → Magic Burst** (`/xiui magicburst`) for the separate display’s visibility, appearance and text. `/xiui skillchain status` reports rules, tracked targets, the selected target and feature switches.

One shared tracker drives per-target resonation, compatible closer icons on hotbars/crossbar and offensive-spell MB hints. The independent HUD shows the selected enemy's elemental burst artwork, estimated countdown, split dual-element images, fade-in and urgency pulse. HUD and prediction switches are independent. Retail rules are the default; Horizon applies an explicit property overlay.

Phoenix (75 cap) uses pinned SQL order and pre-SoA pet exclusions. All profiles retain property priority; Phoenix opener/continuation hints require more than three seconds and retain a 200 ms conservative closing margin. That shared profile also selects debuff estimates and Phantom Roll base powers. `/xiui rules phoenix|retail|horizon` changes it directly. See [Phoenix sources and implementation](../phoenix-source-audit.md).

Size, opacity, artwork and animation use normal settings. Countdown, Chain name and Elements have separate shared text profiles, offsets and effects; global typography visits all three. Optional element lists wrap. Edit mode supplies a separate movable Distortion sample without changing combat observations.

## Source evidence

- Lua reference: `XIUI/modules/hotbar/skillchain.lua`.
- Lua reference: `XIUI/modules/magicburst.lua`.
- Lua reference: `XIUI/modules/hotbar/slotrenderer.lua`.
- Lua reference: `XIUI/XIUI.lua: skillchainTrackingEnabled and incoming-action routing`.
- Native integration: [src/runtime/combat/action_packet.hpp](../../../src/runtime/combat/action_packet.hpp).
- Native integration: [src/runtime/combat/combat_source.hpp](../../../src/runtime/combat/combat_source.hpp).
- Native integration: [src/runtime/modules/target/target_module.hpp](../../../src/runtime/modules/target/target_module.hpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Per-target resonation and observed chain windows, compatible weapon-skill/ability predictions, supported retail/Horizon property sets and burst elements.
- Independent Magic Burst artwork/timer, dual-element treatment, urgency pulse/fade, scaling, movement and edit preview.
- Hotbar/crossbar slot highlights driven by the same tracker once those modules exist.

## Integration

The existing bounded action decoder already retains the skillchain additional-effect message; no packet layout or host/runtime ABI change was required. [skillchain_source](../../../src/runtime/combat/skillchain_source.cpp) examines all results before updating each target once, including chains reported on later hits. [magic_burst_module](../../../src/runtime/modules/magicburst/magic_burst_module.cpp) and [hotbar/crossbar foreground hints](../../../src/runtime/modules/actionbars/actionbars_display.cpp) share that source, element mask and deadline.

## Implementation

1. Track up to 128 server-ID keyed targets, checked against entity identity/index and health. Successful known openers establish resonation properties; observed chain messages establish burst elements even when opener metadata is unknown. Unknown successful weapon skills clear stale properties. Targets never borrow one another's state.
2. Follow XIUI's timing estimates: initial resonation lasts 10 seconds; observed chains delay continuation hints 3.5 seconds and shorten remaining time by depth. Terminal Light/Darkness, Radiance/Umbra and maximum depth offer no impossible continuations. Light/Darkness support all four native burst elements. Times are estimates, not server deadlines.
3. Gate physical Blue Magic with Chain Affinity/Azure Lore and supported elemental/helix spells with Immanence. Native local buffs and bounded remote observations supply gates; consume one-shot buffs after all packet targets. Unknown/healing spells cannot become guessed openers from their element alone. Avatar-specific hints require the active avatar.
4. Clear state on expiry/death, zone/logout, invalid identity, unavailable client state, settings/profile reload and rules changes. Evict oldest observations at capacity. Tracking stays active for either enabled consumer. Macro hints inspect representative actions; current `<t>` hints do not guess arbitrary/native-macro contents.

Prediction settings use `[actionbars] skillchain_predictions` and `skillchain_rules`, with legacy `[magicburst] predictions` / `rules` read as migration fallbacks. HUD settings retain `magicburst`, `layout.magicburst` and `text.magicburst.*`; observations are transient. [Pinned tables, art and notices](../../../assets/skillchains/NOTICES.txt) retain XIUI provenance and source hashes. [Table generation](../../../scripts/build-skillchain-data.py) resolves 395 Retail and 736 merged Horizon records; [art generation](../../../scripts/build-burst-art.py) reproduces the eight-orb atlas. Normal resource packing embeds artwork/notices with existing XIUI GPL-3.0 notices; no external assets, Lua runtime or extra DLL are needed.

## Input and controller behavior

The burst HUD is display-only. Configuration supports controller navigation without adding gameplay focus stops or automatically selecting/casting a suggested action.

## Completion criteria

- [x] Observed chains open the correct element/timer display; expired or unrelated targets do not leave stale burst hints.
- [x] Predictions and the burst HUD agree because they share state, and either can be enabled independently.
- [x] Unknown properties remain unknown; ruleset changes clear state and preview cannot create a live combat observation.

## Focused validation

`xiui_config_input_smoke --skillchain-only` passed: packed action decoding/truncation, opener/closer matching, target isolation, later multi-hit effects, continuation delay, terminal Light, four-element bursts, expiry/death, Blue Magic/Immanence gates, unknown-spell rejection, Retail/Horizon differences, bounded capacity/lifecycle and settings/text/position round trips. The focused check also covers legacy preference migration, canonical key precedence, the shared Skillchains section on all eleven bar views, and exactly-once HUD settings requests. The native x86 runtime built successfully.

AgentLink confirmed the settings relocation hot reload at **17:49:07**, runtime **Oct 17 2026 17:48:23**, GPU text, visible HUD, Retail rules and both features enabled. Compatible reload retained settings/layout. Cody's live WS/Magic Burst timing, single-/dual-element artwork, pet/BLU/SCH actions and physical-controller acceptance remain pending.

The installed development runtime contains this feature. Standalone **1.0.2** predates it; these changes await the next standalone release. References include original XIUI, [Ivaar Skillchains](https://github.com/Ivaar/Skillchains) and [LandSandBoat](https://github.com/LandSandBoat/server).

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

