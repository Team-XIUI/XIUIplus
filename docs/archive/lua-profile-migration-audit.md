# XIUI Lua migration capability audit

> Historical record. Use the [current documentation](../README.md) for implementation guidance.

October 17, 2026. Source audit against Native commit `23c36dc9a8306c9b45eb9cccbc56387631b65d7a`. The audit below records that baseline. The user subsequently approved the action/macro, palette/crossbar, action-bar, pet, song-bonus and manual Fast Cast work described next.

## Scope decision

Approved: typed shared action records and macro cooldown/availability metadata; independent hotbar/crossbar palettes and ordering; a separate Shared expanded bank; per-bar text/spacing/label and crossbar layout/visibility controls; pet recast presentations, per-ability controls, resource text/width and labeled lifetime estimates; song-family duration bonuses; manual Fast Cast adjustments.

Deferred: richer backgrounds/borders, arbitrary fonts and external artwork, notification/treasure appearance changes, party/enemy visibility/layout extras, four-band HP/overlay parity, Mob Info source/format changes, Satchel/wardrobe filtering and tooltip/sort changes, and historical-cache metadata policy. These remain audit notes, not implementation tasks for this step.

The full-profile migration workflow remains described in the migration plan. This step adds the selected Native destinations and upgrades the existing action importer; it does not present that action importer as a finished whole-profile migration.

## Scope and evidence

