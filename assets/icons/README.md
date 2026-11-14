# Config icons

`github.png` is the original XIUI GitHub social icon from [tirem/XIUI](https://github.com/tirem/XIUI/blob/4493ec0963abadf69a0c7d9a9d76e3ce4e45c90a/XIUI/assets/socials/github.png).
The original XIUI GPL notice is retained with the bundled XIUI+ asset notices.

`python scripts/assets/build-icon-masks.py` generates the compact alpha runs used in the shared icon atlas. Runtime drawing needs no image files or decoding.

`target-arrow.png` is original XIUI's `XIUI/assets/arrow.png`, used for its inline target-of-target connector. Its original GPL notice is covered by the bundled XIUI+ asset notices. `python scripts/assets/build-icon-masks.py` bakes the white coverage mask into the same atlas; the shared icon renderer rotates it for left, above and below layouts.
