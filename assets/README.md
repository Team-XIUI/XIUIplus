# Assets and generated data

Resources ship inside the standalone DLL. [cmake/resources.cmake](../cmake/resources.cmake) defines inputs; [pack-resources.py](../scripts/pack-resources.py) compresses suitable records during the build. Generated tables under `src/runtime/resources/` are data, not handwritten complexity; edit generator/source inputs.

## Generator routes

- XIV 1.0 frame/gauge artwork in `xiv1/`: [generate-xiv1-theme.py](../scripts/generate-xiv1-theme.py), with screenshot crop sources and atlas layout in [the theme note](xiv1/README.md). Embedded as resource 850.

- Action/controller artwork in `actionbars/`: [build-action-icons.py](../scripts/build-action-icons.py); custom artwork: [build-custom-action-icons.py](../scripts/build-custom-action-icons.py). These write atlases and lookup metadata, including `resources/action_icons.hpp` and `resources/custom_action_icons.hpp`.
- Status/job theme sources and atlases in `icon-themes/`: [build-icon-themes.py](../scripts/build-icon-themes.py). Job artwork in `jobs/`: [build-job-icons.py](../scripts/build-job-icons.py).
- Minimap archive, calibration and notices in `minimap/`: [build-minimap-maps.py](../scripts/build-minimap-maps.py). The generator restores alpha on source-connected burnt borders and preserves coordinate artwork; runtime contour rendering lives in `modules/minimap/` and `ui/contour.hpp`.
- Recast artwork in `recast/`: [build-recast-icons.py](../scripts/build-recast-icons.py); pet portraits in `pets/`: [build-pet-portraits.py](../scripts/build-pet-portraits.py).
- Notification artwork in `notifications/`: [build-notification-icons.py](../scripts/build-notification-icons.py); mob icons/data in `mobs/`: [build-mob-icons.py](../scripts/build-mob-icons.py) and [build_mob_data.py](../scripts/build_mob_data.py).
- Phoenix data in `phoenix/`: [build-phoenix-data.py](../scripts/build-phoenix-data.py); status/timing/skillchain tables: [build-status-rules.py](../scripts/build-status-rules.py), [build-timing-rules.py](../scripts/build-timing-rules.py) and [build-skillchain-data.py](../scripts/build-skillchain-data.py).
- Burst artwork in `skillchains/`: [build-burst-art.py](../scripts/build-burst-art.py); target arrow: [build-target-arrow.py](../scripts/build-target-arrow.py); GitHub icon: [build-github-icon.py](../scripts/build-github-icon.py); cursors: [build_cursor_assets.ps1](../scripts/build_cursor_assets.ps1).
- Shipped defaults/theme and the full Classic/XIV HUD captures are in `config/`: use [the capture workflow](../docs/development.md#capture-current-defaults). XIVClassic is an authored Classic derivative with XIV 1.0 Theme and Party resource labels. Named presets are independent of reset defaults. `profiles/share-v2.ini` is the immutable sharing preset.

Paths such as `resources/` and `modules/` above are relative to `src/runtime/`. Read generator arguments and source URLs/manifests before regeneration: some require external artwork directories, pinned downloads or Pillow. Normal builds use checked-in assets without refreshing upstream data. Regenerate only affected families and retain notices/attribution. Packaging embeds notices from `distribution/` and individual asset directories.
