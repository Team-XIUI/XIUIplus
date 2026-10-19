# Chat

Two independently placed chat windows share captured messages, composition, reply history, alerts and color preferences. History browsing and typing have distinct ownership.

**Start in:** [chat_module.cpp](../../src/runtime/modules/chat/chat_module.cpp), [chat_history.cpp](../../src/runtime/modules/chat/chat_history.cpp), [native_chat_input.cpp](../../src/runtime/chat/native_chat_input.cpp), [hud_runtime.cpp](../../src/runtime/hud_runtime.cpp).

**Keep intact:** Preserve native routing, drafts, caret and recipient during handoff. Alt+R/Ctrl+R open replies; sending Enter owns its release. Do not send twice, replay alerts, overwrite native editors, or let compact animation change saved bounds.

**Focused check:** `xiui_chat_smoke --menu-input-only`; choose a narrower argument from its dispatcher for layout, filters or encoding.

[Detailed guide](../reference/chat-input.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
