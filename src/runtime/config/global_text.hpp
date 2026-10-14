#pragma once

#include "settings/text_style_settings.hpp"

#include <array>

namespace xiui { struct module_option; }

namespace xiui::ui
{
    struct global_text_settings
    {
        static constexpr size_t option_count = text_property_count;
        text_style style{theme::CreateDefault().meter_text};
        std::array<bool, option_count> applied{};

        global_text_settings(void) { this->style.scale = 1; }
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        auto Apply(size_t index, text_style& target) const -> void;
    };
}
