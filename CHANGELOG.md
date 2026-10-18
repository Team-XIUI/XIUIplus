# Changelog

## Unreleased

## 1.0.3 — 2026-10-17

- Move Keybinds into the sidebar below Hotbars → Actions and Crossbar → Controller Setup. Selecting it opens the slot list directly; retain saved bindings, workspace bounds and existing topic selections.

- Add a Keybinds list for all ten hotbars and Crossbar, with per-slot key capture/clear, shared controller mapping dropdowns, bank previews, keyboard/controller navigation and an independent workspace. Retain existing bindings and local-only device mappings.

- Simplify shared combat-rule ownership and charged-recast helpers. Unify treasure batch eligibility and input checks, isolate edit samples, and avoid copying live lot history while rendering. Preserve profile keys and existing Lot/Pass behavior.

- Add Treasure Pool management with individual/batch Lot/Pass, native items/winners/history, estimated timers and shared mouse/keyboard/controller actions. Keep treasure alerts in Notifications.
- Add pinned Phoenix (75 cap) combat rules alongside Retail/Horizon: ordered skillchains, era roll powers/labels and ranged debuff estimates. Refresh mob data and spawn-slot hints, share charged-recast interpretation and add bounded distant party/alliance minimap markers.

- Put shared skillchain/MB prediction and server-rule controls in Action Bars → Hotbars/Crossbar → Skillchains, with a shortcut to the independent Magic Burst HUD editor. Preserve existing preferences and positions.

- Add shared skillchain tracking and next-action predictions for hotbars/crossbar, plus an independent Magic Burst HUD with elemental artwork, estimated countdowns, Retail/Horizon rules and shared text/settings controls.
- Add mouse-wheel zoom while hovering inside the Minimap, with per-zone persistence and a styled zoom readout in its bottom-right corner.
- Give the Minimap zoom readout its own visibility, text styling and X/Y position controls under Text → Zoom.

## 1.0.2 — 2026-10-17

- Reduce the standalone DLL size with WebP quality 90 maps and lossless compression for embedded icon atlases, cursors and mob data. Retain all regular/enhanced maps, original icon pixels and the single-DLL distribution.

## 1.0.1 — 2026-10-17

- Add Rename profile to the three-dot menu for custom profiles. Protect Default and preserve profile settings, positions and character assignments.
- Leave gameplay D-pad Up/Down with FFXI's native party/alliance targeting, removing automatic XIUI party browsing and its extra confirmation step.

## 1.0.0 — 2026-10-17

- Add Satchel with item actions, offline inventories and storage slips, alongside configurable hotbars, controller crossbars, macros, action sets and import workspaces.
- Use XIUI and Team XIUI throughout the interface, with `/xiui` as the primary command and existing command aliases retained.
- Hide settings that do not apply to the selected frame, layout or feature while preserving their saved values. Share visibility across layout, settings search and keyboard/controller navigation.
- Refine Controller Setup with themed dropdowns, sliders, compact spacing and correct popout hover cursors.

- Show predicted overhealing as a saturated pink overlay with dark diagonal stripes over existing HP on Player, Party/Alliance and World Target bars. Draw the overlay directly so the native gauge texture cannot mute it. Retain green useful-heal previews and add per-surface overheal visibility/color controls, including forecasts at full HP. Upgrade the former translucent pink default once while preserving custom colors.

- Dock the movement toolbar in a compact strip at the top center, with the selected element name, live coordinates and centering icons with hover help.

- Add Stacked, Single row and Independent World Info layouts, a separate Time toggle and individually movable weekday, clock, weather and date elements with saved positions and shared controller movement.

- Move Ready Check configuration into Extras, retaining direct commands and shared settings navigation.

- Give Minimap coordinates and the Map 1 label separate visibility controls; display grid positions in parentheses, such as (G-8). Preserve existing hidden labels and shared text styling.

- Batch Minimap dots with cached antialiased circle geometry, cache native map/artwork selection and skip entity scans while hidden. Single-map zones avoid terrain lookups; visible player movement retains per-frame updates.

