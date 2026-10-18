# P11: Blue Magic learned alert

Status: **Implemented**. Runtime/release build and focused checks passed; appearance/audio acceptance remains for Cody.

Audit date: October 15, 2026. Priority: Small independent feature.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

Native now has a separate **Extras → Blue Magic Learned** module. Generic reward/skill notifications retain their existing behavior.

The new module starts disabled to preserve the captured layout. Its explicit Preview action works while disabled; otherwise a disabled module has no edit preview or hit bounds.

## Source evidence

- Lua reference: `XIUI/modules/bluemagiclearned.lua`.
- Lua reference: `XIUI/config/notifications.lua: Blue Magic tab`.
- Lua reference: `XIUI/XIUI.lua: learned-message routing`.
- Native integration: [src/runtime/notifications/notification_center.hpp](../../src/runtime/notifications/notification_center.hpp).
- Native integration: [src/runtime/notifications/notification_source.cpp](../../src/runtime/notifications/notification_source.cpp).
- Native integration: [src/runtime/modules/notifications/notification_module.cpp](../../src/runtime/modules/notifications/notification_module.cpp).
- Native integration: [src/runtime/ui/embedded_sound_player.hpp](../../src/runtime/ui/embedded_sound_player.hpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Learned-event banner artwork, entrance/pulse/hold/fade, configurable duration/scale/position, sound and edit preview.
- A dedicated toggle/settings surface under **Extras**, as requested, alongside the existing reward stack.

## Dependencies

No new feature dependency.

## Implementation

1. [blue_magic_alert.hpp](../../src/runtime/modules/bluemagic/blue_magic_alert.hpp) bounds-checks the complete `0x029` learned-message structure, requires message 419 and the local recipient, and rejects invalid spell indexes. The module additionally validates the live local identity/login/zone and the native spell resource's Blue Magic skill and name. Chat text is never parsed as a learned event.
2. [blue_magic_module.cpp](../../src/runtime/modules/bluemagic/blue_magic_module.cpp) presents the original two-sided entrance, pulse, configurable hold and fade. Four pending events and an eight-entry recent-event ring bound queued/deduplicated work. Audio starts once when each accepted event begins. Preview animation is separate and silent; Test sound is an explicit shared action.
3. [assets/bluemagic](../../assets/bluemagic/README.md) contains the original XIUI WAV and a 900×450 BGRA version of its 1800×900 image, plus the upstream GPL notice. Resource 600 is cached once per device by `game_icon_cache`; 601 is played by the existing embedded PCM player and 602 retains the notice. No addon files are loaded at runtime.
4. `[bluemagic]`, `[layout.bluemagic]` and the shared fallback text profile persist enabled/sound, position, size/scale and hold preferences. Disabling, zoning/logout, identity/job changes and settings/profile loads clear pending animation. Closing configuration clears the transient preview. A renderer reload cannot replay a learned event because events are not persisted.

## Input and controller behavior

Display-only event presentation. Preview/sound-test settings use shared controller actions and never manufacture a live learned packet.

## Completion criteria

- [x] Local learned-event validation, bounded deduplication and one-shot banner/audio paths implemented.
- [x] Disable, preview-close and native lifecycle cleanup implemented.
- [x] Artwork keeps its aspect ratio; unavailable artwork uses the shared themed text profile and controls.
- [x] Runtime/release build and focused learned-alert fixture passed.
- [ ] Cody's appearance/audio check.

## Focused validation

[tests/blue_magic_checks.cpp](../../tests/blue_magic_checks.cpp) covers malformed/local/unrelated/skill-gain packets, duplicate/queue bounds and deterministic entrance/pulse/fade/cleanup through `RunBlueMagicChecks()`. It passed through `xiui_config_input_smoke --new-features-only`; runtime/release build and isolated standalone startup passed. Cody checks the banner appearance and audio.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

