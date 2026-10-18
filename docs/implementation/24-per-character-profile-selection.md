# P24: Per-character active profiles

Status: **Implemented** in the Native working tree, October 16, 2026. Live two-character acceptance remains with Cody.

Audit date: October 15, 2026. Priority: Medium for multi-character use.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current behavior

The profile library stays shared. Once a character is identified, XIUI restores that character's selected profile. Selecting, creating, duplicating or importing a profile remembers it for that character through the existing controls. A character without a binding adopts the current global selection as a compatibility fallback.

Bindings live locally in `[profiles.characters]` in `config/xiuinative.ini`. They are excluded from profile files, preference overlays and share keys. The existing `[profiles] active` entry remains the global compatibility fallback.

## Source evidence

- Lua reference: `XIUI/XIUI.lua: charSettings.currentProfile and settings update callback`.
- Lua reference: `XIUI/core/profile_manager.lua`.
- Native integration: [src/runtime/settings/settings_profiles.cpp](../../src/runtime/settings/settings_profiles.cpp).
- Native integration: [src/runtime/hud_runtime_profiles.cpp](../../src/runtime/hud_runtime_profiles.cpp).
- Native integration: [src/runtime/hud_runtime.cpp](../../src/runtime/hud_runtime.cpp): settings_path_ and Initialize.
- Native integration: [src/runtime/settings/profile_share.cpp](../../src/runtime/settings/profile_share.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- A shared profile library with a local active-profile binding per stable server/character identity.
- Safe login/character transitions, a deliberate fallback for missing profiles and preservation of pending settings/drafts.
- No inferred automatic job-based HUD switching: Lua's job-specific action palettes are covered by P04.

## Dependencies

Existing Native profile manager; does not require hotbar palettes.

## Implementation

1. [profile_identity](../../src/runtime/settings/profile_identity.cpp) gates identity on logged-in, non-zoning SDK state and matching local entity/server IDs. The uniquely matched character-list lookups used by Ashita's `ffxi.account` library provide a world ID, checked against the selected character's name. Keys combine world and character ID. Clients without those supported lookups use installation-local character IDs; failed or mismatched reads remain unknown.
2. [settings_profiles](../../src/runtime/settings/settings_profiles.cpp) resolves the local binding against the shared catalog. Missing IDs/files select Default; an unreadable or unsupported existing profile leaves committed settings unchanged.
3. [hud_runtime_profiles](../../src/runtime/hud_runtime_profiles.cpp) drains pending writers before switching and uses the normal preference overlay/load path. It cancels profile confirmations, controller/config focus and transient interaction without sending chat, while preserving the chat draft and session data.
4. Profile autosaving is suspended during unknown identity and after a failed automatic load. Explicit selection can recover and bind the chosen profile to the newly identified character. Zoning back into the same character resumes its existing state.
5. Creation/import/duplication/selection and deletion fallback update only the current character's binding. Other bindings remain in the local INI; a later login repairs a binding whose profile was deleted. Runtime reload resolves the character binding again.
6. [settings_store](../../src/runtime/settings/settings_store.cpp) excludes the whole `profiles.*` namespace case-insensitively from portable preferences and overlays.

## Input and controller behavior

Profile selection and confirmations reuse current controller navigation. Character changes cancel active destructive dialogs and transient action focus without submitting anything.

## Completion criteria

- [x] Two synthetic character identities select different shared profiles and restore their layouts across transitions and a fresh runtime.
- [x] Unknown identity and failed profile loads cannot overwrite another character's saved profile.
- [x] Profile files, overlays and share keys exclude local character bindings.
- [ ] Cody verifies actual login switching between two characters, including client/world identity lookup and native input ownership.

## Focused validation

`xiui_config_input_smoke --character-profiles-only` passed. It covers pending-save ordering, separate character/world bindings, fresh-runtime restoration, unknown identity, unsupported/missing profiles, explicit recovery, draft retention, confirmation cancellation, portability filtering and the existing profile controls using isolated temporary settings.

The runtime is installed in the development setup. The game was closed during installation, so no live two-character switch or runtime activation was claimed. No Minimap implementation was included in this change.