- Add optional Minimap corner rounding with matching image/frame/marker clipping, plus a drop shadow with opacity, softness and X/Y offset controls. Persist both through normal settings and profiles.

- Brighten lost-HP trails to white, flash remaining HP with a warning tint and add shared yellow/orange/red warning fills below 75/50/25%. Retain gradient contrast, alpha and explicit Player state-color overrides.

- Add an optional fading Minimap vision cone with angle/size/opacity controls and green NPC dots with visibility/floor filtering.
- Collapse settings search into an icon that animates into the input. Retain Ctrl+F and the placeholder, add a typed-query clear X, and share Clear with controller Actions.

- Add a Minimap module with Square, Circle and Wide (16:9) shapes, bundled Regular/Enhanced maps, native floor/coordinate resolution, player and party/alliance/pet/target markers, per-zone zoom and shared presentation controls. Decode maps on demand in a worker and retain at most eight textures.

- Remember the selected shared UI profile per character/world. Restore on login, protect pending saves and unknown identity, retain chat drafts, handle missing/unsupported profiles and exclude local bindings from profile files and share keys.

- Apply native yellow/orange/red HP value zones below 75/50/25 percent across Player, Pet, Party and both alliances. Keep current HP percentages, text effects, bar styling and explicit Player custom-text overrides.

## 0.2.11 — 2026-10-16

- Add independently positioned notification groups with per-type routing, appearance, stacking direction, pinned invitations, compact rows and shared controller details.
- Add configurable HP damage/healing trails and flashes across health bars, with shared timing, colors and strength controls.
- Add Hide overhead names and a red Hide all UI action with recovery instructions, covering XIUI and the native HUD.
- Make Alt+R/Ctrl+R start with the most recent incoming teller, retain drafts while cycling, and restart the cycle when a new tell arrives. Restore incoming order across runtime reloads.
- Keep the same compact chat window focused after F → Enter → Escape. A second fresh Escape leaves focus; held Escape cannot dismiss both layers. Cycle F through both XIUI chat windows and available Buffs/Debuffs.
- Darken tooltips, correct overlapping nine-slice edges and clipped labels, and remove disabled modules from edit-mode selection.
- Add a distinct Pet icon, correct category selection, expose portrait backgrounds/overlays/positioning and clipping presets, and add optional Pet Target background tint.
- Reduce reload/startup hitches through shared resource caching and add startup timing diagnostics.

- Add Config text size (px) beside Config font in General → Appearance → Config Text. Apply size live across config controls and measurements, preserve heading/help proportions and pixel spacing, and save/reset it independently of HUD text.

- Keep Settings search visibly editable with a softly rounded inset field, muted placeholder, separate shortcut hint and retained query. Shared text inputs use the native control palette and an understated focus underline/caret instead of an outline stroke.

- Add cross-category Settings search with multiword matching, category/topic/bar/text paths, keyboard/controller result navigation and direct scroll/highlight. Build its catalog from an isolated preference snapshot; selecting a result navigates without editing values or enabling modules.

- Replace the header input-mode text button with the active keyboard/controller icon. Hover identifies the mode; the dropdown retains full Auto, Keyboard & Mouse and Controller labels.

- Add a saved icon-only Configuration sidebar, toggled by the top-left arrow or controller Utilities. Category names appear on hover/focus; content uses the reclaimed width. Profile selection, creation and actions remain available as compact icons, and responsive scrolling/focus stays synchronized.

- Suppress uninitialized status snapshots during zoning instead of displaying repeated KO icons. Validate local/party slot data at the shared source, retain genuine KO and known-empty lists, and preserve untimed fallback data without mismatching native timers.

- Keep expanded chat history open through Alt+Tab and foreground loss while releasing input, retaining its tab, scroll and draft. Prevent trailing whitespace from wrapping into empty message rows; preserve explicit line breaks and indentation.

- Add native-following reactive sizing to both chat windows, with custom idle delay/minimum/maximum lines and whole-message fades. Optional shared stacking keeps the upper window aligned as the lower one collapses, preserving saved positions and active chat/history controls.

