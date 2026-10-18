# P27: Ready Check sound selection

Status: **Implemented** in the Native working tree. Playback validation: **Pending Cody's check**.

Audit date: October 15, 2026. Priority: Small independent customization.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

Ready Check now offers **Built-in chime** (default) and **Custom WAV**, a portable custom filename, Reload custom WAV and explicit Test sound. Existing send/receive toggles and 0–150% volume apply to either choice. Choosing or editing a sound never starts a ready check or plays a preview.

## Source evidence

- Lua reference: `XIUI/config/readycheck.lua: getSoundFiles and drawSoundFileRow`.
- Lua reference: `XIUI/modules/readycheck/init.lua`.
- Native integration: [src/runtime/modules/readycheck/ready_check_sound.hpp](../../src/runtime/modules/readycheck/ready_check_sound.hpp).
- Native integration: [src/runtime/modules/readycheck/ready_check_module.cpp](../../src/runtime/modules/readycheck/ready_check_module.cpp).
- Native integration: [src/runtime/ui/embedded_sound_player.hpp](../../src/runtime/ui/embedded_sound_player.hpp).
- Native integration: [src/runtime/ui/sound_choices.hpp](../../src/runtime/ui/sound_choices.hpp).
- Bounded local decoder: [src/runtime/modules/readycheck/ready_check_wav.hpp](../../src/runtime/modules/readycheck/ready_check_wav.hpp).
- Focused fixture: [tests/ready_check_sound_checks.cpp](../../tests/ready_check_sound_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- The current generated chime as the default sound, plus optional local WAV files.
- Preserve independent send/receive toggles, 0-150% volume and explicit preview.

## Dependencies

Current Ready Check sound player; no protocol changes.

## Implemented behavior

Place an optional file under the active Ashita installation's `config/sounds` directory, then enter only its filename, for example `party bell.wav`. Supported files are PCM WAV, 8-bit or 16-bit, mono or stereo, 8–192 kHz, at most 30 seconds and 8 MiB. Subdirectories, absolute paths and parent traversal are rejected. Audio files are optional and are not copied into profiles or share keys.

Selection, filename edits and Reload load/validate the clip outside rendering. Missing, invalid or unsupported clips fall back to the original generated chime; an unsupported playback format also retries the chime. The Reload option and `/readycheck status` report the current sound/fallback. Test sound and `/readycheck test` explicitly preview at the existing volume. Volume scales this playback's samples without changing the game/system volume.

`[readycheck] sound_choice` and `sound_file` persist the mode and a relative filename. Default installations still require only the DLL. Existing waveOut lifetime handling stops and releases playback on Stop, unload and runtime replacement. No Ready Check protocol, roster, response or announcement behavior changed.

## Input and controller behavior

Controller selection changes the configured choice; only explicit Test plays a preview. Opening a dropdown does not start a ready check or send chat.

## Completion criteria

- [x] Default mode retains the existing generated chime and single-DLL distribution.
- [x] Supported WAVs use the existing volume/toggles; unavailable audio retains a chime fallback.
- [x] Portable preferences contain a mode and filename, with no audio bytes or absolute paths.
- [ ] Cody's playback check.

## Focused validation

`RunReadyCheckSoundChecks` in [tests/ready_check_sound_checks.cpp](../../tests/ready_check_sound_checks.cpp) passed through `xiui_config_input_smoke --new-features-only`. It covers valid/invalid PCM, path restrictions, missing-file fallback and mode/filename/volume persistence without playing audio. Runtime/release build and isolated standalone startup passed. Cody checks audible playback.

