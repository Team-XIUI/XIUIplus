# Phoenix mob snapshot

`phoenix_mobs.bin` is built from [HeadsUp](https://github.com/Tanyrus/headsup/tree/161ad60f7d0ed10fd5b71a6889bd777a43d5dc7b), commit `161ad60f7d0ed10fd5b71a6889bd777a43d5dc7b`, using its `data/phoenix_mobs.tsv`. That dump pins Phoenix commit `11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e`, generated October 16, 2026: 64,702 mobs across 216 zones.

HeadsUp and Phoenix distribute this data under GPL-3.0-or-later; the accompanying license is retained. Original TSV, server dump tooling and provenance are available at the pinned HeadsUp repository. This snapshot describes that server build, not current hidden live-server values.

Regenerate from that TSV:

```powershell
python scripts/build_mob_data.py path/to/phoenix_mobs.tsv assets/mobs/phoenix_mobs.bin
```

Binary layout: `XIM1`, record count, record stride and name-table byte count (little-endian 32-bit fields); sorted 72-byte records; deduplicated UTF-8 zero-terminated names. Damage adjustments use `1 + SDT / 10000`; elemental/status resistance ranks and explicit immunities remain separate. Job/family, exact live levels and respawn deadlines are unavailable and are not invented.

The Target spawn display supplements the finalized records with the effective Phoenix spawn-slot relationships in [assets/phoenix](../phoenix/README.md). Multiple NM candidates, source chances and base cooldowns remain distinct from an observed personal respawn timer.

Target information uses the installed client's native element and status artwork. The four physical-damage images in `icons` are the original Slashing, Piercing, Impact and H2H assets used by Lua XIUI's Mob Info, credited to ThornyFFXI/MobDB under the retained MIT license. `scripts/build-mob-icons.py` packs these into the embedded, padded `physical.bgra` atlas. Resources 704–705 contain the art/license; no external icon installation is needed. Light/Dark Sleep share the native Sleep icon with distinct native element badges. Provenance stays in these documents and notices, without a HUD footer.

The same original MobDB pack supplies PassiveNQ/HQ, AggroNQ/HQ, Link, Sight, TrueSight, Sound, Scent, Magic, JA and Blood. The builder packs these into `detection.bgra` (resource 706), sharing the retained MIT notice. HQ indicates notorious monsters; true sound reuses Sound artwork as in the Lua reference. Native dataset flags decide which cells appear. The row has no Detects heading and keeps readable fallback names for missing art.
