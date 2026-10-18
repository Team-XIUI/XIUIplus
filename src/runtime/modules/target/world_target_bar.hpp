#pragma once

#include "modules/module.hpp"
#include "modules/healing/heal_prediction.hpp"

namespace xiui
{
    struct world_target_bar
    {
        static constexpr size_t base_options = 10, option_count = base_options + meter_style_option_count + text_profile_option_count + heal_preview_settings::option_count;
        static constexpr size_t heal_options_start = option_count - heal_preview_settings::option_count;
        heal_preview_settings heal_preview{};
        heal_prediction_source* healing{};
        bool enabled{}, name{}, percent{}, follow_fill{true};
        float width{120}, offset_x{}, offset_y{10};
        D3DCOLOR fill_top{0xFF704242}, fill_bottom{0xFFFB9494};
        ui::meter_style meter{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 2> text{{{ui::theme::CreateDefault().caption_text}, {ui::theme::CreateDefault().meter_text}}};
        int selected_text{};
        uint32_t target_id{};
        meter_animation_state animation{};

        world_target_bar() { this->meter.height = 6; }
        auto ApplyTextDefaults(const ui::theme& theme) -> void { this->text = {{{theme.caption_text}, {theme.meter_text}}}; }
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        static auto Section(size_t index) -> size_t;
        auto Bounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto Draw(const hud_snapshot& snapshot, uint32_t id, D3DCOLOR nameColor, D3DCOLOR top, D3DCOLOR bottom, const module_render_context& context) -> void;
    };
}
