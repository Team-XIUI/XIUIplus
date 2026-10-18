# Phoenix rules and spawn data

Pinned server source: [phoenixffxi/Phoenix](https://github.com/phoenixffxi/Phoenix/tree/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e), live commit `11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e` (October 14, 2026).

The **Phoenix (75 cap)** profile follows the ToAU configuration retained as `source/live_settings.lua` from [HeadsUp commit 161ad60](https://github.com/Tanyrus/headsup/tree/161ad60f7d0ed10fd5b71a6889bd777a43d5dc7b). That export pins the same Phoenix revision: ToAU enabled, WotG/Abyssea/SoA disabled, level cap 75, content restrictions enabled and Adoulin weapon-skill changes disabled. This is a published source snapshot; it does not assert that a running server uses identical private settings.

`source/` retains the SQL and Lua inputs and their GPL license. `sources.json` records exact hashes. `spawn_slots.json` retains 708 candidate/placeholder relationships and hashes of the effective zone YAML, applying Phoenix zone and NM-respawn overlays after base data. Chances and cooldowns describe source rules, not observed respawn deadlines.

Regenerate the checked-in headers:

```powershell
python -B scripts/build-skillchain-data.py
python -B scripts/build-phoenix-data.py
```

To refresh spawn relationships from the pinned checkout, install PyYAML and pass `--source path/to/Phoenix` to the second command. Update the pinned commit and inputs deliberately before regenerating for another server revision. The finalized mob-stat snapshot is documented separately in [assets/mobs](../mobs/README.md).

Skillchain inputs retain ordered properties and pre-SoA Blood Pact exclusions. Roll inputs combine base, pre-SoA and pre-Abyssea tables; displayed powers remain base values without unknown gear/job bonuses. Phoenix debuff ranges are estimates, and native local-player deadlines stay authoritative. Retail and Horizon remain separate profiles. No inventory restriction policy is implemented by this data import.
