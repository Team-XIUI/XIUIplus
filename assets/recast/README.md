# Recast action artwork

`icons/abilities` uses recast timer IDs, with optional job suffixes for shared
abilities. `icons/spells` uses spell IDs. Missing artwork falls back to XIUI's
shared casting symbols.
Run `python scripts/build-recast-icons.py` to rebuild the atlas and index.
The atlas includes extruded edges to prevent neighboring icons bleeding when scaled.
