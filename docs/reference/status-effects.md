# Status Effects

XIUI+ uses the installed client's status icons, names and descriptions. Local countdowns come from actual client timers. Confirmed combat effects use the original XIUI duration database for estimated countdowns, identified in their hover tooltips.

## Your Buffs and Debuffs

Open **Status Effects** in configuration, or use `/xiuiplus statuses`, `/xiuiplus buffs` or `/xiuiplus debuffs`. The page selects **Buffs** or **Debuffs**. These are independent HUD modules: enable either, move each in edit mode, and adjust its width, icon size, columns, spacing, offsets and opacity separately.

Each group offers an icon limit and overflow count, separate icon/row spacing, countdown visibility, crowd-control-only countdowns, optional heading/container and separate heading, timer and tooltip text profiles. Zero columns wraps at the available width. **Timer placement** offers Below icon, Above icon or Over icon. Text → Timers X/Y offsets fine-tune the countdown without moving the icons.

Countdowns show seconds alone at 60 seconds or less, `MM:SS` under an hour and whole hours from one hour: `61 → 01:01`, `3599 → 59:59`, `3600 → 1h` and `21420 → 5h`. Minutes are dropped while an hour or more remains. Duration estimates use the same format while keeping their range and uncertainty markers. Buffs/Debuffs reserve space using the configured timer font, scale and effects, so long timers wrap with their icons instead of shrinking independently. Hover and controller inspection use the same measured layout.

**Show preview**, under General, displays six sample buffs or debuffs and example countdowns in edit mode. The heading identifies preview content. Turning preview off shows actual effects or an empty-state label. Samples never enter gameplay data or render outside edit mode.

Debuffs → General → **Follow Buffs settings** inherits the Buffs icon, timer, text, heading and container settings as they change. Inherited controls display the Buffs values and are read-only in Debuffs. Position, width, enabled state and preview remain independent. Turning follow off restores the retained Debuffs settings; enabling it never copies over those preferences.

Shipped defaults use 12-pixel Buffs icons and white heading, timer and tooltip text with a stroke and no drop shadow. Debuffs follow Buffs styling; their position, width, visibility and preview remain independent.

Grouping uses effect-ID metadata shared with Party. It does not infer whether an effect is beneficial from its cancelability. Weakness is a debuff; food and Dread Spikes stay with buffs. Neutral and unrecognized effects remain visible with buffs.

## Party rows

Party/alliance **Text** settings also offer separate **Sync**, **HP values**, **MP values** and **TP values** profiles. Each controls its own font, size, colors, effects and offsets. Sync replaces the former Details label; existing saved styling is retained. MP and TP initially inherit the former Values styling, then save independently. Bar labels are the HP/MP/TP captions; values are the displayed numbers. TP styling applies to bars and number-only layouts.

**Jobs → Job placement → After name** is the default when job text is enabled. Jobs follow the displayed name, including its alignment and offsets. **Text → Jobs** styles job text; **Text → Zone** separately styles out-of-zone names. Changing Jobs placement, offsets or size leaves zone labels and their entry height unchanged.

Number-only TP shows the complete value and reserves enough width for four digits at the selected font/size, including the padding used by Left, Center and Right placement. **Cast placement → Replace name** starts the cast track at the name's actual position; its own X/Y adjustments apply relative to that name anchor.

**Text → Cast → Y offset** moves the cast label independently of the track; negative values move it upward. It does not change entry height or resource positions. **Jobs → Job icon X offset / Y offset** similarly moves either fixed or name-following icons without changing entry height, separately for Party and each alliance.

Effects belong to each party member. In the old full-party preview, the four extra buffs beside the second sample member were that member's examples, rather than another row of your character's buffs. Those implicit sample effects have been removed.

**Party → Status Effects → Preview status effects** explicitly shows sample buffs and debuffs while editing. It uses the selected ordering, placement, row visibility, limits and text styles. Sample countdowns follow the normal **Status timers** switches, so turning timers off hides them in the preview too. The Party heading identifies status preview content.

