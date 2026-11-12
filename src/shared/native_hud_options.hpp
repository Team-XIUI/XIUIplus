#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xiui
{
    // Macro replacement policy; separate from native HUD visibility preferences.
    inline constexpr uint32_t native_keyboard_macro_palette = uint32_t{1} << 31;
    inline constexpr uint32_t native_instant_macro_palette = uint32_t{1} << 30;
    inline constexpr uint32_t native_edit_windows = uint32_t{1} << 29;
    // Draws the native typing line while Chat Window 1 stays hidden.
    inline constexpr uint32_t native_chat_input_visible = uint32_t{1} << 28;

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
        {"player", "Player Bars", "Native solo HP, MP, and TP.", "gaugewin"},
        {"party", "Party List", "Native party frame.", "partywin"},
        {"alliance1", "Alliance 1", "First alliance frame.", nullptr},
        {"alliance2", "Alliance 2", "Second alliance frame.", nullptr},
        {"target", "Target Box", "Box only; overhead arrows stay visible.", "targetwi"},
        {"castbar", "Cast Bar", "Native spell progress bar.", "casttime"},
        {"status", "Status Icons", "Native buffs and debuffs.", "buff    "},
        {"compass", "Compass", "Native radar and direction display.", nullptr},
        {"clock", "Clock", "Native day and time display.", nullptr},
        {"connection", "Connection Info", "Network frame, S/R, and arrows.", "netstat "},
        {"title", "Menu Title", "Top menu title strip.", "titlewin"},
        {"help", "Help Bar", "Top instructions strip.", "helpwind"},
        {"chat1", "Chat Window 1", "Main chat log and typing input.", "logwindo"},
        {"chat2", "Chat Window 2", "Second chat log.", "logwin2 "},
        {"chatlog", "Expanded Chat", "Expanded native log window.", "fulllog "},
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
