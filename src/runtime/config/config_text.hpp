#pragma once

#include "design_system.hpp"

namespace xiui
{
    class settings_store;
    struct module_option;
}

namespace xiui::ui
{
    struct config_text_settings
    {
        static constexpr size_t option_count = 15;
        int font{};
        float letter_spacing{};
        shadow_effect shadow{true, 1, 1, 0xEE000000, false};
        int outline{1};
        D3DCOLOR outline_color{0xF0000000};
        glow_effect glow{};
        sheen_effect sheen{};

        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
    };
}