**Full party preview** still controls whether six party entries appear. With it off, status preview uses the visible members, or one sample entry when the party is empty. Preview effects replace display copies only; leaving edit mode restores the actual native effects and timers.

Party → **Status Effects** keeps the existing left/right/inside placement and status-container controls. **Status order** offers:

- **Native order:** preserve the client's order and the existing layout. This remains the default.
- **Show debuffs first:** put harmful effects first, retaining native order within each group.
- **Show buffs first:** put buffs and neutral/unrecognized effects first, retaining native order within each group.
- **Split buffs and debuffs:** display two separate rows. Use **Selected status row** to edit Buff row or Debuff row independently.

Split rows have separate icon sizes, columns, horizontal icon spacing, vertical row spacing, limits, alignment, offsets, opacity, countdown options and tooltips, plus **Split row gap** between the buff and debuff groups. Party → Text exposes separate **Buff timers and details** and **Debuff timers and details** profiles. The measured row height includes both groups so the next party member starts below them.

Party defaults use split buff/debuff rows beside each entry, with 12-pixel icons, status containers off and countdowns off. The former 10-pixel default increases once to 12; custom sizes and later changes stay intact. Split-row size controls accept 10–48 pixels; other status strips retain their 12-pixel minimum.

For **only debuffs**, choose **Split buffs and debuffs**, select **Buff row**, and turn off **Show selected row**. Keep Debuff row enabled. Hide Debuff row instead for only buffs. The hidden row leaves no empty gap, and preview follows the same visibility choices.

The status page shows only controls for the selected mode: one combined set in Native/Buffs first/Debuffs first, or the selected row's set in Split. Size, limit, columns, timers and tooltips appear once. Container opacity/padding are shared and labeled separately from icon/countdown opacity. Combined status rows also have independent vertical spacing, and their timer text offsets work.

For the combined modes, **Status timers** optionally overlays real local countdowns inside the icons. Other party members have no native durations to display. Existing Party status placement, containers and saved geometry remain available.

## Target and Enemy List

Their **Status Effects** controls set visibility, icon size, spacing, wrapping, limit/overflow, alignment, offsets, opacity and countdown presentation. **Unknown timer markers** can show `?` for a combat-observed effect with no usable database duration. Hiding a timer or marker only changes its presentation.

XIUI+'s effect database is the primary duration catalog for combat observations. A confirmed successful application starts its fixed countdown: Paralyze uses 120 seconds, Blind 180 seconds, and Blaze Spikes 180 seconds. Countdowns use plain labels such as `1:30`; the hover tooltip identifies an estimated duration. A later confirmed application refreshes the effect; damage, resists and no-effect messages do not. Observed wear-off, removal or death clears the effect immediately, even before the estimate ends. Estimated expiry clears the tracked icon too, preventing known mob buffs from remaining indefinitely when no wear-off message arrives.

Fixed spell, weapon-skill, ability, pet and additional-effect durations are embedded from the retained XIUI+ source snapshot. Per-effect fixed overrides are included; gear, merit, job-point and TP formulas and server overlays are omitted. Damage-only secondary effects are not inferred. Loading midway through a fight does not reconstruct an earlier application time. Target, Enemy List and Pet views share the same combat source; native local/party lists still determine which effects are actually present on those characters.

Target prefers a known native local/party snapshot, including an empty list, over combat history. Mob icons come from confirmed gain/removal observations; a damaging spell or weapon skill alone does not add an assumed effect.

Target mob-information groups have a stable anchor before the status grid. Gaining, losing or wrapping debuffs does not move behavior/link/detection icons. **Target → Cast Bar** draws the main target's cast as a separate bar immediately below HP, with its text centered on the bar. Width and paired X/Y offsets control that bar alone. **Cast position → Independent** enables its own draggable element and shared controller Movement; returning to Below Target retains the independent position for later. Cast starts, interruptions and completion do not change the positions of status icons, mob information or attached target views. Existing cast style, colors and text preferences remain available.

## Reading effect details