- Make Configuration resizable from every edge/corner, with a themed grip and keyboard/controller resize actions. Settings, cards, selectors and custom lists reflow through shared geometry for rendering, clicks, scrolling and focus.

- Simplify status tooltips to a capitalized bold/brighter effect name, description and a smaller estimate note when applicable. Remove effect IDs, remaining/unavailable durations and combat-observation text.

- Status countdowns show plain numbers; the estimate label remains in the hover/controller tooltip.

- Use the original XIUI effect database as the primary duration catalog for confirmed combat effects. Target/Enemy/Pet countdowns identify estimates in their tooltips, expire against the imported durations and clear immediately on observed removal/death. Preserve exact local native timers; unknown effects stay untimed. Embed the source snapshot, attribution and license without a Lua runtime dependency.

- Keep Target mob-information icons anchored when statuses appear or wrap. Main-target casts now use a separate bar below HP with centered cast text, independent width/offsets and optional shared Movement; cast start/end no longer reflows other Target elements.

- Party/Alliance TP now fills once per 1,000 TP, overlaying the completed bar through 2,000 and 3,000 with the shared Player TP renderer. Numeric TP and existing settings remain intact.

- XIV Party preset now uses Cody's October 16 captured Party settings (593 preferences across 24 sections) through the same embedded full-preference path as Classic. Preserve each list's position when applying; existing configurations and global defaults are retained on reload.

- Exclude Party/Alliance job icons from entry width, content inset and fitted container calculations. Icon anchors and offsets still apply.

- Consolidated all Party/Alliance job text controls under Text → Jobs and renamed the icon-only category Job Icons. Renamed shared text Placement to Alignment. Existing settings, positions and rendering are retained.

- Party/Alliance job text can stack main job above subjob and anchor to either the displayed name or the actual HP fill start, retaining shared Jobs text effects/offsets and existing defaults.

- Removed the decorative tick marks around Target Rings.

- Target Rings now follow current native target name/claim colors by default while retaining configured opacity. Add separate NPC/player visibility switches, off by default, with decorative rings for NPCs without HP.

- Smooth Target Ring HP changes in both directions with the shared meter interpolation, and place its front triangle along the enemy's native facing direction. Reset animation for target changes and keep the confirmed rendering boundary intact.

- Fixed intermittent Target Ring movement jumps: capture the camera on actual world draws and project/clip the terrain geometry directly into scene coordinates, preserving native depth occlusion.
- Fixed invisible Target Rings by rendering onto the actual scene color/depth pair before native UI/compositing. Restore native stream/index bindings and target/viewport/transforms before resuming a native draw. Observe world draws before replacement renderers; host/runtime ABI 16.
- Renamed Target Bar to Target and added Target → Target Rings: terrain-clipped floor rings with inner/outer radius, color/opacity and optional native HP fill. Background DAT loading and depth-tested world drawing use host/runtime ABI 16.
- Cap chat typing/paste/completion at the native outgoing budget. Add a default-on Character counter above the input on the right, with a Chat → General toggle. Preserve whole Unicode characters/tokens and existing restored drafts.
- Classic party preset now disables SYNC text.

- Fixed Party → General preset rendering: Classic and XIV now use two bordered boxes in one row. Removed XIUI from the choices and split description lines cleanly.

- Enabled separate timestamp colors by default in Chat and Chat 2, including existing settings. Preserve chosen colors and later opt-outs; messages retain their native/custom colors.


- Added approximate Cure / Curaga HP prediction to Party/Alliance, Player Bar and World Target Bar, with cast recipient locking, Curaga area previews and shared potency adjustment. Host/runtime ABI 13 adds native party world anchors.

- Fixed native window borders by applying the client's DAT vertex-color/alpha conversion, confirmed against live Type 6 frame vertices. Restores the intended strength of its gold trim. Type 7's stock right edge now matches the left edge's intensity/blend after native color conversion, while retaining its own artwork. Other themes and customized edge colors remain intact. No opacity or layout preferences changed.

