# Chat input limit and counter

**Chat → Bubbles** sits beside Window 1 and Window 2. It enables overhead speech for Say, Linkshell 1, Linkshell 2, Party, Yell, Shout and NPC dialogue, with individual channel toggles. Only received messages from loaded speakers within the configured range appear; LS messages already follow the game's membership routing. NPC choices remain native menu entries. Bubbles also work when the chat windows are disabled.

The default range is 30 yalms and duration is 8 seconds, with at most 12 active speakers. Bubbles default to text only, a 360 px maximum width and 28 px of head spacing. Raising background opacity enables the native frame and tail. Bubbles ease in over 180 ms and out over 240 ms with a subtle upward slide; replacement messages crossfade. Anonymous NPC continuation chunks flow into the current bubble without restarting its entrance animation. Embedded line breaks remain, and long text wraps at the bubble width. Pending NPC dialogue stays visible until advanced. Zoning, hidden UI, disabled channels, out-of-range speakers and reused entity slots clear stale bubbles. Speakers behind the camera or outside the screen are not projected into the HUD. Appearance controls set width, head spacing, padding, background opacity and shared chat colors; Text retains the standard font and effects controls.

NPC dialogue shows a small filled down triangle with 2 px of left padding after the final wrapped row. The current prompt bounces/pulses while the game is in dialogue and chat is at its live bottom; older markers remain static. Explicit timed controls suppress the prompt. Pending dialogue pauses reactive collapse, and Enter/controller Confirm stays with the native game.

Native NPC redraws use mode 190 with the continuation control already removed, so chat reconstructs the cue for that mode and uses native dialogue state for animation. Modes 150/151 retain their explicit control handling. `/xiuiplus chat capture` reports the latest captured NPC mode, routing and final 32 raw bytes for focused diagnosis; it never advances dialogue.

With **Config → Misc. 2 → Keyboard Size → Full**, an ordinary letter, top-row digit or punctuation key opens XIUI+'s input directly and inserts the first character once. Space opens composition without adding a space to the draft. Numpad movement, function keys, Ctrl/Alt commands, native menus, targeting and other editors retain their input. Native chat drafts also transfer with their text and caret intact, without sending a message. Compact 1 and Compact 2 retain F-to-focus for chat; Full mode treats an unfocused F as the first character. Unavailable keyboard settings leave initiation with the game. F retains window/tab navigation after explicit chat/status focus. `/xiuiplus chat input` reports Full, Compact 1 or Compact 2 separately.

**Alt+R** and **Ctrl+R** open reply composition over a native command menu or target selection, using the same ownership as slash input. Enter sends once and keeps keyboard capture through its release, so that gesture cannot also open or confirm a native menu. A new Enter press after release returns to normal gameplay.

In Compact mode, **F** selects chat; pressing **F** again selects status effects. **Enter** opens the large log. Inside it, **F** cycles Window 1, Window 2 and every channel tab, including Shout and Emotes, before wrapping; **Shift+F** goes backward. Window 2 has its own native message routing in the large log and adds no extra compact focus stop.

Chat input stops accepting new text at the client's native send limit. Typing, clipboard paste and completions use the same outgoing command preparation as Send, including the chosen channel, tell recipient and leading auto-translate commands. Tokens and multibyte characters are kept whole. Oversized pastes insert only the portion that fits; rejected insertions leave a selection intact.

**Chat → General → Character counter** is on by default. It shows the current length and maximum just above the input on the right, and turns red at the limit. Turning it off hides the counter while keeping the limit enforced.

The counter follows the game's encoded length budget. Japanese characters and auto-translate tokens can consume multiple units. Hidden channel-prefix overhead is deducted from the displayed maximum; explicitly typed commands count as part of the input. The native limit is read from the client, with the existing 255-unit fallback when unavailable.

The counter reuses the chat chrome text profile, including its font and effects. Its row is reserved while the input is visible; history, completion lists, controller actions and error hints leave it room. It adds no input focus target. The toggle uses shared mouse, keyboard and controller settings controls, and persists with ordinary chat preferences and profiles.

Existing drafts restored across a reload remain editable, including an old draft above the current limit. New insertions cannot grow beyond the limit; the final Send check remains in place and rejects over-limit or unrepresentable text without sending it.

Focused checks: `xiui_chat_smoke --input-limit-only`, `--auto-translate-commands-only`, and the shared settings/defaults checks. These cover exact native budgets, paste, selection replacement, Unicode/token boundaries, completion, counter placement, visibility persistence and reset.
