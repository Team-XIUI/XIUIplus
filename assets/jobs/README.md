# XIVParty job icons

The unmodified 64×64 job PNGs in `xivparty/` are from [Tylas11/XivParty](https://github.com/Tylas11/XivParty/tree/e5a27a01297d83e5e0259575487b13c243b58136/assets/jobIcons), revision `e5a27a01297d83e5e0259575487b13c243b58136`.

They are packed into `xivparty.bgra` and embedded in the runtime, with no dependency on a Windower installation. Rebuild the atlas with `uv run --with pillow python scripts/assets/build-icon-atlases.py`. Job IDs 1–22 retain FFXI's order, followed by MON and SPC. No unknown player job is guessed.

The upstream redistribution notice from `uiJobIcon.lua` is retained in [LICENSE.txt](LICENSE.txt).
