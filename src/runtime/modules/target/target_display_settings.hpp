#pragma once

#include "modules/module.hpp"
#include "entities/entity_name_colors.hpp"
#include <limits>

namespace xiui
{
    struct target_display_settings
    {
        static constexpr size_t base_options = 20, layout_options = base_options + meter_style_option_count,
            option_count = layout_options + 4;
        bool name{true}, casts{true}, lock{true}, percent_players{true}, percent_mobs{true};
        bool custom_cast_meter{}, custom_cast_fill{}, entity_fills{};
        bool mob_level{};
        int id_format{}, fill_class{}, name_class{}, cast_placement{};
        float cast_height{8}, cast_spacing{3}, cast_width{}, cast_offset_x{}, cast_offset_y{};
        D3DCOLOR cast_top{0xFF704242}, cast_bottom{0xFFFB9494};
        ui::meter_style cast_meter{ui::theme::CreateDefault().meter};
        module_text_profile cast_text{ui::theme::CreateDefault().meter_text};
        std::array<D3DCOLOR, entity_class_count> fill_top{}, fill_bottom{};
        auto Load(const settings_store& store, D3DCOLOR top, D3DCOLOR bottom, const ui::meter_style& meter, const module_text_profile& text) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        static auto Section(size_t index) -> size_t
        {
            if (index == base_options + 1) return std::numeric_limits<size_t>::max();
            if (index == 19) return 11;
            return index <= 5 ? 0 : index <= 11 || index >= base_options ? 9 : 10;
        }
    };
    auto TargetIdSuffix(uint32_t serverId, int format) -> std::string;
}
