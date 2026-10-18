# Pet portraits

These 24 images come from `XIUI/assets/pets` at upstream revision `a9e8d0c93710e5838749f8afc36f0058451c9a7e`. [sources.zip](sources.zip) preserves the original 14 avatar portraits, eight spirits, Wyvern and jug icon. [NOTICES.txt](NOTICES.txt) and [LICENSE.txt](LICENSE.txt) retain provenance and the upstream license. No automaton or charmed-pet portrait was present.

`python scripts/build-pet-portraits.py` rebuilds the embedded 1024×512 BGRA atlas from that archive. The 128-pixel cells preserve each image's aspect ratio with transparent padding. Native names select matching avatar/spirit portraits; Wyvern and jug categories work with custom names. Missing art uses the existing SMN, BST, DRG or PUP job icon when fallback is enabled.
