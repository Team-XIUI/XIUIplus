# Chat input limit and counter

With **Config → Misc. 2 → Keyboard Size → Full**, ordinary typing opens XIUI's visible input as soon as the native client accepts the first character. Native chat drafts transfer with their text and caret intact, without sending a message. Compact 1 and Compact 2 retain F-to-focus for chat; Full mode leaves an unfocused F available as the first character. Unavailable keyboard settings also leave it with the game. F retains window/tab navigation after explicit chat/status focus. Native search and other text editors keep their own input.

**Alt+R** and **Ctrl+R** open reply composition over a native command menu or target selection, using the same ownership as slash input. Enter sends once and keeps keyboard capture through its release, so that gesture cannot also open or confirm a native menu. A new Enter press after release returns to normal gameplay.

Chat input stops accepting new text at the client's native send limit. Typing, clipboard paste and completions use the same outgoing command preparation as Send, including the chosen channel, tell recipient and leading auto-translate commands. Tokens and multibyte characters are kept whole. Oversized pastes insert only the portion that fits; rejected insertions leave a selection intact.

**Chat → General → Character counter** is on by default. It shows the current length and maximum just above the input on the right, and turns red at the limit. Turning it off hides the counter while keeping the limit enforced.

The counter follows the game's encoded length budget. Japanese characters and auto-translate tokens can consume multiple units. Hidden channel-prefix overhead is deducted from the displayed maximum; explicitly typed commands count as part of the input. The native limit is read from the client, with the existing 255-unit fallback when unavailable.

The counter reuses the chat chrome text profile, including its font and effects. Its row is reserved while the input is visible; history, completion lists, controller actions and error hints leave it room. It adds no input focus target. The toggle uses shared mouse, keyboard and controller settings controls, and persists with ordinary chat preferences and profiles.

Existing drafts restored across a reload remain editable, including an old draft above the current limit. New insertions cannot grow beyond the limit; the final Send check remains in place and rejects over-limit or unrepresentable text without sending it.

Focused checks: `xiui_chat_smoke --input-limit-only`, `--auto-translate-commands-only`, and the shared settings/defaults checks. These cover exact native budgets, paste, selection replacement, Unicode/token boundaries, completion, counter placement, visibility persistence and reset.
