# Chat

Two independently placed chat windows share captured messages, composition, reply history, alerts and color preferences. History browsing and typing have distinct ownership.

**Bubbles** is the third Chat category. [chat_bubbles_module.cpp](../../src/runtime/modules/chat/chat_bubbles_module.cpp) owns overhead rendering, nearby speaker identity and lifetime; [chat_bubbles.cpp](../../src/runtime/chat/chat_bubbles.cpp) parses supported chat channels. Bubbles use live captured messages, shared channel colors, text profiles and native containers; they take no input focus and never display NPC menu options.

**Start in:** [chat_module.cpp](../../src/runtime/modules/chat/chat_module.cpp), [chat_history.cpp](../../src/runtime/modules/chat/chat_history.cpp), [native_chat_input.cpp](../../src/runtime/chat/native_chat_input.cpp), [hud_runtime.cpp](../../src/runtime/hud_runtime.cpp).

**Keep intact:** Preserve native routing, drafts, caret and recipient during handoff. Retain NPC continuation markers without capturing native advance input. Alt+R/Ctrl+R open replies; sending Enter owns its release. Do not send twice, replay alerts, overwrite native editors, or let compact animation change saved bounds.

**Focused check:** `xiui_chat_smoke --bubbles-only`, `--menu-input-only` or `--dialogue-prompt-only`; choose the argument for the changed boundary.

[Detailed guide](../reference/chat-input.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