Hover an icon for its capitalized native name in bold, slightly brighter text, followed by its description. Estimated timers add the smaller note **Estimate: Effects may end early**. Effect IDs, remaining/unavailable-duration lines and combat-observation details are omitted. Tooltips fit their text up to 300 pixels wide, wrapping longer content. They render after HUD contents, stay within the viewport and yield to active configuration/input surfaces. Pointer hover and controller inspection use the same presentation.

Controller routes use the configured semantic bindings:

- Enter a Party row, then use **Next tab** to inspect its visible effects. **Actions** retains Tell composition.
- Enter Enemy List, then use **Actions** to inspect the focused enemy's visible effects.
- Use `/xiuiplus focus target`, `/xiuiplus focus buffs` or `/xiuiplus focus debuffs` for direct inspection, or choose an available group in `/xiuiplus focus`.
- Directions or previous/next tab move between effects. **Cancel** returns to the originating row or releases the standalone inspection surface.
- On your own Buffs/Debuffs, **Confirm** (normally A) or keyboard **Enter** requests removal of the selected effect when the client marks it cancellable. The effect remains visible until the server reports its removal. Held Confirm does not repeat the request; Cancel/B still leaves inspection.

Hovering and browsing never cancel an effect. Non-cancellable effects and remote party/enemy/target/pet inspection remain read-only. The normal game retains input outside deliberate XIUI+ focus. Cancellation uses the installed status resource's `CanCancel` flag from the [Ashita SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/main/plugins/sdk/Ashita.h) and the native request used by Ashita's [debuff addon](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/debuff/debuff.lua).

## What the client provides

During zoning or an incomplete login, the shared party source clears its transient records. Local and remote status snapshots must be initialized before use: zero-filled slots are unavailable, because effect ID 0 is KO and must not become 32 repeated icons. A real KO followed by empty-slot markers remains valid, as does an initialized empty list. Local invalid slot values also reject the snapshot. A validated `GetBuffs()` fallback can supply untimed effects while the paired status-icon/timer list is unavailable. Party, Target and Buffs/Debuffs all consume this shared result; there is no arbitrary post-zone delay.

Focused verification: `xiui_config_input_smoke --status-snapshots-only` passes for zero-filled buffers, real KO, initialized empty lists, invalid slots, extended IDs and decode bounds. The broader legacy `--party-only` check reaches an unrelated old module-owned visibility assertion (`event hiding applies to each list`); current event visibility lives in the shared runtime policy. The zoning update's runtime built at 12:50:45 and AgentLink confirmed its installation at 12:51:49 on October 16. Live zoning appearance remains for Cody to check.

**Your character:** `IPlayer::GetStatusIcons()` and `GetStatusTimers()` provide paired IDs and real expiration timestamps. XIUI+ converts their 60Hz timestamps with the validated client clock already used by EXP. Missing, hidden, expired or unavailable timestamps produce no invented countdown. Repeated effects retain their own paired deadlines.

**Other party members:** the native party status snapshot provides up to 32 effect IDs per member, including high bits and member identity. It supplies no expiration timestamps. Alliance members do not gain a fabricated status list or timers.

**Mobs and other entities:** the SDK exposes no complete arbitrary-entity buff/debuff list or remaining-duration container. XIUI+ uses confirmed combat observations and its imported duration catalog, clearing them on matching removal, death, lifecycle reset or estimated expiry. Joining a fight late or missing an observation can leave this list incomplete. Resource icons and descriptions do not prove an effect is active; an estimated deadline is not a server-provided expiry.

**Paralysis and Blindness:** their installed icon resources do not hide timers. Local-player countdowns use native expiries. Enemy timers use XIUI+ estimates after confirmed application; an observed removal clears the estimate immediately. Keep recast timing separate from effect expiry.

See [Status Effects](../modules/status.md), [Mob information data](../modules/mobinfo.md) and the [focused checks](../../tests/README.md) for current source ownership and validation. Native visual/gameplay and physical-controller behavior remain for Cody to check.
