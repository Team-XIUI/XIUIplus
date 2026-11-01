# Chat

Full keyboard mode opens XIUI composition directly on the first text key; Compact 1/2 retain native gameplay shortcuts. `/xiui chat input` reports each native layout distinctly. Keep numpad movement, command modifiers, native editors and targeting outside implicit composition. Focused check: `xiui_chat_smoke --full-keyboard-only`.

Fresh reset defaults use smaller 620×180 windows and Follow game reactive sizing. `chat_module::InitializeNativeWindows` adopts the native one/two-window count once, after a validated read succeeds. The saved `chat.native_windows_initialized` marker keeps later custom layouts intact; older settings without the marker remain authoritative.

Foreground loss suspends input without clearing chat composition, caret, channel, recipient or the selected chat/status window. Each fresh opening of expanded history resets its history-tab scroll positions to newest. F/Cycle Window selects chat over the combat command menu, then Enter/Confirm expands the log; Enter owns its release. Equipment/Storage and inventory Sort retain native cycling. `window-input` and `controller-input` cover these ownership boundaries.

Two independently placed chat windows share captured messages, composition, reply history, alerts and color preferences. History browsing and typing have distinct ownership.

For a single combined log, select Messages → All in Window 1 and turn off Second window. Its expanded history starts with an All tab that retains output from both native logs; channel tabs remain available. Changing the source invalidates the history cache without touching the draft.

The XIV 1.0 appearance uses the shared themed background and frame for compact and expanded logs, with outlined serif text, slightly wider tracking and slanted slate tabs. Appearance exposes independent Background and Frame toggles plus container opacity; Layout retains padding. These settings save per window and follow Match Window 1 when enabled. Its default palette uses gold for battle/system output, ivory for speech, lavender for both tell directions and emotes/notices, and peach for echo. Explicit channel and inline colors remain authoritative. `chat_theme.hpp` shares drawing, wrapping and caret metrics; theme changes invalidate wrapped rows without rewriting saved appearance or the native palette.

Log prose uses normal weight, with bold speaker names, native inline highlights and fields from recognized English combat/loot/fishing messages. Emotes emphasize their actor, with recognized targeted formats also emphasizing the recipient. `chat/chat_emphasis.cpp` annotates captured messages once without rewriting text or colors; both windows and expanded history share those spans. The shared glyph renderer caches actual regular/bold faces separately, and wrapping measures the selected weight. Unknown messages remain normal. Focused check: `xiui_chat_smoke --emphasis-only`.

Compact F/Cycle Window navigation has one chat stop before Buffs/Debuffs. Expanded history has Window 1 and Window 2 tabs followed by all channels through Shout and Emotes. Window 2 is hidden when disabled, when the game uses one native window, or when no native message types route to it; unreadable native routing preserves an enabled window. Configured quiet logs remain available. Layout, mouse hit testing and keyboard/controller cycling share tab availability; removing the selected tab returns to Window 1. Native focus polling retains the expanded log and selected tab. Focused checks: `--history-only`, `--full-keyboard-only` and `window-input`.

The normal Window 2 frame and its input/edit controls follow the same native window-count and routing availability. Native Multi-Window Off hides them without rewriting XIUI's saved enable choice, size, filters or position; re-enabling native multi-window restores the configured window.

`KeyboardInputState` publishes selective navigation through the host/runtime API; `shared/keyboard_input.hpp` and `host/keyboard_suppression.hpp` reserve those physical keys before native DirectInput reads, including reads before WM delivery. A native `fulllog` owner releases XIUI focus and temporarily restores its own visibility without editing saved preferences. Focused checks: `xiui_keyboard_suppression_smoke` and `window-input`.

**Bubbles** is the third Chat category. [chat_bubbles_module.cpp](../../src/runtime/modules/chat/chat_bubbles_module.cpp) owns overhead rendering, nearby speaker identity and lifetime; [chat_bubbles.cpp](../../src/runtime/chat/chat_bubbles.cpp) parses supported chat channels. Bubbles use live captured messages, shared channel colors, text profiles and native containers; they take no input focus and never display NPC menu options.

Bubbles have no edit-mode placeholder, frame, corner handle or movement-picker entry. Configure their appearance and scale through Chat → Bubbles; live overhead messages retain normal rendering and scaling.

**Start in:** [chat_module.cpp](../../src/runtime/modules/chat/chat_module.cpp), [chat_history.cpp](../../src/runtime/modules/chat/chat_history.cpp), [native_chat_input.cpp](../../src/runtime/chat/native_chat_input.cpp), [hud_runtime.cpp](../../src/runtime/hud_runtime.cpp).

**Keep intact:** Preserve native routing, drafts, caret and recipient during handoff. Native menu polling must preserve browsing focus in both windows. Retain NPC continuation markers without capturing native advance input. Alt+R/Ctrl+R open replies; sending Enter owns its release. Do not send twice, replay alerts, overwrite native editors, or let compact animation change saved bounds.

**Focused check:** `xiui_chat_smoke --bubbles-only`, `--menu-input-only` or `--dialogue-prompt-only`; choose the argument for the changed boundary.

[Detailed guide](../reference/chat-input.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

Manual height changes override the reactive maximum line count while retaining collapse timing. Space and slash share sent history; empty native chat handoff preserves the unsent draft. Focused regression: `xiui_chat_smoke --sent-history-only`.

Window 2 can match Window 1's size, appearance and timing through General. Filters, tabs, visibility and position stay independent; inherited controls and resizing are disabled. Preserve Window 2's own settings on save so unlinking restores them. Focused regression: `xiui_chat_smoke --match-window-only`.

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Chat reuses its top-right resize handle in edit mode; width and height change the viewport and reflow lines without changing the font. The toolbar/controller Resize action uses the same cancelable gesture. There is no second corner handle. Whole-chat scale remains uniform. Legacy unequal X/Y scales convert to viewport dimensions on load so text returns to normal proportions; preserve independent scale values when changing layouts or inheritance.
