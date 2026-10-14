#pragma once

#include "modules/module.hpp"

namespace xiui
{
    struct target_of_target_settings
    {
        static constexpr int inline_right = 0, inline_left = 1, below = 2, above = 3;
        bool enabled{true}, health{true}, percent{}, label{}, distance{};
        bool cast{}, statuses{}, attack{}, arrow{true}, name{true}, prefix{};
        bool follow_meter{true}, follow_fill{true}, follow_text{}, tint_name{true};
        float width{}, bar_height{}, gap{8}, offset_x{}, offset_y{};
        float arrow_size{24}, arrow_gap{8}, arrow_x{}, arrow_y{}, cast_height{8};
        int placement{inline_right}, alignment{1}, name_placement{}, selected_text{};
        D3DCOLOR fill_top{0xFF704242}, fill_bottom{0xFFFB9494}, arrow_color{0xFFFFFFFF};
        ui::meter_style meter{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 4> text{};
        static constexpr size_t base_options = 32;
        static constexpr size_t option_count = base_options + meter_style_option_count + text_profile_option_count;

        auto Load(const settings_store& store, bool showDistance, bool showPercent, bool showLabel) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        static auto ConfigSections(void) -> std::span<const module_config_section>;
        auto Contains(size_t index, size_t section) const -> bool;
        auto ShowArrow(void) const -> bool;
        auto BarHeight(float mainHeight) const -> float;
        auto Area(const ui::rect& module, float precedingHeight, float mainBarHeight = 20) const -> ui::rect;
        auto Arrow(const ui::rect& module, float mainBarHeight) const -> ui::rect;
    };
}
