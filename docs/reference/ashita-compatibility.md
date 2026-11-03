# Ashita compatibility

XIUI+ ships one standalone `XIUIplus.dll` for Ashita plugin interfaces **4.16** and **4.30**. These are interface numbers, not the complete core version. `/xiuiplus mouse status` reports both the running core version and its interface.

The build uses the 4.30 SDK. [ashita_compat.hpp](../../src/shared/ashita_compat.hpp) selects the supported host interface for Ashita's loader and provides typed views of Party, Player, Entity and Target. All runtime and host reads of those objects use the views. Keep new SDK accesses on this path; accepting an older loader interface alone does not make newer virtual-method calls compatible.

The slot maps come from the official [4.16 SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/21053e446a0374ed19c3f9bb04f67cc7279af58d/plugins/sdk/Ashita.h), compared with the [4.30 SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/ed757ee915fb762c8bff61ca73a3aa839de9c91a/plugins/sdk/Ashita.h). The differences affect party member/status getters, mastery and combat/recast/zoning reads, later entity methods and native action/target methods. Legacy calls preserve x86 member-call conventions, including small-struct returns. Updating the build SDK requires reviewing these maps.

The 4.16 party interface does not expose remote master levels. XIUI+ leaves those unavailable fields at zero and resolves the local player's master level from the older player API. Keyboard binding signatures and outgoing-packet callback signatures also changed, but XIUI+ does not call those incompatible methods. The mouse, keyboard-state and controller callback registrations used by XIUI+ retain their signatures.

## Mouse boundary

[mouse_input.hpp](../../src/shared/mouse_input.hpp) normalizes Ashita callbacks before runtime routing. Button messages establish press/release state even when callback data has no Win32 held-button flags. Physical state can cancel a missing release on movement; it never starts an action. Shift/Ctrl are read separately, and wheel/X-button data keep their high word. Foreground suspension clears the observed button history.

Raw callback data and normalized keys appear separately in the mouse trace. Hover continues to follow cursor position; it is not evidence that a button callback reached Configuration. A Shift-only report still needs the affected client's trace before assigning a cause to another addon or a particular core build.

## Focused validation

`./scripts/check.ps1 -Name ashita-compat` covers both virtual layouts and zero-data pointer gestures. `xiui_actionbars_ui_smoke --input-ownership-only` covers Configuration capture and missed-release cancellation with normalized input.

`scripts/package.ps1` also loads the final standalone DLL against isolated 4.16 and 4.30 host fixtures and checks both interface exports. It does not replace or downgrade the running Ashita core. These checks establish the ABI and routing boundaries; live 4.16 gameplay and native-hook availability still require acceptance on that setup. Existing native signature/capability guards remain in effect.
