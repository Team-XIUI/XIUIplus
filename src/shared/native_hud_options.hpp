#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xiui
{
    // Macro replacement policy; separate from native HUD visibility preferences.
    inline constexpr uint32_t native_keyboard_macro_palette = uint32_t{1} << 31;

    enum class native_hud_element : uint8_t
    {
        player, party, alliance1, alliance2, target, castbar, status,
        compass, clock, connection, title, help, chat1, chat2, chatlog, count
    };

    struct native_hud_option
    {
        const char* key;
        const char* label;
        const char* description;
        const char* resource;
    };

    inline constexpr std::array<native_hud_option, static_cast<size_t>(native_hud_element::count)> native_hud_options{{
        {"player", "Player bars", "Native solo HP, MP, and TP.", "gaugewin"},
        {"party", "Party list", "Native party frame.", "partywin"},
        {"alliance1", "Alliance 1", "First alliance frame.", nullptr},
        {"alliance2", "Alliance 2", "Second alliance frame.", nullptr},
        {"target", "Target box", "Box only; overhead arrows stay visible.", "targetwi"},
        {"castbar", "Cast bar", "Native spell progress bar.", "casttime"},
        {"status", "Status icons", "Native buffs and debuffs.", "buff    "},
        {"compass", "Compass", "Native radar and direction display.", nullptr},
        {"clock", "Clock", "Native day and time display.", nullptr},
        {"connection", "Connection info", "Network frame, S/R, and arrows.", "netstat "},
        {"title", "Menu title", "Top menu title strip.", "titlewin"},
        {"help", "Help bar", "Top instructions strip.", "helpwind"},
        {"chat1", "Chat window 1", "Main chat log and typing input.", "logwindo"},
        {"chat2", "Chat window 2", "Second chat log.", "logwin2 "},
        {"chatlog", "Expanded chat", "Expanded native log window.", "fulllog "},
    }};

    constexpr auto NativeHudBit(const native_hud_element element) -> uint32_t
    {
        return uint32_t{1} << static_cast<uint8_t>(element);
    }

    struct native_hud_preferences
    {
        bool enabled{true};
        std::array<bool, native_hud_options.size()> shown = [] {
            std::array<bool, native_hud_options.size()> values{};
            values.fill(true);
            return values;
        }();

        auto HiddenMask(void) const -> uint32_t
        {
            uint32_t mask{};
            if (this->enabled)
                for (size_t index = 0; index < this->shown.size(); ++index)
                    if (!this->shown[index])
                        mask |= uint32_t{1} << index;
            return mask;
        }
    };
} // namespace xiui
