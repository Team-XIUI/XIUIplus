# Vincerno macro icons

Source: https://github.com/Vincerno/xiui-icons

Pinned commit: `da71da9e137a26cdc9c3f807d0859ba8fbb0a671`.

`sources.zip` retains all 220 upstream PNGs with their original hotbar-relative
paths, including clean variants. `LICENSE.txt` retains the upstream GPL-3.0
license; `NOTICES.txt` and the license are embedded and seeded alongside XIUI's
other notices. The Photoshop template is not needed at runtime.

Run `python scripts/build-custom-action-icons.py` from the repository to rebuild
`icons.bgra` and `src/runtime/resources/custom_action_icons.hpp`. The script uses
Pillow and reads only the checked-in source archive. A separate compressed atlas
keeps existing action artwork intact and loads only when requested through the
shared icon cache. Portable override keys use `vincerno/<original path>` without
the PNG extension, in lowercase.
