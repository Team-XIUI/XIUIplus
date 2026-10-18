#pragma once

#include "ui/focus_frame.hpp"

namespace xiui { class settings_store; struct module_option; }

namespace xiui::ui
{
    struct controller_focus_settings
    {
        static constexpr size_t option_count = 14;
        focus_frame_style style{};

        auto Reset(D3DCOLOR highlight) -> void;
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;

    private:
        D3DCOLOR default_color_{style.color};
    };
}