- Added Target → Mob Info sub-tabs for Common and ten separately configurable groups. Preserve the default combined layout, or choose separate rows/independent movement with group sizing, spacing, offsets and optional text overrides. Existing aggregate switches migrate without moving Target.
- Added optional Target → World Target Bar: a compact native HP meter beneath the target's projected ground position, with standard meter, text, color and offset controls. World projection uses the versioned host/runtime snapshot; development host/runtime must be installed together. Separated release import-library output to prevent host/release link collisions.

- Replaced Target's passive/aggressive, linking and detection text with the original XIUI/MobDB icons, including NM/true-sight variants. Removed the Detects heading; missing art retains readable fallback labels.
- Added an independent Animation speed (%) slider for Chat and Chat 2, from 25–300%. 100% preserves the existing 220 ms entrance; higher is faster. Timing and transition expiration share the setting, and existing directions remain unchanged.
- Added Chat/Chat 2 → Animation → New messages: Off, Fade + slide up (default) and Fade + slide right from the left. New live lines share a short entrance across wrapped text, timestamps and auto-translate art, clipped inside the log. Scrollback, expanded/replayed history and duplicate deliveries remain static; each window saves its own choice.
- Replaced Target mob-information damage/element/status labels with icon/value cells: native client element/status art, original XIUI/MobDB physical-damage symbols and distinct Light/Dark Sleep badges. Removed the HUD snapshot footer while retaining provenance/notices in the distribution.
- Added Pet/Pet Target with native vitals, observed status details, five category presentations, original available portraits, shared job recasts/Ready charges and snapped/independent target placement. Missing lifetime deadlines remain unavailable; optional elapsed time measures observation only.
- Integrated mob information into Target Bar using the pinned HeadsUp Phoenix dataset: optional name-level labels and information rows for detection, damage adjustments, ranks and immunities. Target/child views share validated server-ID/zone lookup. Removed the separate Mob Info module/category/movement/focus path; its saved display choices seed Target preferences without moving Target.
- Added Blue Magic Learned and Phantom Roll under Extras, with separate previews. Blue uses validated local learned events and original banner/audio; Roll tracks two seats, native deadlines and Double-Up while labeling potency and ordinary-die bust chance as base values.
- Added Target name/cast/lock and HP-percent switches, enemy ID suffixes, independent cast meter/text and optional shared entity palettes/HP gradients.
- Added embedded status/job icon themes and per-list status overrides. Background/cursor themes remain out of scope. Added custom Ready Check PCM WAV selection with reload/test and built-in fallback.
- Removed feature record 26 and refreshed the Markdown/HTML tracker: 15 of 26 records implemented. Feature enable states follow the latest captured live defaults; Mob Info and World Target Bar are enabled in the 22:40 capture. The settings inventory includes every registered module and alternate view; corrected Pet Target global-reset ordering and excluded a reserved empty Target control.
- Refreshed new-install and Reset to defaults settings from Cody's live October 15 configuration: all 7,301 saved UI preferences across 409 sections, current layout and theme. Session data, profile metadata and retired demo settings are excluded; existing saved configurations remain authoritative.
- Configuration remembers the last category and settings page across reopening and reloads, with General as the initial fallback. Navigation stays local to the installation rather than traveling in shared profiles.
- Fixed Player HP/MP fills after job changes by sharing Party's validated native resources. Stale previous-job maxima are rejected; confirmed full totals remain usable after damage or MP spending, and unknown maxima use native percentages without inventing totals.
- Added Player HP/MP display formats, independent HP/MP/TP label/value profiles, configurable HP state gradients and TP-ready/overlay/flash colors, preserving existing saved text through migration.
- Added a shared selected-spell MP cost overlay for Player and the actual local Party row, independent of Cast Cost visibility; Player Bar → General and Party → Resources control their own previews, both on by default. Uses native resource base MP cost without guessed modifiers.
- Added independent Subtarget settings and Attached/Independent placement, plus a Detached Target of Target window. Separate windows share mouse/controller movement, centering, visibility and committed-position persistence; linked settings remain available.
- Fixed Party/alliance job text to follow the rendered player name by default. Added independent Zone text controls so Jobs placement, offsets and typography no longer move or resize out-of-zone entries.
- Saved Cody's current Party settings as the complete Classic preset, including the latest job-icon offsets. Applying it preserves the selected Party/alliance list's position and native connections.
- Added paired Party/alliance job-icon X/Y offsets. Icon offsets and cast-text offsets move their artwork without changing entry height or resource positions.
- Fixed number-only TP clipping with explicit Left/Center/Right text placement by reserving its alignment padding as well as all four digits.
- Fixed Party name-replacement casting to use the name's actual alignment and offsets. Compact Classic rows share the name-following icon and TP gutter; out-of-zone panels retain their bounds. Number-only TP reserves four digits and shows the full value without an ellipsis.
- Aligned Player attack/resting ticker code and missing-key defaults with the shipped profile: Mini bar, with a two-pixel gap from TP/HP respectively. Saved custom preferences remain available.
- Audited settings across all 29 modules and alternate views. Standardized dimensions, offsets, spacing and text/effect ordering; width/height and X/Y controls share rows. Separated Party window settings from bar layout, and Enemy List target panels/card appearance from its main layout. Pointer, scrolling and controller navigation share the displayed order while saved settings and IDs remain stable.
- Added Party → Presets with Classic, XIUI and XIV preset buttons for the selected Party/alliance list. A themed Cancel/Apply warning appears before replacing custom settings; list positions and native source connections are retained. Classic follows compact native FFXI proportions with shorter right-aligned MP and small numeric TP on the left. All presets use regular game bars; XIV uses three horizontal bars. XIUI insets the name slightly into HP and replaces it with casting.
- Moved Reset to defaults into the three-dot menu with a Cancel-first confirmation. It now restores every XIUI module, setting and position from any category, including the shipped XIUI Bahnschrift fonts. Mouse, keyboard and controller share the action.
- Renamed Party Details text to Sync and split HP, MP and TP into independent Party/alliance text controls for font, size, colors, effects and offsets. Existing MP/TP appearance migrates from Values once; later edits remain independent.
- Removed Swing/Resting suffixes from sample Party member names.
- Added an explicit Party buff/debuff/timer preview for edit mode and removed the implicit four buffs on the second full-party sample. Preview uses display copies and preserves gameplay timer settings.
- Party status icons now default to 12 pixels with containers and timers off. Previews respect timer switches; hiding Buff row shows only debuffs.
- Exposed native Chat Filters and per-window Log routing in Chat/Chat 2, preserving unrelated settings. Added an Emotes history tab; native routing remains authoritative.
- Added a compact config scrollbar with thumb dragging, track paging and shared wheel/controller position. The rail and thumb use the active theme's panel, border and selected-control colors.
- Made status tooltips fit their text with a narrower width limit and wrapped details. Confirmed native item icons already retain white/blue quality borders, independently of decorative slot frames.
- All bars now selects editing scope without copying current values; a shared edit applies only its own setting.
- Fixed status timer text offsets and added Above/Below/Over icon placement. Added independent status row spacing and removed duplicate Party controls by showing settings for the current combined/split mode.
- Added edit-only Buffs/Debuffs previews and Debuffs → Follow Buffs settings, retaining independent position and saved preferences.
- Fixed mouse selection of the header input-mode dropdown when the controller is unavailable, and added a distinct Status Effects sidebar icon.
- Added Party status ordering and independently customizable buff/debuff rows, plus separate native Buffs and Debuffs HUD modules with movement, scaling and effect details.
- Added shared Target/Enemy status controls and native tooltips. Local countdowns use client timestamps; unavailable party/mob durations stay untimed. Removed obsolete estimated-duration rules.
- Added Enemy List columns, separate target panels, entry borders and click preferences.
- Consolidated contextual visibility in General → Visibility, with global/per-module settings and legacy preference migration.
- Allowed Party/alliance panels to reach the screen edge without reserving outside countdown text or edit padding.
- Fixed leading auto-translated emote/command tokens being sent as Say or other channel text.
- Added Separate timestamp color and a per-window timestamp color picker under Chat → Colors, preserving message colors and normal text effects.