Reviewed the actual regular release ZIPs for [Lua 1.8.4 stable](https://github.com/tirem/XIUI/releases/tag/v1.8.4) and [1.8.5 beta](https://github.com/tirem/XIUI/releases/tag/v1.8.5), including defaults, factories, configuration controls, migration code, saved-data writers and relevant renderers. The beta asset was updated October 9; its contents differ from the original tag. ZIP hashes are in the [migration plan](lua-profile-migration-plan.md).

The expanded defaults contain 510 non-nil root keys / 3,260 leaves for 1.8.4 and 520 / 3,278 for 1.8.5. These include repeated bar settings, legacy aliases and empty collections. They are neither feature counts nor import coverage percentages. Read/write inspection also found settings created outside defaults and character-owned data outside profiles.

Profiles generated offline through the distributed Lua `profile_manager.SaveTable` establish the real writer format. The proposed in-game populated-profile import has **not** run. This is a capability audit, not a claim that every setting has a tested converter.

Research, writer fixtures and the paused implementation are retained under ignored `out/lua-migration-research/`. The earlier unapproved runtime extensions remain removed. The selected Native destinations above are implemented; the rest of the findings below describe the audited baseline and deferred choices.

## Work the importer needs regardless of feature choices

These are migration plumbing, not missing HUD features:

- Read the real `local settings = {}; settings[...]=...; return settings;` files and unwrap `userSettings`. The current action importer reads returned literal tables, and the profile menu imports Native share keys. Neither is a complete Lua profile importer.
- Discover all profiles, profile order, character selections, modern `windowPositions`, legacy XIUI window positions and character-owned Satchel data. A profile file alone does not contain the entire setup.
- Resolve defaults, legacy aliases, global/per-module overrides, colors, enum names, units and scale. Native and Lua numeric theme indices must not be copied blindly.
- Convert every module's settings into existing Native persistence. Start from Native defaults so Lua cannot accidentally alter Native-only features.
- Preserve all action assignments, including disabled bars, unused slots, pet/job layouts, explicit clears, shared references and unused library entries that fit the existing model.
- Convert keyboard bindings and logical controller bindings. Hardware identifiers and backend indices require a separate review; copying them blindly is not portable.
- Create new Native profiles, handle name collisions and import Lua's Default as a new profile. Leave the current Native Default, active setup and character associations unchanged until explicitly applied.
- Preview exact conversions, approximations, unsupported customizations, missing files and capacity conflicts before writing. Keep a persistent report; never silently truncate actions or call an approximation exact.
- Adapt old/new Satchel cache files and copy compatible custom WAV files using existing Native support. Historical data must retain its provenance; it must not become a fresh live observation.

Native's existing profile, text/meter, module-position, action, controller and cache foundations can support this workflow. The missing importer itself does not justify expanding every renderer.

Native evidence: [action importer](../../src/runtime/modules/actionbars/action_import.cpp), [profile share reader](../../src/runtime/settings/profile_share.cpp), [profile menu](../../src/runtime/hud_runtime_profiles.cpp), [profile persistence](../../src/runtime/settings/settings_profiles.cpp).

## Baseline differences and migration fallbacks

Each item below records the baseline comparison. The scope decision above identifies the implemented groups. Fallbacks remain relevant for the deferred groups and for source data that cannot be represented exactly.

### 1. Backgrounds and borders

Lua stores per-module background themes, independent background/border opacity and texture scale, and tint colors. Bars, parties, pets, notifications and treasure have their own combinations.

Native's shared container renderer uses one selected game Window Type. Its container style has opacity, edge size, padding, scan lines and border visibility. It cannot generally express independent per-module window skins, both tint channels or independent background/border opacity and texture scaling. Some individual modules have additional fills, but that does not provide general parity.

**Existing-feature fallback:** retain bounds, supported opacity/padding and background visibility; use the selected Native window skin. Report the appearance choices that differ. Selecting a Native theme is an approximation, not a conversion of all those fields.

Evidence: Lua `libs/windowbackground.lua`, party/hotbar/pet factories; Native [container style and renderer](../../src/runtime/ui/container_renderer.hpp).

### 2. Fonts, outlines and external artwork

Lua allows installed font families, normal/bold weight and outline widths. Native has four fixed family/weight combinations: Bahnschrift bold, Segoe UI semibold, Tahoma bold and Grammara normal. Its configurable outline is an enabled/color choice rather than Lua's width control.

Lua also persists custom slot frame paths, custom action icon paths and party cursor filenames. Native uses its built-in frame choices, embedded action artwork and selection arrow; there is no corresponding general external-image path setting for these controls.

**Existing-feature fallback:** use an available font, supported text effects and built-in artwork, and report each replacement. Existing size, offset, color, shadow, glow and gradient controls still remain useful.

Evidence: Lua `fontFamily`, `fontWeight`, `fontOutlineWidth`, `customFramePath`, party `cursor`; Native [font catalog](../../src/runtime/ui/font_catalog.hpp), [text persistence](../../src/runtime/settings/text_style_settings.cpp), [slot style](../../src/runtime/modules/actionbars/actionbar_slot_style.hpp), [action icons](../../src/runtime/ui/game_icon_cache.cpp).

### 3. Reusable action library and macro behavior

Lua's library can hold typed actions as shared records, including unassigned records. Native slots support those action types, but its reusable `action_macro` library holds text lines, name and icon. It has no typed reusable-action definition with all Lua metadata. The current importer expands referenced records into slots instead of preserving the whole shared library.

Lua macros can select a different action as their recast source (`recastSourceType`, `recastSourceAction`, `recastSourceItemId`); beta macro records can also set `alwaysAvailable`. Native's action model has no equivalent fields. Keeping the macro commands alone would lose cooldown/availability behavior even if the commands still execute correctly.

**Existing-feature fallback:** preserve supported typed slot assignments and text macros; report lost library relationships or metadata. Do not flatten a typed action to text without warning. This is one of the strongest candidates for optional Native work because it changes behavior, not just appearance.

Native evidence: [action model](../../src/runtime/modules/actionbars/action_model.hpp), [current importer](../../src/runtime/modules/actionbars/action_import.cpp), [macro store](../../src/runtime/modules/actionbars/action_store.cpp). Lua evidence: `modules/hotbar/{data,macropalette,actions,recast}.lua`.

### 4. Palette independence, shared expanded crossbar and limits

Lua maintains separate hotbar/crossbar palette lists and ordering. Native stores palettes in a shared collection containing both surfaces. It can select them independently, but filtering and reordering do not preserve two independent collections/orders exactly. Simply merging same-named palettes could associate unrelated source data.

Lua's `useSharedExpandedBar` selects a **separate Shared bank**, retaining the L2+R2 and R2+L2 assignments independently. Native stores six banks: the two triggers, the two ordered expanded combinations and the two double taps. There is no seventh Shared bank or that selection mode.

Native also caps each profile at 64 palettes, 1,024 reusable macros and 32 pet layouts per palette; macros permit 32 lines of up to 240 bytes each. An actual source exceeding a limit needs an explicit decision. These are conditional conflicts, not a claim that ordinary profiles exceed them.

**Existing-feature fallback:** separate names/IDs to avoid collisions and choose a reported order. Shared expanded assignments could replace the two active expanded banks only with an explicit loss warning; that cannot also preserve all three independent source banks. Stop on data that does not fit rather than dropping it.

Evidence: Lua `modules/hotbar/palette.lua`, crossbar `Shared` mode and `useSharedExpandedBar`; Native [action layout](../../src/runtime/modules/actionbars/action_model.hpp) and [palette filtering, ordering and limits](../../src/runtime/modules/actionbars/action_store.cpp).

### 5. Action-bar presentation

Lua supports different horizontal/vertical slot gaps, per-bar text settings, label wrapping and detailed crossbar diamond/group/button-icon geometry. Native has one slot gap per bar, shared action text profiles and its own crossbar geometry. Lua's active-only/combat-only visibility, release-delay and fade controls are not equivalent to Native's inactive-side and animation toggles. Background customization also has the shared limitations above.

**Existing-feature fallback:** preserve slot sizes/counts, supported frames/colors/anchors, enabled bars and bindings; approximate spacing and shared text. List conflicting per-bar text styles and visibility policies. Assignments can be retained even when the presentation differs.

Evidence: Lua hotbar/crossbar factories; Native [bar presentation](../../src/runtime/modules/actionbars/actionbars_module.hpp) and [settings](../../src/runtime/modules/actionbars/actionbars_module.cpp).

### 6. Pet resources, recast displays and lifetime estimates

Native already has per-pet-type layouts, portraits and their transforms/clipping, resource meters, effects and ability cooldowns. Those do not need to be invented for migration.

Differences remain: Lua has full/compact/icon recast presentations, individual ability toggles, ready/recast gradients, separate name/timer styles and more resource-specific width/text choices. Native renders the supported cooldowns as text rows and shares its values text style across HP/MP/TP. Lua's estimated charm/jug remaining-time preferences do not match Native's explicitly unknown remaining lifetime and optional observed elapsed time.

**Existing-feature fallback:** keep supported pet layouts and cooldown rows. Report visual/control differences. Never label elapsed time as remaining time or fabricate an accurate expiry.

Evidence: Lua pet factories, `modules/petbar/{data,display}.lua`; Native [pet module](../../src/runtime/modules/pet/pet_module.cpp).

### 7. Notification cards and treasure presentation

Native already supports 2–6 notification groups, event routing, separate positions, stack direction, durations and pinned invitations. Treasure alerts belong in that module.

Lua's separate title/subtitle styling, card X/Y scale, progress-bar scale/direction and independent card background/border choices do not all exist in Native's single-text-profile group renderer.

Native Treasure already offers the pool and lot/pass actions. Lua additionally exposes independent timer-bar/timer-text/lot visibility, loot-state colors, collapsed-button preferences, reverse growth and a bottom anchor. Native's current pool settings do not expose those equivalents.

**Existing-feature fallback:** retain routing/groups, supported notification timing and pool enable/expanded/position/text settings. Use Native's cards and pool layout; report the remaining display preferences. No separate treasure-notification system is needed.

Evidence: Lua notification factories, `config/treasurepool.lua`; Native [notification settings](../../src/runtime/modules/notifications/notification_module.cpp), [notification style](../../src/runtime/notifications/notification_renderer.hpp), [treasure module](../../src/runtime/modules/treasure/treasure_module.cpp).

### 8. Party, enemy-list and visibility details

Native already has independently configured party/alliance lists, layouts, casts, resource text, job icons, effects, timers and selection feedback.

Specific gaps include Lua's ability to hide the main-job text while retaining subjob text, saved alternate horizontal/compact templates, custom cursor art, and the beta `partyListOnlyWhenEngaged` visibility rule. Native job text starts with the main job; it has one current set of dimensions per party rather than both Lua templates.

Lua's `enemyListAlignBottom` bottom-up growth also has no current Enemy List setting. Existing event/menu/native-macro visibility, player-while-in-party visibility, crowd-control-only effect timers and uncertain-duration markers already have Native counterparts.

**Existing-feature fallback:** migrate the selected party template and existing visibility controls; retain Native job display and top-down enemy layout. Report inactive templates and behavior rules that cannot be carried over.

Evidence: Lua party factories and `user.lua`; Native [party settings](../../src/runtime/modules/party/party_module.cpp), [enemy layout](../../src/runtime/modules/enemylist/enemy_list_module.hpp), [visibility model](../../src/runtime/config/module_visibility.hpp), [effect controls](../../src/runtime/ui/status_strip.hpp).

### 9. HP colors and estimated timing preferences

Lua stores four independently customizable HP gradient bands. Native Player exposes normal/warning/critical fills; other health meters use shared warning tinting. Those are not four independently configurable Lua bands. Lua's damage/heal overlay gradients and separate flash colors also do not map exactly to Native's flat damage/heal overlay colors and flash timing/strength.

Lua's manual per-job Fast Cast and cure/song/avatar timing adjustments have no matching Native cast settings. Native reads observed cast-bar progress, so missing sliders do not mean casting is missing.

Lua `songPlusLullaby`, `songPlusRequiem`, `songPlusElegy`, `songPlusThrenody`, `songPlusNocturne` and `songPlusVirelai` affect estimated debuff durations. Native's current duration lookup has no matching configurable song-family bonus inputs.

**Existing-feature fallback:** retain supported meter gradients and existing cast tracking; report band/overlay approximations and timing inputs with no destination. Leave current Native estimation behavior in place unless separately approved.

Evidence: Lua `core/settings/colors.lua`, `config/castbar.lua`, `handlers/debuffhandler.lua`; Native [Player settings](../../src/runtime/modules/player/player_module.cpp), [health feedback](../../src/runtime/ui/health_feedback.hpp), [cast source](../../src/runtime/modules/castbar/player_cast_source.cpp), [duration lookup](../../src/runtime/combat/status_durations.hpp).

### 10. Mob Info presentation and source selection

Native already shows levels, behavior/detection/linking, physical/element modifiers, ranks, immunities and server ID. Its information groups can be positioned independently, so a separately positioned Lua panel does not by itself require a new module.

Lua's selectable `mobInfoDataSource`, mob job display, no-data placeholder, hide-while-engaged rule, grouped percentage modifiers, separator style and reversed resistance-sign option do not have matching controls. Native uses its embedded database and its own multiplier/rank presentation. The Mob Info server-ID row is decimal; the separate Target suffix option is not equivalent to a full hex row.

**Existing-feature fallback:** map supported group visibility/positions/text and level display. Keep Native's database and labels, reporting source/format differences. Retain existing Horizon support elsewhere; this audit does not authorize removing it or substituting unrelated server data.

Evidence: Lua `mobInfo*` defaults/configuration; Native [Mob Info groups](../../src/runtime/modules/target/target_mob_info.cpp), [target display](../../src/runtime/modules/target/target_display_settings.cpp), [database](../../src/runtime/modules/mobinfo/mob_database.cpp).

### 11. Satchel and storage trackers

Native already has the Satchel grid, item tooltips, cached-character/slip browsing, empty-slot display and sorting. Lua's `satchelIncludeContainers`, configurable close-on-Escape and tooltip-icons-as-words preferences have no direct equivalents. Native's Satchel icon-size maximum is 64 pixels versus Lua's 96; tooltip fonts also have the general fixed-catalog limitation.

Lua's Auto Sort Bags preference keeps bags visually sorted and, where permitted, requests stacking. Native separates display order from autosort after its own transfers. Copying the boolean alone would change behavior.

The beta's individual `wardrobeEnabled[1..8]` presentation switches have no equivalent in Native's combined/separate wardrobe tracker, which includes unlocked bags. These are display preferences, distinct from inventory restrictions, which remain excluded from this work.

**Existing-feature fallback:** map existing grid/tracker settings and explicit display sorting; report filtering, size clamps, Escape/tooltip behavior and autosort differences. Do not enable restrictions or send inventory actions during migration.

Evidence: Lua `config/satchel.lua`, `modules/satchel/settings.lua`, `modules/inventory/wardrobe.lua`; Native [Satchel settings](../../src/runtime/modules/satchel/satchel_module.cpp) and [inventory tracker](../../src/runtime/modules/inventory/inventory_module.cpp).

## Data limitations rather than HUD feature gaps

Lua 1.8.4 caches inventories inside character settings; 1.8.5 uses `inventory_cache.lua`. The saved data includes items/counts, gil and slip bytes but does not provide all Native snapshot metadata, such as reliable observation time and every bag capacity/instance detail. The importer must not invent those values or overwrite a newer live Native snapshot.

Whether historical caches with unknown metadata should be exposed needs an explicit policy before writing a cache converter. A historical/unknown-age representation may need a small Native data-model change; it must not quietly claim the cache was just observed. This is separate from migrating module preferences.

External addon commands and native game macro references can retain their text/identity, but XIUI does not own the referenced addon or the game's macro files. Missing dependencies should be reported. A migration must never execute commands to test their meaning.

## Existing capabilities to reuse

There are existing Native counterparts for the core Player/Target/Subtarget/Target-of-Target, Enemy List, party/alliance, Pet/Pet Target, cast/cost, EXP, Gil, storage trackers, Satchel, Ready Check, Blue Magic, Phantom Roll, Skillchain and Magic Burst features. Existing text effects, meters, icon themes, sounds, controller support and persistence should be reused wherever their semantics match.

This means the work is primarily a converter across the whole setup, with the specific capability differences above. It does not mean every Lua field in those modules already has an exact mapping.

## Suggested decisions

1. Prioritize action data fidelity: shared typed library records, recast overrides, palette independence and the Shared expanded bank. These affect what users see selected or how assigned actions behave.
2. Decide separately whether remaining behavioral preferences matter enough to add: engaged-only party visibility, song duration bonuses, Satchel filtering/sort semantics and estimated pet lifetime presentation.
3. Treat appearance parity as optional scope: backgrounds/fonts/artwork first, then finer card/recast/text/layout controls. Import supported values and explicitly report substitutions when a capability is declined.
4. Finish the importer within the chosen scope, with every discovered meaningful field classified as exact, approximate, unsupported or blocked by source data. Unsupported fields are visible to users; they are not an automatic instruction to expand Native.

The selected Native work is implemented. The next migration step is the complete profile/settings workflow, using these destinations and reporting the remaining deferred differences.

## Validation and installed build

- Focused action data/identity, unused typed-library editing, shared appearance propagation with legacy override preservation, independent ordering/list scrolling, Shared-bank input, bar settings, song/Fast Cast arithmetic and pet preference checks pass.
- Real writer fixtures from 1.8.4 and 1.8.5 each import 408 actions, all 3 library entries and 4 independent sets without unsupported actions.
- Controller setup, Pet HUD/portrait persistence and embedded-default/existing-settings/reset checks pass.
- The standalone package and isolated Ashita startup checks pass. Version remains 1.0.3; no version bump or release tag was requested for this step.
- Installed the checked DLL in `C:/Games/PhoenixXI-dev/PhoenixXI/plugins/xiuinative.dll`; SHA-256 `4D45413278E8027C7C99E3354DCFDF926DD38DEFAEB4AD5510DFB9D5E3B661B2` matches the packaged DLL. AgentLink confirmed loading and the new seven-bank status. The theme file and all existing saved layout values remained unchanged; Native saved the added settings and palette migration after reload; the prior DLL, profiles and configuration were backed up under `out/lua-capabilities-install-20261017-204654/`.
- Startup also reported the untouched target-ring boundary hook unavailable because its device function was not owned by Ashita.dll. That separate hook issue was not investigated in this step.

The full in-game Lua population/profile migration journey has not run. Controller hardware and visual/gameplay behavior remain for Cody to inspect.
