# Native chat filters

Open **Chat → Native Filters** or **Chat 2 → Native Filters**. Each page starts on its corresponding native Log window and keeps its own group selection. **Native filter group** selects Chat Filters, Log: Window 1 or Log: Window 2; selecting a group does not change any filter or retarget the other page. Global Chat Filters remain shared game settings.

**Chat Filters** exposes all 55 entries from the installed client's menu, using its labels and supported choices. **Show** means the native filter is Off; **Hide** means it is On. Channels that support **Hold** retain that native behavior. Tell offers native notification and Hold separately or together. System levels are cumulative, matching the game: hiding a higher level also hides lower levels.

**Log: Window 1 / Window 2** exposes each window's 28 routing categories: chat channels, emotes, NPC messages, self/other recovery and damage, effects, and system messages. Enabled means that type appears in the selected native window. XIUI+'s **Messages** source then follows Window 1, Window 2 or their union.

For missing emotes, set **Chat Filters → Emotes → Show**, then enable **Log: Window 1 → Emotes** (or Window 2 for that destination). Expanded XIUI+ history also has an **Emotes** tab for incoming and outgoing emote text. A motion-only emote has no text to display.

For missing item-reward or inventory-full warnings, enable **Log: Window 1 > NPC conversations** (or Window 2 for that destination). FFXI routes general zone messages through this category, including "You cannot obtain the item. Come back after sorting your inventory." Enabling Basic system messages alone does not include them.

These controls use the client's configuration setter and read back the result. They refresh both windows from the game, preserve unrelated bits, and are not copied into XIUI+ portable profiles or changed by XIUI+ Reset defaults. Unavailable client bindings disable the affected controls. Changes affect newly received messages; existing scrollback retains its original routing. Native Hold affects the game's log behavior; XIUI+ keeps its own bounded scrollback.

`/xiuiplus chat status` reports native-filter availability and Window 1's Emotes routing.

Implementation: [native filter controls](../../src/runtime/chat/native_chat_filters.cpp), [validated client settings](../../src/runtime/ui/native_window_config.cpp), and [native admission](../../src/host/native_chat_source.cpp). The setting IDs and setter follow the [official Ashita Config addon](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/config/README.md); filter labels and definitions come from the running client's menu. Focused checks: `xiui_chat_smoke --native-filters-only` and `--history-only`.
