# Chat

Full keyboard mode opens XIUI+ composition directly on the first text key; Compact 1/2 retain native gameplay shortcuts. `/xiuiplus chat input` reports each native layout distinctly. Keep numpad movement, command modifiers, native editors and targeting outside implicit composition. Focused check: `xiui_chat_smoke --full-keyboard-only`.

The Chat setup page offers Follow game windows and One combined window through `SetWindowLayout`. Follow selects each native log's matching source and adopts its one/two-window count after a validated read succeeds. Combined selects All in the primary log and disables the second. The saved `chat.native_windows_initialized` marker keeps a selected combined layout and later customizations intact; older settings without the marker remain authoritative.

Foreground loss suspends input without clearing chat composition, caret, channel, recipient or the selected chat/status window. Each fresh opening of expanded history selects the first tab and resets its history-tab scroll positions to newest. F/Cycle Window selects chat over the combat command menu, then Enter/Confirm expands the log; Enter owns its release. Equipment/Storage and inventory Sort retain native cycling. `window-input` and `controller-input` cover these ownership boundaries.

Expanded history fills the current game viewport independently of the compact window's position. Capture the history viewport before applying the chat rendering transform, so the screen-to-module conversion happens once. Layout measures text under the same transform as rendering; channel-tab hit testing and controller focus share those bounds. Moving compact chat preserves the expanded log's screen position and the compact window's saved layout. Focused check: `xiui_chat_smoke --history-only`.

Two independently placed chat windows share captured messages, composition, reply history, alerts and color preferences. History browsing and typing have distinct ownership.

