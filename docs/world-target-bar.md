# World Target Bar

Implemented October 15, 2026 as a native Target extension.

Open **Target → World Target Bar → General → Enabled**. The default is a 120-pixel bar with a six-pixel HP track, centered ten pixels below the target's ground position. The October 15 22:40 default capture enables it with Cody's saved configuration; existing installations retain their own enable state. Target must remain enabled.

Layout controls width/height and paired X/Y offsets. Bar Style, Animation, Effects and Colors use the normal meter controls. Colors follows Target's gradient and optional entity colors by default; turn that off for a custom gradient. Optional Name and HP percent each have their own text profile. The world bar needs no fixed screen-position setting: it follows the selected target and camera.

The host reuses its captured world view/projection and viewport, projecting the native target ground coordinates. It validates target identity, health-bearing spawn flags, rendered/hidden flags and zoning state. Invalid, offscreen or behind-camera anchors draw nothing. Native HP percent supplies the fill; no HP maximum is estimated. Target changes clear the previous meter animation. Standard Target visibility rules also hide this overlay.

The bar is a screen overlay anchored in the world. It does not depth-test terrain or other models, so it can remain visible through geometry when the client still renders that entity. Flying/large-model placement can be adjusted with Y offset; it is anchored to the entity's native ground position rather than an estimated mesh bounding box.

Rendering occurs outside Target's content-bounds measurement. Moving the camera or target therefore does not enlarge the normal Target edit frame, change its height or move attached Target of Target/Subtarget windows. The display captures no mouse, keyboard or controller input. Its settings use shared config navigation, including the new Target view selector.

Implementation: [host projection](../src/host/plugin.cpp), [versioned snapshot](../src/shared/runtime_api.hpp), [settings/rendering](../src/runtime/modules/target/world_target_bar.cpp) and [Target integration](../src/runtime/modules/target/target_module.cpp). ABI 16 requires the development host/runtime pair to be replaced together; the standalone distribution remains one DLL.

`xiui_config_input_smoke --mob-info-only` covers projection rejection, compact placement, settings round trips and category ownership alongside the Mob Info checks. Shared settings presentation/defaults and standalone startup checks are recorded in the [feature audit](feature-parity.md). Cody checks appearance and gameplay.

## Healing recipient

[Heal prediction](heal-prediction.md) can temporarily place this bar under the intended party recipient during Cure / Curaga selection and casting, including native subtargets while the hard target remains an enemy. Its preview toggle and shared potency adjustment are under General; preview color is under Colors. The normal target returns when the healing preview ends.
