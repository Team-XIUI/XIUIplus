#include "localization/localization.hpp"
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
        module_state scale_state{true, {580, 320, 120, 24}, 32, 8};

        world_target_bar() { this->meter.height = 6; }
        auto ApplyTextDefaults(const ui::theme& theme) -> void { this->text = {{{theme.caption_text}, {theme.meter_text}}}; }
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        static auto Section(size_t index) -> size_t;
        auto Bounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto Draw(const hud_snapshot& snapshot, uint32_t id, D3DCOLOR nameColor, D3DCOLOR top, D3DCOLOR bottom, const module_render_context& context) -> void;
    };

    class world_target_bar_module final : public hud_module
    {
    public:
        explicit world_target_bar_module(world_target_bar& bar) : bar_(bar) {}
        auto Id(void) const -> const char* override { return "target.world"; }
        auto DisplayName(void) const -> const char* override { return "World Target Bar"; }
        auto State(void) -> module_state& override { this->bar_.scale_state.enabled = this->bar_.enabled; return this->bar_.scale_state; }
        auto State(void) const -> const module_state& override { return const_cast<world_target_bar_module*>(this)->State(); }
        auto CanMove(void) const -> bool override { return false; }
        auto ResetDefaults(const ui::theme&) -> void override {}
        auto Load(const settings_store&) -> void override {}
        auto Save(const settings_store&) const -> void override {}
        auto ContentOptionCount(void) const -> size_t override { return 0; }
        auto ContentOption(size_t) -> module_option override { return {}; }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return false; }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override
        {
            auto bounds = this->bar_.scale_state.bounds;
            bounds.width = this->bar_.width; bounds.height = this->bar_.meter.height + 20;
            this->bar_.scale_state.bounds.width = bounds.width; this->bar_.scale_state.bounds.height = bounds.height;
            context.DrawMeter(bounds, i18n::Text("World Target"), "75%", .75f, this->bar_.fill_top, this->bar_.fill_bottom,
                this->bar_.meter, this->bar_.animation, false, &this->bar_.text[0], &this->bar_.text[1]);
        }
    private:
        world_target_bar& bar_;
    };
}
