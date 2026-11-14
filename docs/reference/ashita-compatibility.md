# Ashita compatibility

XIUI+ ships one standalone `XIUIplus.dll` for Ashita plugin interfaces **4.16** and **4.30**. These are interface numbers, not the complete core version. Compatible SDK providers such as [VanaCore](https://apps.viion.co.uk/vanacore.html) can provide either interface without being an Ashita core DLL. `/xiuiplus mouse status` reports available host metadata and the selected plugin interface.

The build uses the 4.30 SDK. [ashita_host.hpp](../../src/shared/ashita_host.hpp) resolves the supported host interface for Ashita's loader; [ashita_compat.hpp](../../src/shared/ashita_compat.hpp) provides typed views of Party, Player, Entity and Target. All runtime and host reads of those objects use the views. Keep new SDK accesses on this path; accepting an older loader interface alone does not make newer virtual-method calls compatible.

Loader negotiation uses available core version exports to advertise 4.16 on an older Ashita core and otherwise advertises the build SDK, 4.30. Discovery covers renamed modules, decorated x86 export names and manually mapped images absent from Windows' DLL list. The bounded PE export reader does not require `GetProcAddress` registration. An early miss is retried until initialization. Once the loader supplies `IAshitaCore`, its vtable and first method identify the provider image; `GetHandle()` is only a fallback hint. The scene-boundary hook validates against this image owner.

The interface XIUI+ advertises and the loader accepts selects the SDK layout. Core version exports are optional metadata: an SDK facade with no `GetInterfaceVersion` or `GetAshitaVersion` export must not fail initialization or be forced onto 4.16. With no core metadata XIUI+ requests 4.30, which VanaCore documents as supported. `SelectedInterfaceVersion()` and `LegacyInterface()` drive SDK access; `HostInterfaceVersion()` describes detected core metadata only. `runtime_create_context::ashita_interface` carries the selected ABI to embedded and separate development runtimes. Known unsupported core interfaces and unsupported explicit ABI requests are still rejected. Missing metadata is reported as unavailable, without inventing a core version.

The slot maps come from the official [4.16 SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/21053e446a0374ed19c3f9bb04f67cc7279af58d/plugins/sdk/Ashita.h), compared with the [4.30 SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/ed757ee915fb762c8bff61ca73a3aa839de9c91a/plugins/sdk/Ashita.h). The differences affect party member/status getters, mastery and combat/recast/zoning reads, later entity methods and native action/target methods. Legacy calls preserve x86 member-call conventions, including small-struct returns. Updating the build SDK requires reviewing these maps.

`IItem` and `IStatusIcon` keep the same layout, but their bitmap contents vary by core build. Ashita 4.1.0.2 and later pass the DAT's headerless DIB, and `ImageSize` keeps the DAT value, which includes the 17-byte type/name prefix. Earlier cores rebuilt complete `.bmp` files. [item_icon.hpp](../../src/runtime/resources/item_icon.hpp) accepts both layouts and bounds reads by the fixed `Bitmap` buffer rather than `ImageSize`. `/xiuiplus equipment status` prints the leading bitmap bytes for the first equipped icon that fails.

The 4.16 party interface does not expose remote master levels. XIUI+ leaves those unavailable fields at zero and resolves the local player's master level from the older player API. Keyboard binding signatures and outgoing-packet callback signatures also changed, but XIUI+ does not call those incompatible methods. The mouse, keyboard-state and controller callback registrations used by XIUI+ retain their signatures.

## Mouse boundary

[mouse_input.hpp](../../src/shared/mouse_input.hpp) normalizes Ashita callbacks before runtime routing. Ashita 4.3.2.1 passes zero `wParam` on button messages, so callback data cannot be relied on for held buttons. Button messages establish press/release state; held flags on movement are trusted only after the host has sent them. `GetAsyncKeyState` reports physical buttons (mapped here for swapped buttons), runs ahead of queued messages and may never observe a press. Polling therefore recovers a missing release only after it saw that press and then reported release for `mouse_release_grace_ms`; it never starts an action or ends an uncorroborated drag. Shift/Ctrl are read separately, and wheel/X-button data keep their high word. Foreground suspension clears the observed button history.

Raw callback data and normalized keys appear separately in the mouse trace. Hover continues to follow cursor position; it is not evidence that a button callback reached Configuration. A Shift-only report still needs the affected client's trace before assigning a cause to another addon or a particular core build.

Run `/xiuiplus mouse clear`, open Configuration, try an ordinary click followed by a Shift-click, then run `/xiuiplus mouse status`. The trace retains six plain and six Shift button events separately, so later Shift-clicks cannot erase all plain-click evidence. Status also reports the core module name, selected SDK layout and per-mode event counts. An empty plain history means no plain button callback was recorded since clearing; it does not identify the upstream owner.

## Keyboard boundary

Ashita core 4.3.2.1 handles `/bind` in two places:

- **Window messages:** Ashita's ImGui consumes keys first. With game text input open and `keyboard.blockbindsduringinput` on, the key goes to the game without plugin callbacks. Otherwise a key that matches a bind by key, direction and modifiers (ignoring `%`/`$` input-state flags) reaches plugin callbacks with `blocked` set. Ashita then withholds the key from the game, so no `WM_CHAR` is produced.
- **DirectInput `GetDeviceData`:** BlockInput clears the buffer first. Matching binds then run and are removed from the buffer before plugin data callbacks see it, so a data callback cannot stop a bind.

XIUI+ holds BlockInput while any surface captures the keyboard, so binds cannot run while an XIUI+ editor, chat composition or keybind capture is focused. Those surfaces accept keys marked `blocked`; elsewhere `blocked` leaves the key with its existing owner. Keybind capture rejects a blocked key, because that owner would keep it during play. Ashita cannot treat XIUI+ fields as game input, so `%`/`$` flags and a disabled `blockbindsduringinput` do not apply there: binds stay suppressed while typing in XIUI+.

## Focused validation

`./scripts/check.ps1 -Name ashita-compat` covers both virtual layouts and zero-data pointer gestures. `xiui_actionbars_ui_smoke --input-ownership-only` covers Configuration capture and missed-release cancellation with normalized input.

`scripts/package.ps1` probes the final standalone DLL against isolated 4.16 and 4.30 host fixtures, including an SDK facade with no Ashita version exports. `xiui_ashita_compat_smoke <core DLL> <plugin DLL> --mapped-real` additionally checks version exports from a real core image without running its loader, imports or game hooks. These checks use fixtures and do not establish live VanaCore, gameplay or native-hook behavior.
