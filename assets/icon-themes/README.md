# Status and job icon themes

These packs come from the local XIUI source at revision `a9e8d0c93710e5838749f8afc36f0058451c9a7e`. Original PNGs and pack credits are retained in `sources/*.zip`; [NOTICES.txt](NOTICES.txt) records their attribution and [LICENSE.txt](LICENSE.txt) retains XIUI's GPL-3.0 license.

Status packs are HD, Tetsouou and XIView. Missing effects use installed native artwork. Job packs are Classic, FFXI and FFXIV-1; missing jobs use the existing XIVParty icons and its retained notice. Native statuses and XIVParty jobs remain the defaults.

`python scripts/assets/build-icon-themes.py` rebuilds the six embedded BGRA atlases and status-presence metadata from the retained sources. The 32-pixel job packs expand into the existing 64-pixel job cells with nearest-neighbor sampling. No background or cursor art is included.
