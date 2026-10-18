# SimpleLog compatibility plan

Status: SimpleLog v1.2.1 installed and focused in-game checks completed on October 16, 2026. XIUI application code and runtime are unchanged. Broader combat/load-order acceptance remains open.

Research target: [Kunomaclis SimpleLog v1.2.1](https://github.com/kunomaclis/SimpleLog/releases/tag/v1.2.1), released October 1, 2026, at commit `9444776a6a889f64cc4f06b6bf3742728751c08b`. This plan concerns that maintained fork, rather than assuming every SimpleLog fork behaves identically.

## Installation and observed results

Installed the official release archive into `C:/Games/PhoenixXI-dev/PhoenixXI/addons/simplelog` and loaded it through AgentLink. Archive SHA-256 matched the release digest: `30a479b37bea4f7084bd1b561836392368a696fe9163b7b941665e74f9b4041d`. SimpleLog reported version 1.2.1 and a ready profile, creating its initial configuration, filter and color files. It remains loaded; `/slog` opens its settings.

Focused checks used local-only chat output and SimpleLog's real action formatter with synthetic action tables. No combat packets were injected, no actual Cure or attacks were performed, and jobs/position/game statistics were not changed. Temporary in-memory formatter/filter preferences were restored immediately; no diagnostic callbacks were installed.

- A line emitted with `AddChatMessage` reached XIUI once, retaining its inline actor color (captured mode 20, inline index 247).
- SimpleLog's Cure formatter produced a captured mode-30 message with actor/target color spans.
- Its two-hit formatter condensed 11,111 and 22,222 into a single 33,333 result with hit count 2. XIUI captured the mode-28 output and color spans.
- Temporarily enabling SimpleLog's applicable filter suppressed a local Cure fixture: its action message became zero and no replacement line was emitted. The original filter value was restored.
- Two successive identical local messages remained visible as separate XIUI rows. The bounded capture-count check accounted for each test message once.
- A settled in-game capture showed both `0x07` and ordinary LF examples on separate XIUI rows. The earlier source-only hard-break concern did not reproduce. `ParseAutoTranslate` alone preserved `0x07`; the precise upstream normalization point was not established.
- These checks ran with both native chat display flags already disabled, without changing that preference. XIUI continued receiving and displaying the output.

Limits: this is not a live server-combat encounter or a completed multi-target/AoE, alternate loading-order, Window-2-specific, zoning or addon-unload test. The capture and formatting evidence establishes useful coexistence in the current setup, without a bridge or XIUI code changes.

## Recommended behavior

Keep SimpleLog installed as its normal Ashita addon. SimpleLog owns battle-message formatting, condensation, filters and actor/target colors. XIUI displays the resulting chat feed in its existing windows, with its normal font styling, wrapping, scrolling, reactive sizing and input controls.

The preferred path is:

Game combat events → SimpleLog → final admitted native chat → XIUI history → XIUI Chat 1 / Chat 2.

The current capture receives SimpleLog output in the focused live checks above. A separate bridge remains conditional on finding an actual capture gap in an untested case.

## What the sources establish

### SimpleLog produces ordinary chat output

The tagged [entry point](https://github.com/kunomaclis/SimpleLog/blob/v1.2.1/simplelog.lua) registers packet and incoming-text handlers. Its [action handlers](https://github.com/kunomaclis/SimpleLog/blob/v1.2.1/lib/actionhandlers.lua) generate formatted messages through `AshitaCore:GetChatManager():AddChatMessage`. Its [text handlers](https://github.com/kunomaclis/SimpleLog/blob/v1.2.1/lib/texthandlers.lua) suppress selected original messages while allowing replacement output.

The [packet handlers](https://github.com/kunomaclis/SimpleLog/blob/v1.2.1/lib/packethandlers.lua) can rewrite action-packet message fields and block duplicate action-message packets. Compatibility must preserve the resulting display behavior without creating another combat-message formatter in XIUI.

### XIUI already captures a native output path

[native_chat_source.cpp](../src/host/native_chat_source.cpp) hooks native window insertion and records actual Window 1 / Window 2 admission from the native filter call. It delivers text only when at least one destination admits it. XIUI's `HandleIncomingText` is currently a no-op, and its plugin flags do not register the generic text callback as a capture source.

[runtime_host.cpp](../src/host/runtime_host.cpp) merges the two native-window deliveries of the same message, assigns XIUI sequence IDs and retains history across runtime replacement. The existing deduplication is based on native identity, content and disjoint window destinations. Ordinary repeated combat lines must remain distinct.

The installed client demonstrably delivers SimpleLog output through XIUI's current capture. Alternate loading orders and all possible interactions with other text-modifying addons remain unverified; do not promise universal load-order independence.

### A source-level hard-break concern did not reproduce live

SimpleLog's [action handlers](https://github.com/kunomaclis/SimpleLog/blob/v1.2.1/lib/actionhandlers.lua) replace `${lb}` with byte `0x07` in several message paths. XIUI's [text conversion](../src/runtime/chat/chat_encoding.cpp) has no explicit conversion for that control character, and [ParseChatText](../src/runtime/chat/chat_model.cpp) drops it. There is no current explicit hard-break handling for this form.

However, the settled live comparison rendered the native-break example correctly as two rows. Do not change the parser based only on its isolated handling of that byte: the observed feed is normalized upstream. If another setup demonstrates a failure, first record the bytes actually delivered to XIUI, then address that specific normalization gap while preserving color and message identity.

### Color support mostly exists

SimpleLog's [ColorIt helper](https://github.com/kunomaclis/SimpleLog/blob/v1.2.1/lib/functions.lua) uses Ashita's two standard inline-color helpers. XIUI already parses `0x1E` / `0x1F` color controls, their reset, and Shift-JIS text; its palette and rendering paths resolve inline spans.

One existing preference matters: [chat_color_overrides::Resolve](../src/runtime/chat/chat_colors.cpp) gives an enabled XIUI message-type color override priority over inline colors. Users wanting SimpleLog's per-actor colors should leave the relevant XIUI combat-color override disabled. Preserve saved preferences; do not silently change them or introduce a new color-precedence policy without an explicit product decision.

## Proposed work sequence

### 1. Establish the current compatibility boundary

In the implementation phase, compare a small set of native and XIUI chat output using the pinned release and the user's existing settings. Check a normal attack, a condensed multi-target action, a message with inline colors, and a message containing a native hard break.

Record which lines reach XIUI, their final mode, window destinations, color tags and identity. Use temporary, bounded local diagnostics only if existing diagnostics cannot answer this. Avoid recording ordinary chat contents in persistent logs.

Repeat the relevant comparison with the native chat display hidden through XIUI's presentation controls, and with both addon/plugin loading orders. Hiding the native display must remain separate from filtering its message destinations.

Outcome: identify whether a failure belongs to capture, native routing, color precedence or text layout. Do not add a second feed to compensate for a settings/routing issue.

### 2. Complete shared chat-format support

Address only formatting discrepancies established by a live comparison. The native hard-break example already works in the current setup. Retain SimpleLog's text, ordering, inline colors and mode. Preserve the shared font effects, timestamps, history identity and message-based scrolling.

Native filters and native Window 1 / Window 2 routing continue to govern admission. XIUI's existing message-source selection governs which captured destinations each XIUI window displays. A condensed line has one final mode and route; do not reconstruct its constituent hits to invent different destinations.

### 3. Add a capture adapter only if necessary

If formatted lines already arrive, retain the current capture path and ship the focused formatting fixes plus compatibility guidance.

If final addon output demonstrably bypasses the current hook, investigate a generic final-output capture point first. A generic callback must be proven to observe completed edits and block decisions; seeing a callback alone does not establish this. The SDK exposes mutable text and blocked state but does not, by itself, prove that every addon has already finished processing it.

If a cooperative bridge is ultimately required, specify its sender, version, message identity, final mode/colors and destination semantics before implementation. It must have one active source for each message, bounded delivery and replay handling, and a clean fallback when absent. Avoid simultaneously appending a bridge copy and a native copy, or collapsing legitimate identical messages with text/time heuristics. Any required SimpleLog modification is a separate, explicit integration choice.

### 4. Keep the user setup small

Prefer automatic coexistence: users load SimpleLog normally and configure its behavior through `/slog`. XIUI keeps its current chat controls.

Add concise compatibility guidance explaining native routing and XIUI color overrides. An optional shortcut to `/slog` can be considered later; it is not required for compatibility. Do not claim automatic addon/version detection unless a supported mechanism is established. Do not add a formatting toggle that merely duplicates SimpleLog's own enable/settings controls.

## Focused acceptance criteria

- A condensed action appears once, without an extra original combat line.
- Legitimate successive identical actions remain separate messages.
- Inline actor, target, damage and status colors match the final output when XIUI overrides are disabled; explicit overrides retain their documented precedence.
- Native hard breaks, arrows, indentation and multi-target names remain readable. All continuation rows scroll and retire with their parent message.
- Native Window 1, Window 2 and combined sources retain their actual routing. Hiding native chat visually does not lose XIUI output.
- SimpleLog filters are respected; XIUI does not resurrect suppressed messages.
- Relevant cases include damage, healing, misses, weapon skills, status messages, pet actions and crafting output.
- Loading/unloading SimpleLog and reloading XIUI do not replay or duplicate existing output, alter profiles or lose drafts/history.
- XIUI's combat/status sources remain separate from formatted chat. Audit any demonstrated packet-order interaction as its own data issue; chat compatibility must not casually change blocked-packet policy.

Use focused text/capture checks after implementation. Cody checks the visual and gameplay result; no broad suite or automated gameplay run is part of this plan.

## Decision

Keep the existing native-output capture: the focused live comparison confirms SimpleLog output reaches XIUI, including condensed messages, colors and multiline text. No XIUI bridge or parser change is currently justified. Use ordinary gameplay to evaluate the remaining acceptance cases, and investigate only a demonstrated incompatibility.