Chat decodes and sends FFXI's Western character extensions, including accented Latin-1 letters and Œ/œ, Š/š, Ž/ž and Ÿ. They retain their accents alongside Japanese and auto-translate tokens and consume two native bytes in the input budget. Existing Shift-JIS mappings remain preferred; unsupported characters still leave the draft unsent. The extension mapping follows [POLUtils' FFXI conversion table](https://github.com/Windower/POLUtils/blob/master/PlayOnline.FFXI/ConversionTables/85xx.dat). Focused check: `xiui_chat_smoke --encoding-only`.

The primary log's typing row reserves a narrow fixed left column for a channel tag of at most three characters (SAY, PTY, LS1, LS2, UNI, TEL, etc.). The entire row and pill animate in together and remain visible throughout composition, including an empty draft. Closing composition collapses the row completely, returning its space to the log. Clicking opens a mouse channel picker; matching release commits a choice, outside click or Escape cancels. Channel changes preserve the draft and caret, replace recognized chat prefixes and never rewrite utility commands. Tell/Reply use a known recipient or leave the draft intact. [chat_channel_picker.cpp](../../src/runtime/modules/chat/chat_channel_picker.cpp) owns this picker; existing controller composition retains its channel actions.

Ctrl/Alt + S, P, L, ; and T change the composing channel by scan code (AZERTY M is Linkshell 2) through `ChooseSendChannel`; `TakeChannelSave` lets the runtime persist it. Focused check: `--native-channel-only`.

Tab completion draws its panel with the window's own Background, Frame and Opacity; `DrawLog` clips rows under the list instead of painting an opaque box. The kind badge column is measured.

A native chat editor that stays open without handoff for 250 ms (Enter released) sets `native_chat_input_visible`, which draws only the native typing line while Chat Window 1 stays hidden. `/xiuiplus chat input` reports the last such editor's status and native state.

Opening composition or changing channels does not shift the draft or caret. Game mode reads the native send prefix; XIUI+ overrides and typed chat commands show their effective destination. Without a selected or readable native channel, the pill defaults to SAY. Clearing the draft preserves any chosen channel. The pill shares channel colors and chrome text effects; message sending and the right-side character counter retain their existing paths. Focused check: `xiui_chat_smoke --native-channel-only`.

Row, text and pill transitions share a 240 ms ease-in/out. `/r`, `/reply` and reply-shortcut drafts show REP in the tell color; direct tells retain TEL. Incoming tells do not relabel a reply already being composed.

Native Filters keeps separate view selections for Window 1 and Window 2, defaulting to each page's corresponding native Log window. Routing toggles update and read back only the selected native destination, independently of Match Window 1 appearance settings. Existing scrollback retains its original admission; native filter changes govern new messages.

The character counter shares the typing row, with reserved space on the right. Draft rendering, selection, caret measurement and pointer placement use the chat body's font and weight. The default XIUI+ face follows the appearance theme; explicitly selected faces retain their family and italic style in logs, typing, tabs and bubbles without losing themed text effects. Normal and expanded logs use a short top-edge alpha fade across glyphs, effects and auto-translate artwork; the container and typing area stay outside that fade.

Shipped defaults follow native Window 1 in the primary log and native Window 2 in the secondary log. For a single combined log, select Messages → All in Window 1 and turn off Second window. Its expanded history starts with an All tab that retains output from both native logs; channel tabs remain available. Changing the source invalidates the history cache without touching the draft.

Empty filtered and expanded logs stay blank. Startup guidance only appears in a compact log before any messages have been captured.

A compact log scrolled up (mouse wheel, Page Up) stays anchored while messages arrive, so it shows a persistent marker in its bottom-right corner: the unread count, or **Older messages** when nothing new has arrived. Clicking the marker, Ctrl+End or scrolling down returns to the latest line. Sending a message returns both windows to the latest line, and closing the expanded log returns that compact window; foreground cleanup that keeps the expanded log open keeps the position. `/xiuiplus chat scroll <rows>` scrolls the compact log for live checks (positive is older). Focused check: `chat-scrollback`.

The XIV 1.0 appearance uses the shared themed background and frame for compact and expanded logs, with outlined serif text, slightly wider tracking and slanted slate tabs. Appearance exposes independent Background and Frame toggles plus container opacity; Layout retains padding. These settings save per window and follow Match Window 1 when enabled. Its default palette uses gold for battle/system output, ivory for speech, pink for both tell directions, purple for emotes, lavender for notices, and peach for echo. Explicit channel and inline colors remain authoritative. `chat_theme.hpp` shares drawing, wrapping and caret metrics; theme changes invalidate wrapped rows without rewriting saved appearance or the native palette.

Each captured message stores its local clock time once, separately from the monotonic time used for animation and expiry. Row rebuilds, later native routing, both chat windows and expanded history format that saved value; they never reconstruct it from the current clock. Capture subtracts elapsed milliseconds before rounding to seconds, including across midnight. Focused check: `xiui_chat_smoke --timestamps-only`.

Log prose uses medium weight in XIV 1.0 and regular weight in XI, with bold speaker names, native inline highlights and fields from recognized English combat/loot/fishing messages. Regular XIV 1.0 prose fills receive a 25% lift toward white for readability, preserving channel hues, gradients, opacity and text effects; bold spans and timestamps keep their own color handling. Emotes emphasize their actor, with recognized targeted formats also emphasizing the recipient. Named fields get up to an eight-percent brightness lift after channel/inline color resolution, preserving hue, alpha and text effects. `chat/chat_emphasis.cpp` annotates captured messages once without rewriting text or source colors; both windows and expanded history share those spans. The shared glyph renderer caches weights separately, and wrapping measures the selected weight. Unknown messages retain the body weight. Focused check: `xiui_chat_smoke --emphasis-only`.

Compact F/Cycle Window navigation has one chat stop before Buffs/Debuffs. Expanded history has Window 1 and Window 2 tabs followed by all channels through Shout and Emotes. Window 2 is hidden when disabled, when the game uses one native window, or when no native message types route to it; unreadable native routing preserves an enabled window. Configured quiet logs remain available. Layout, mouse hit testing and keyboard/controller cycling share tab availability; removing the selected tab returns to Window 1. Native focus polling retains the expanded log and selected tab. Focused checks: `--history-only`, `--full-keyboard-only` and `window-input`.

The normal Window 2 frame and its input/edit controls follow the same native window-count and routing availability. Native Multi-Window Off hides them without rewriting XIUI+'s saved enable choice, size, filters or position; re-enabling native multi-window restores the configured window.

`KeyboardInputState` publishes selective navigation through the host/runtime API; `shared/keyboard_input.hpp` and `host/keyboard_suppression.hpp` reserve those physical keys before native DirectInput reads, including reads before WM delivery. The native `chatctrl` channel selector owns its navigation and confirmation, releasing XIUI+ log focus while preserving the draft and scroll. A native `fulllog` owner releases XIUI+ focus and temporarily restores its own visibility without editing saved preferences. Focused checks: `xiui_keyboard_suppression_smoke` and `window-input`.

**Bubbles** is the third Chat category. [chat_bubbles_module.cpp](../../src/runtime/modules/chat/chat_bubbles_module.cpp) owns overhead rendering, nearby speaker identity and lifetime; [chat_bubbles.cpp](../../src/runtime/chat/chat_bubbles.cpp) parses supported chat channels. Bubbles use live captured messages, shared channel colors, text profiles and native containers; they take no input focus and never display NPC menu options.

Bubbles have no edit-mode placeholder, frame, corner handle or movement-picker entry. Configure their appearance and scale through Chat → Bubbles; live overhead messages retain normal rendering and scaling.

Bubble anchors use the same validated native model render position as target rings, then apply the existing head-height offset. Movement coordinates can lead the visible mesh; unavailable or replaced actors draw no bubble instead of using a predicted anchor.

**Start in:** [chat_module.cpp](../../src/runtime/modules/chat/chat_module.cpp), [chat_history.cpp](../../src/runtime/modules/chat/chat_history.cpp), [native_chat_input.cpp](../../src/runtime/chat/native_chat_input.cpp), [hud_runtime.cpp](../../src/runtime/hud/hud_runtime.cpp).

**Keep intact:** Preserve native routing, drafts, caret and recipient during handoff. Native menu polling must preserve browsing focus in both windows. Retain NPC continuation markers without capturing native advance input. Alt+R/Ctrl+R open replies; sending Enter owns its release. Do not send twice, replay alerts, overwrite native editors, or let compact animation change saved bounds.

**Focused check:** `xiui_chat_smoke --bubbles-only`, `--menu-input-only` or `--dialogue-prompt-only`; choose the argument for the changed boundary.

Manual height changes override the reactive maximum line count while retaining collapse timing. Space and slash share sent history; empty native chat handoff preserves the unsent draft. Focused regression: `xiui_chat_smoke --sent-history-only`.

Window 2 can match Window 1's size, appearance and timing through General. Filters, tabs, visibility and position stay independent; inherited controls and resizing are disabled. Preserve Window 2's own settings on save so unlinking restores them. Focused regression: `xiui_chat_smoke --match-window-only`.

## Chat window behavior

Fresh defaults use 620×180 windows, starting with one. The first successful native Multi-Window read enables the second when appropriate. Follow game is the default for reactive sizing, colors, routing and timestamps. This initialization runs once; existing saved layouts and later customizations are preserved. See [Setup](setup.md).

To combine messages into one XIUI+ window, set **Chat → Window 1 → Messages** to **All** and disable **Second window**. The expanded log then starts with an **All** tab covering both native logs. Native routing and global filters retain their configured values.

**Chat → Window 2 → General → Match Window 1 settings** links the window's dimensions, background, padding, text, timestamps, animation and reactive sizing to Window 1. Inherited controls are disabled, and Window 2's resize grip is hidden. Visibility, position, message routing, tabs and native filters remain editable. The link follows later Window 1 edits and survives profile saves; turning it off restores Window 2's previous settings. Existing profiles keep independent settings until this is enabled.

Press **F** to cycle XIUI+ focus through **Chat 1 → Chat 2 → Buffs → Debuffs**, then back to Chat 1. Disabled/context-hidden chat windows and disabled or empty status groups are skipped. With Chat 2 disabled, the second F selects Buffs when effects are available. Status focus highlights the actual XIUI+ icons and opens their shared tooltip; arrow keys or Tab/Shift+Tab inspect effects, and Escape releases focus. The controller's window-cycle action follows the same sequence. **Enter** on a focused chat window expands its history; inside expanded history, **F / Shift+F** changes channel tabs. **Escape** returns to compact chat with the same window focused; a second fresh Escape leaves chat focus. Each chat window retains its own tab, scroll and the shared unsent draft. Native text entry, targeting and existing input owners retain priority.

**Chat / Chat 2 → Layout → Reactive window sizing** offers Follow game, Off and Custom. Follow game is the default and reads each native window's reactive resize switch, minimum and maximum lines, and resize delay. Window 2's inherited delay uses Window 1's value. Custom exposes Resize delay (seconds), Minimum lines and Maximum lines. These controls do not change the game's settings.

Resizing a window vertically, or changing its Height control, enables **Use window height for maximum**. Its saved height determines live message capacity instead of the native or custom maximum line count; minimum lines and timed collapse still apply. A width-only resize does not change this choice. Turn the toggle off, reselect Reactive window sizing, or edit Maximum lines to restore the explicit line cap.

Reactive windows grow for incoming messages. A new message restarts the idle delay; once it passes, the oldest visible message fades over 220 ms, and later messages retire at the configured interval. Wrapped rows stay together, including at the minimum-line boundary. History remains available after messages leave the compact view. Composition, history browsing and layout editing keep the window usable instead of collapsing its active controls. Saved window dimensions describe the expanded layout; collapse does not overwrite them.

**Chat → Layout → Shared window stack** optionally links two vertically stacked chat windows. The lower window retains its bottom anchor, and the upper window follows its changing top edge while keeping the saved gap. Either chat window can be above the other. Side-by-side windows retain their independent positions. The link is suspended for layout editing and resizing.

The compact frame is shared by rendering, pointer hit testing and controller focus. Shared stacking changes only the displayed position; saving or reloading does not accumulate offsets.

**Chat / Chat 2 → Animation → New messages** scrolls the whole message stack upward as new messages fade in. Travel follows each incoming message's full wrapped height; overlapping arrivals add their remaining motion so existing text does not jump. Outgoing rows stay visible until they slide past the top of the log. Fade + slide right also brings incoming text in from the left. Off keeps immediate placement. Each window retains its own animation speed, and scrolled or expanded history stays still.

Expanded/fullscreen history aligns messages to the bottom, with the newest visible message along the lower edge. It remains visible through Alt+Tab and foreground loss, including Windows' snipping overlay. The selected tab, scroll position and unsent draft are retained; mouse capture, composition and controller ownership are released while the game is inactive. Return to the game to continue browsing; Escape returns to compact chat focus, and Enter closes history.

Soft wrapping does not move trailing spaces onto an empty continuation row. Explicit blank lines and indentation remain available. Focused check: `xiui_chat_smoke --history-only`.

Native settings are read from the client's validated configuration records: Window 1 reactive/minimum/maximum/delay IDs 14/16/15/17, and Window 2 IDs 182/184/183/198. The [official Ashita config addon](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/config/README.md) documents these fields. Unavailable native settings retain a fixed window rather than guessing client values.

In both native windows, reactive value 0 means On, and Window 2 inherits Window 1's delay. The native values are authoritative; the whole-message fade presentation is XIUI+'s implementation.

## Chat input limit and counter

**Chat → Bubbles** sits beside Window 1 and Window 2. It enables overhead speech for Say, Linkshell 1, Linkshell 2, Party, Yell, Shout and NPC dialogue, with individual channel toggles. Only received messages from loaded speakers within the configured range appear; LS messages already follow the game's membership routing. NPC choices remain native menu entries. Bubbles also work when the chat windows are disabled.

The default range is 30 yalms and duration is 8 seconds, with at most 12 active speakers. Bubbles default to text only, a 360 px maximum width and 28 px of head spacing. Raising background opacity enables the native frame and tail. Bubbles ease in over 180 ms and out over 240 ms with a subtle upward slide; replacement messages crossfade. Anonymous NPC continuation chunks flow into the current bubble without restarting its entrance animation. Embedded line breaks remain, and long text wraps at the bubble width. Pending NPC dialogue stays visible until advanced. Zoning, hidden UI, disabled channels, out-of-range speakers and reused entity slots clear stale bubbles. Speakers behind the camera or outside the screen are not projected into the HUD. Appearance controls set width, head spacing, padding, background opacity and shared chat colors; Text retains the standard font and effects controls.

NPC dialogue shows a small filled down triangle with 2 px of left padding after the final wrapped row. The current prompt bounces/pulses while the game is in dialogue and chat is at its live bottom; older markers remain static. Explicit timed controls suppress the prompt. Pending dialogue pauses reactive collapse, and Enter/controller Confirm stays with the native game.

Native NPC redraws use mode 190 with the continuation control already removed, so chat reconstructs the cue for that mode and uses native dialogue state for animation. Modes 150/151 retain their explicit control handling. `/xiuiplus chat capture` reports the latest captured NPC mode, routing and final 32 raw bytes for focused diagnosis; it never advances dialogue.

With **Config → Misc. 2 → Keyboard Size → Full**, an ordinary letter, top-row digit or punctuation key opens XIUI+'s input directly and inserts the first character once. Space opens composition without adding a space to the draft. Numpad movement, function keys, Ctrl/Alt commands, native menus, targeting and other editors retain their input. Native chat drafts also transfer with their text and caret intact, without sending a message. Compact 1 and Compact 2 retain F-to-focus for chat; Full mode treats an unfocused F as the first character. Unavailable keyboard settings leave initiation with the game. F retains window/tab navigation after explicit chat/status focus. `/xiuiplus chat input` reports Full, Compact 1 or Compact 2 separately.

**Ctrl/Alt + S, P, L, ; and T** switch the draft to Say, Party, Linkshell, Linkshell 2 or Tell through the same path as the channel picker, preserving the draft and saving the send channel. They match physical key positions like the native chat line, so AZERTY's M is Linkshell 2. While the send channel follows the game, gameplay presses stay native and XIUI+ adopts the native mode; with an XIUI+ channel selected, they open composition on the chosen channel.

**Alt+R** and **Ctrl+R** open reply composition over a native command menu or target selection, using the same ownership as slash input. Enter sends once and keeps keyboard capture through its release, so that gesture cannot also open or confirm a native menu. A new Enter press after release returns to normal gameplay.

In Compact mode, **F** selects chat; pressing **F** again selects status effects. **Enter** opens the large log. Inside it, **F** cycles Window 1, Window 2 and every channel tab, including Shout and Emotes, before wrapping; **Shift+F** goes backward. Window 2 has its own native message routing in the large log and adds no extra compact focus stop.

Chat input stops accepting new text at the client's native send limit. Typing, clipboard paste and completions use the same outgoing command preparation as Send, including the chosen channel, tell recipient and leading auto-translate commands. Tokens and multibyte characters are kept whole. Oversized pastes insert only the portion that fits; rejected insertions leave a selection intact.

**Chat → General → Character counter** is on by default. It shows the current length and maximum just above the input on the right, and turns red at the limit. Turning it off hides the counter while keeping the limit enforced.

The counter follows the game's encoded length budget. Japanese characters and auto-translate tokens can consume multiple units. Hidden channel-prefix overhead is deducted from the displayed maximum; explicitly typed commands count as part of the input. The native limit is read from the client, with a 255-unit fallback when unavailable.

The counter reuses the chat chrome text profile, including its font and effects. Its row is reserved while the input is visible; history, completion lists, controller actions and error hints leave it room. It adds no input focus target. The toggle uses shared mouse, keyboard and controller settings controls, and persists with ordinary chat preferences and profiles.

Existing drafts restored across a reload remain editable, including an old draft above the current limit. New insertions cannot grow beyond the limit; the final Send check remains in place and rejects over-limit or unrepresentable text without sending it.

Focused checks: `xiui_chat_smoke --input-limit-only` and `--auto-translate-commands-only`.

## Native chat filters

Open **Chat → Native Filters** or **Chat 2 → Native Filters**. Each page starts on its corresponding native Log window and keeps its own group selection. **Native filter group** selects Chat Filters, Log: Window 1 or Log: Window 2; selecting a group does not change any filter or retarget the other page. Global Chat Filters remain shared game settings.

**Chat Filters** exposes all 55 entries from the installed client's menu, using its labels and supported choices. **Show** means the native filter is Off; **Hide** means it is On. Channels that support **Hold** retain that native behavior. Tell offers native notification and Hold separately or together. System levels are cumulative, matching the game: hiding a higher level also hides lower levels.

**Log: Window 1 / Window 2** exposes each window's 28 routing categories: chat channels, emotes, NPC messages, self/other recovery and damage, effects, and system messages. Enabled means that type appears in the selected native window. XIUI+'s **Messages** source then follows Window 1, Window 2 or their union.

For missing emotes, set **Chat Filters → Emotes → Show**, then enable **Log: Window 1 → Emotes** (or Window 2 for that destination). Expanded XIUI+ history also has an **Emotes** tab for incoming and outgoing emote text. A motion-only emote has no text to display.

For missing item-reward or inventory-full warnings, enable **Log: Window 1 > NPC conversations** (or Window 2 for that destination). FFXI routes general zone messages through this category, including "You cannot obtain the item. Come back after sorting your inventory." Enabling Basic system messages alone does not include them.

These controls use the client's configuration setter and read back the result. They refresh both windows from the game, preserve unrelated bits, and are not copied into XIUI+ portable profiles or changed by XIUI+ Reset defaults. Unavailable client bindings disable the affected controls. Changes affect newly received messages; existing scrollback retains its original routing. Native Hold affects the game's log behavior; XIUI+ keeps its own bounded scrollback.

`/xiuiplus chat status` reports native-filter availability and Window 1's Emotes routing.

Implementation: [native filter controls](../../src/runtime/chat/native_chat_filters.cpp), [validated client settings](../../src/runtime/ui/native_window_config.cpp), and [native admission](../../src/host/native_chat_source.cpp). The setting IDs and setter follow the [official Ashita Config addon](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/config/README.md); filter labels and definitions come from the running client's menu. Focused checks: `xiui_chat_smoke --native-filters-only` and `--history-only`.

[Shared HUD scaling](hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Chat reuses its top-right resize handle in edit mode; width and height change the viewport and reflow lines without changing the font. The toolbar/controller Resize action uses the same cancelable gesture. There is no second corner handle. Whole-chat scale remains uniform. Legacy unequal X/Y scales convert to viewport dimensions on load so text returns to normal proportions; preserve independent scale values when changing layouts or inheritance.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