## 0.2.10 — 2026-10-14

- Added Party/Alliance resting and swing tickers as strips inside the top of HP, with optional countdowns outside its right edge without shortening bars. Text-only Swing and Resting share the MP row below HP on its left, without pushing MP down. Party → Tickers controls visibility, observed member timing, size, colors, direction and independent text styling.
- Nearby party members use learned melee intervals and observed resting vitals; unsupported, stale and out-of-zone timing is hidden. Player rows reuse the existing attack/recovery clocks.

## 0.2.9 — 2026-10-14

- Draw controller focus after HUD contents so out-of-zone party panels cannot cover it.
- Added a shared two-pixel antialiased focus border with subtle glow and shadow. General → Appearance → Controller Focus provides a live preview and profile-persisted customization across HUD and config focus.
- Border and glow defaults match the config highlight color, using its native orange tint. Previous unmodified white/blue defaults migrate once; custom styling is preserved.

## 0.2.8 — 2026-10-14

- Controller callbacks now use cached ownership and a bounded action queue; UI actions run on the render thread instead of waiting for the runtime mutex inside native input callbacks.
- Keyboard state/data callbacks read cached focus without waiting for rendering. Queued controller actions expire after a stall and are discarded on suspension, profile changes, replacement and unload.
- Retained chat movement, native menu priority and the updated party focus frame from 0.2.7. Added a concurrency regression check with rendering deliberately blocked.

