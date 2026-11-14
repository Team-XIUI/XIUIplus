# Party and Alliance

One module family renders Party and both Alliance groups, with resource bars, jobs, casts, status effects and optional world anchors.

**Start in:** [party_source.cpp](../../src/runtime/modules/party/party_source.cpp), [party_module.cpp](../../src/runtime/modules/party/party_module.cpp), [party_timing_source.cpp](../../src/runtime/combat/party_timing_source.cpp).

**Keep intact:** Revalidate member identity at activation and keep out-of-zone data explicit. Gameplay Up/Down remains native targeting. Jobs text belongs under Text → Jobs; job icons do not shrink the bar canvas. Movement clamps and snaps the party panel; external status strips and timers do not reserve screen-edge space. Presets preserve saved positions.

**Focused check:** `./scripts/check.ps1 -Name party-entry`; choose `party-presets` or `party-name-cast` for those boundaries.

Classic, XIV and fresh-install defaults show job icons with job/subjob text disabled. Advanced Text → Jobs can enable the text again.

Party/alliance leader dots use the existing leader server IDs and draw in the reserved gap immediately before the name, after job artwork so icons cannot cover the marker. Each list's General → Show party leader toggle starts enabled and persists as `show_leader`; disabling it also removes the reserved name gap.

Your solo row shows the native green seeking-party icon while the validated entity's client name flags contain `0x00100000`, including self-targeting with Show when solo enabled. The client's bit layout differs from packet `0x00D`. Seeking clears when the flag is removed or party/alliance membership appears, even if the entity update is delayed. Seeking and leader badges reserve separate space before the displayed name, keeping following job icons clear; XI and XIV 1.0 share the native status artwork.

Alliance General → Copy Party settings / Copy Alliance N settings replace that list's preferences through `CopyPreferences`, keeping position, scale, visibility and Hide in preview. Any new per-list pointer the runtime wires in must be preserved there and in `ResetDefaults`.

Selection → Width adjustment / Height adjustment resize the target and subtarget box around the entry's center, independently for Party and both Alliance lists. Values add to the total frame size; positive values extend toward the window edges, negative values shrink it. Defaults are zero. These controls do not change content widths, row spacing or member hit targets; padding still reserves space for content. Settings persist as `selection_width_extension` / `selection_height_extension` in each list's section.

Native party rearrangement and member-selection menus mirror the client's party-slot cursor, including out-of-zone members. The selection field at the party owner's `+0x54` is a callback address: any nonzero value enables the signed slot at `+0x50`, validated to 0–17. XIUI+ only mirrors the highlight; the client owns navigation, confirmation and party order. Closing the native selector clears its highlight. `party-entry` covers callback-valued selection, cursor movement, invalid slots and dismissal.

When inspecting a treasure item, the name row reserves a fixed-width decision badge before TP/details. It follows the Treasure Pool's highlighted item and Show member decisions toggle, using native member lots for the party and both alliance groups. Values changing between waiting, passed and a roll do not move names or alter row heights. The existing Details text profile supplies its font and effects.

The global XIV 1.0 theme renders transparent entries with aligned 12-pixel gauges, champagne-gold labels and rim-overlapping lining italic values. Enabled job icons use the bundled FFXIV-1 artwork beside names, retaining the shared icon size and position controls. Other appearances follow the global job icon theme. Number-only TP sits at the right of the name row above MP, reserving room so names, casts and detail text cannot overlap it. Bar TP uses one blue 0–3000 track with threshold marks. TP has no text label in either theme. HP width fits the equal-width pair; HP X/Y offsets move their common row, with Y kept below the name. Independent MP placement remains saved for Native. Shared resource geometry bounds legacy text offsets near the rim and keeps HP/MP values on one baseline. The theme always includes its small HP/MP resource labels; Text → Bar labels profile visibility still applies. Native's Bar labels toggle and saved text offsets remain independent. The transient theme flag feeds row, pointer, controller and status layout without changing saved styles, positions or formats.

