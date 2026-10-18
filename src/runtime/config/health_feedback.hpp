#pragma once

#include "modules/module.hpp"
#include "ui/health_feedback.hpp"

namespace xiui
{
    struct health_feedback_settings : ui::health_feedback_style
    {
        static constexpr size_t option_count = 11;
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
    };
}