## 0.2.7 — 2026-10-14

- Chat focus and expanded history now keep controller movement/camera controls and unrelated keyboard movement available while capturing chat navigation. Config and modal controls retain exclusive capture.
- Replaced the party browsing outlines with one native selection frame and a subtle background tint. Removed the party helper window at the bottom of the screen.
- Native menus, including the main menu, inventory and equipment, take priority over party D-pad browsing using the live game menu-input state.

## 0.2.6 — 2026-10-14

- Added DirectInput and XInput controller navigation with configurable input presentation, visible focus and safe press/release handling.
- Y / Triangle focuses chat, Confirm expands it, and Y / Triangle cycles history tabs. Chat supports message/page scrolling, channel selection and explicit Send; typing currently uses a keyboard.
- D-pad Up/Down browses and cycles the party list directly from gameplay. Confirm targets the highlighted member, Cancel leaves browsing, and Y / Triangle switches to chat. Native menus and spell/ability targeting retain their controls.
- Added controller navigation for configuration, profiles, colors, EXP readouts, Enemy List, Ready Check and equipment tooltips.
- Added controller layout movement with grid snapping, fine steps, centering and Cancel to restore the original position.
- Preserved existing settings and shipped defaults. Distributed as one standalone x86 DLL.

## 0.2.1 — 2026-10-13

- Added separate player Cast Bar and Cast Cost modules, including native progress, selected-action costs, cooldowns and charge pools. Player MP now automatically hides at zero maximum MP, with an independent preference.
- Added native Ready Check prompts and party/alliance tracking, compatible party-chat replies, optional summaries, and a built-in chime with volume controls.
- Added inventory dots, equipped-item slots and gil tracking.
- Expanded target and enemy combat information, including status effects and cast progress.
- Added explicit EXP, Limit Points, Capacity Points and Exemplar Points display modes.
- Added independent notification controls for party invitations, trade requests, key items, gil and treasure, plus optional fading containers. Icons appear on the right and include XIUI's original invitation, trade and key-item artwork.
- Improved tell/reply input handling, camera-arrow and Escape passthrough, and preserved native chat colors and filters.
- Organized Player, Target, Enemy List, Chat and Extras configuration into secondary sections and removed empty categories.
- Preserved existing user settings and the captured default profile.

Distributed as one standalone x86 Ashita v4 plugin: `xiuinative.dll`.
