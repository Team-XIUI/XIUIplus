# Chat window behavior

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

Soft wrapping does not move trailing spaces onto an empty continuation row. Explicit blank lines and indentation remain available. The focused `xiui_chat_smoke --history-only` check covers history retention, input release and whitespace wrapping.

Native settings are read from the client's validated configuration records: Window 1 reactive/minimum/maximum/delay IDs 14/16/15/17, and Window 2 IDs 182/184/183/198. The [official Ashita config addon](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/config/README.md) documents these fields. Unavailable native settings retain a fixed window rather than guessing client values.

Read-only inspection of the existing client dump corroborated reactive value 0 = On in both native window callbacks, and Window 2's delay inheritance. The native values are authoritative; the whole-message fade presentation is XIUI+'s implementation.
