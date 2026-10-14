#pragma once

#include "native_ui_sounds.hpp"

namespace xiui::ui
{
    inline constexpr const char* sound_names[]{"Off", "Menu chime", "Confirm", "Cursor", "ChatMon Tell", "ChatMon Talk", "ChatMon Emote", "ChatMon Examine", "ChatMon Invite"};
    inline constexpr native_ui_sound sound_values[]{native_ui_sound::menu_open, native_ui_sound::select, native_ui_sound::hover,
        native_ui_sound::chatmon_tell, native_ui_sound::chatmon_talk, native_ui_sound::chatmon_emote, native_ui_sound::chatmon_examine, native_ui_sound::chatmon_invite};
}
