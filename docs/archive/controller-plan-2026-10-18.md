# Controller work and remaining acceptance

> Historical record. Use the [current documentation](../README.md) for implementation guidance.

Read [Controller implementation](../reference/input.md) first for current behavior, entry commands and the host/runtime contract. Apply [the controller support skill](../../.agents/skills/xiui-controller-support/SKILL.md) when changing interactions.

## Current foundations

- Host input adapters and normalization: `src/host/controller_input.*`, `src/host/controller_normalizer.hpp` and `src/shared/controller_input.hpp`.
- The ABI version is defined in `src/shared/runtime_api.hpp`; use the source value when coordinating host/runtime changes.
- Runtime routing: `src/runtime/hud_runtime_controller*.cpp` and `src/runtime/input/`.
- Configuration: shared control geometry, configuration session state, and per-control keyboard/mouse/controller activation.
- Chat, party/enemy browsing, status inspection, equipment, Satchel, Action Bars and Treasure use their implemented semantic actions. The implementation guide records specific limits.

Native menus, text, subtargeting, movement, camera and macros keep their ownership outside explicitly claimed interactions. Preserve existing consumption/release and lifecycle guards.

## Remaining work

Physical devices, remappers, hotplug and multiplayer behavior need observed hardware/gameplay acceptance. Synthetic actions or an accepted AgentLink queue request do not establish delivery through the actual controller callbacks.

Controller-only chat composition still needs verified native software-keyboard/IME behavior. Existing keyboard composition and native slash/tell adoption do not establish arbitrary text transfer. Verify ownership, draft preservation, cancel and exactly-once submission before extending this route.

Do not change native gamepad settings or Ashita enumeration as a side effect of a presentation preference. Diagnose configured, connected, observed input and runtime readiness separately.

## Historical research

[The original October 14 proposal](controller-research-2026-10-14.md) preserves binding research, the pre-implementation architecture and proposed rollout. Its old ABI and missing-feature descriptions are historical. [The AgentLink evidence](controller-agentlink-2026-10-14.md) records the actual session and its limits. Neither document grants standing authorization for future live probes or feature additions.
