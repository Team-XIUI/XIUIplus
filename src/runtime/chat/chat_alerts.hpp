#pragma once

#include "chat_model.hpp"
#include "settings/settings_store.hpp"
#include "ui/native_ui_sounds.hpp"

namespace xiui
{
    inline constexpr const char* chat_sound_names[]{"Off", "Menu chime", "Confirm", "Cursor", "ChatMon Tell", "ChatMon Talk", "ChatMon Emote", "ChatMon Examine", "ChatMon Invite"};
    inline constexpr ui::native_ui_sound chat_sound_values[]{ui::native_ui_sound::menu_open, ui::native_ui_sound::select, ui::native_ui_sound::hover,
        ui::native_ui_sound::chatmon_tell, ui::native_ui_sound::chatmon_talk, ui::native_ui_sound::chatmon_emote, ui::native_ui_sound::chatmon_examine, ui::native_ui_sound::chatmon_invite};

    class chat_alerts
    {
    public:
        std::array<int, std::size(chat_types)> channels{1};
        int keyword_sound{1};
        std::string keywords;
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto UpdateKeywords(void) -> void;
        auto Matches(const chat_line& line) const -> bool;
        auto SoundFor(const chat_line& line) const -> int;

    private:
        std::vector<std::wstring> terms_;
    };
}
