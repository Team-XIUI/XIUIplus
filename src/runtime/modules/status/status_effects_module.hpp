#pragma once

#include "modules/module.hpp"
#include "modules/party/party_source.hpp"
#include "ui/status_strip.hpp"

namespace xiui
{
    class status_effects_module final : public hud_module
    {
    public:
        explicit status_effects_module(bool debuffs = false);
        auto Id(void) const -> const char* override { return this->debuffs_ ? "debuffs" : "buffs"; }
        auto DisplayName(void) const -> const char* override { return this->debuffs_ ? "Debuffs" : "Buffs"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const party_source* source) -> void { this->source_ = source; }
        auto SetBuffsModule(status_effects_module* buffs) -> void { this->buffs_module_ = buffs; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return preference_options_start + (this->debuffs_ ? 2 : 1); }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto BeginControllerBrowse(void) -> bool;
        auto MoveControllerBrowse(int direction) -> bool;
        auto MoveControllerBrowseHorizontal(int direction) -> bool;
        auto EndControllerBrowse(void) -> void;
        auto ControllerFocused(void) const -> bool;
        auto ControllerBounds(void) const -> ui::rect;
        auto RenderStatusTooltip(const module_render_context& context, IResourceManager* resources, float x, float y, bool controller = false) const -> void;

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct status_module_test;
#endif
        static constexpr size_t appearance_options_start = ui::status_strip_settings::option_count;
        static constexpr size_t text_options_start = appearance_options_start + 6;
        static constexpr size_t preference_options_start = text_options_start + text_profile_option_count;
        bool debuffs_{};
        module_state state_{true, {24, 56, 300, 64}, 48, 24};
        const party_source* source_{};
        status_effects_module* buffs_module_{};
        bool preview_{}, follow_buffs_{};
        ui::status_strip_settings settings_{};
        bool heading_{true}, background_{};
        ui::container_style container_{1, 5, 6, false};
        std::array<module_text_profile, 3> text_{};
        int selected_text_{};
        bool controller_browsing_{};
        uint32_t controller_player_{};
        uint16_t controller_effect_{};
        size_t controller_occurrence_{};

        struct effect_list
        {
            std::array<combat_status, 32> entries{};
            size_t count{};
            uint32_t player{};
            auto Effects(void) const -> std::span<const combat_status> { return {this->entries.data(), this->count}; }
        };
        auto Section(void) const -> const char* { return this->debuffs_ ? "status.debuffs" : "status.buffs"; }
        auto LayoutSection(void) const -> const char* { return this->debuffs_ ? "layout.debuffs" : "layout.buffs"; }
        auto TextSection(size_t index) const -> const char*;
        auto StyleModule(void) -> status_effects_module& { return this->debuffs_ && this->follow_buffs_ && this->buffs_module_ != nullptr ? *this->buffs_module_ : *this; }
        auto StyleModule(void) const -> const status_effects_module& { return this->debuffs_ && this->follow_buffs_ && this->buffs_module_ != nullptr ? *this->buffs_module_ : *this; }
        auto Effects(bool editing = false) const -> effect_list;
        auto OuterInset(void) const -> float { const auto& style = this->StyleModule(); return style.container_.padding + (style.background_ ? style.container_.edge_size : 0); }
        auto HeadingHeight(void) const -> float;
        auto StripBounds(void) const -> ui::rect;
        auto Layout(const effect_list& effects) const -> ui::status_strip_layout;
        auto ControllerIndex(const effect_list& effects) const -> std::optional<size_t>;
        auto FocusEffect(const effect_list& effects, size_t index) -> bool;
    };
}
