# XIUI notification icons

The unmodified 64-by-64 PNGs come from the local XIUI addon at `addons/XIUI/XIUI/assets/notifications`:

- `invite_icon.png`: Party Invitation, including alliance invitations.
- `trade_icon.png`: Trade Request.
- `bazaar_icon.png`: Key Item, matching XIUI's notification renderer.

Their original colors and transparency are preserved. `notifications.bgra` packs these images in that order into a 256-by-64 atlas, embedded in the plugin with no dependency on an installed XIUI addon. Rebuild it with `uv run --with pillow python scripts/build-notification-icons.py`.

The upstream XIUI [GPL-3.0 license](LICENSE.txt) is retained and embedded alongside the artwork.