Name-row casting reserves the combined name and cast footprint while casts are enabled. Switching between idle and casting keeps member heights, resource positions and top/bottom panel anchors fixed; cast text offsets remain visual adjustments. XIV 1.0 places the name-row cast four pixels lower within that shared layout.

XIV 1.0 keeps the name header compact while measuring the complete icon footprint independently. Names sit four pixels left of their saved anchor; the smaller name/icon gap retains icon positions. The resource row starts directly after the header without an extra spacer.

Full party preview marks its first row as the sample leader, including when that row uses a live member. The Show party leader toggle applies immediately. Basic preset cards also include a sample leader.

Classic uses full-width HP above TP on the left and MP on the right, without a left TP column. Its name/icon offsets start the icon at the content edge. A versioned migration corrects only the old Classic spacing signature; custom resource layouts and moved panels retain their preferences. The older MP below HP, TP left layout remains available independently.

## Party job text

Open **Party → Text** and select **Jobs** (the same path exists for either Alliance list). All job text controls live here: Show job text, main/subjob/Master Level switches, stacking, anchor, name placement, font, size, alignment, X/Y offsets, colors and effects.

- **Stack job and subjob:** puts the main job above the subjob. For example, `WHM99/RDM49 ML20` becomes `WHM99 ML20` above `RDM49`. The main/subjob level and Master Level switches still apply. Hiding Subjob removes the second line.
- **Anchor:** choose **Name** to retain After name / Below name placement, or **HP bar start** to follow the actual HP fill origin, including bar position and X/Y offsets. Name placement is disabled while anchored to HP.

The font, size, color, effects, Alignment and X/Y offsets below apply to both lines and either anchor. Name movement does not move HP-anchored text. Stacked text reserves its visible height, and remote members retain their separate Zone text layout.

**Party → Job Icons** contains icon visibility, size, anchoring and X/Y offsets only. Text controls are not duplicated there. Icons do not reserve horizontal entry space, shrink the bar canvas or widen the fitted window; their anchors and offsets control drawing independently. The shared text Alignment control describes Left/Center/Right within the chosen anchor; Name placement chooses After name / Below name.

Configurations that do not set these options use inline, name-anchored text. Party/Alliance settings save `stack_jobs` and `job_anchor` independently, including profiles, resets, preference copies and presets. The controls use the shared mouse, keyboard and controller configuration paths.

**Party → Layout → Resource layout → Classic (HP above MP and TP)** places HP across the top, with TP below left and MP below right. Classic presets use this layout and position the job icon at the normal content edge, with no empty TP gutter. Settings saved with the earlier Classic spacing are corrected once via `classic_layout_version`; custom layouts retain their values. The separate **MP below HP, TP left** choice keeps its left TP column.

Focused check: `xiui_config_input_smoke --party-entry-only`.

## Party presets

Open **Party → General** and scroll below the regular settings to choose one of two clickable boxes. They appear side by side when space permits and stack in a narrow configuration window:

- **Classic:** an FFXI-style party setup, with HP above MP, small TP text, SYNC text off, and the Classic text, job-icon and status layout. It is stored in `config/party-classic.ini`.
- **XIV:** horizontal HP/MP/TP bars, custom name and HP-anchored job text, job-icon offsets, status rows and all other Party presentation preferences. It is embedded from `config/party-xiv.ini`.

Each box describes its layout. Clicking anywhere in a box opens a confirmation dialog with **Cancel** selected. Explicit **Apply** replaces the custom appearance settings of Party and both Alliance lists. Every list retains its own position. Other modules and the shared healing potency adjustment keep their settings.

Saved navigation to the earlier separate Presets pages opens Party → General; saved Window navigation opens Window. Opening this page does not change existing configurations.

Mouse, keyboard and controller use the same action and confirmation paths. The cards use the normal native config theme, text settings, focus and scrolling. Results save through ordinary settings and profiles; a preset is applied only through explicit **Apply**.

Updating a preset's file changes the preset only; it is not applied to existing Party/Alliance configurations on reload. Focused check: `xiui_config_input_smoke --party-presets-only config/party-classic.ini config/party-xiv.ini`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
