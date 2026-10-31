#pragma once

#include "modules/module.hpp"
#include "world/world_state.hpp"

namespace xiui
{
    class world_info_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "world"; }
        auto DisplayName(void) const -> const char* override { return "World Info"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto Update(const world_state& value) -> void { this->data_ = &value; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return 9 + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto HasEditableBounds(void) const -> bool override { return !this->Independent(); }
        auto PreviewVisible(void) const -> bool override { return this->state_.enabled && !this->Independent() && (this->time_ || this->day_ || this->weather_ || this->date_); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->PreviewVisible() && this->data_ != nullptr && (this->data_->time_valid || this->data_->weather_valid); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto ConfigScaleEditable(void) const -> bool override { return true; }
        auto ConfigScaleState(void) -> module_state& override { return this->Independent() ? this->parts_[this->SelectedPart()] : this->state_; }
        auto Independent(void) const -> bool { return this->layout_ == 2; }
        auto InitializeParts(void) -> void;
        auto SelectedPart(void) const -> size_t { return static_cast<size_t>(this->selected_part_); }
        auto SelectPart(size_t index) -> void { this->selected_part_ = static_cast<int>(index); }

    private:
        friend class world_info_part_module;
        struct part_content
        {
            std::array<char, 64> text{};
            size_t profile{};
            int element{-1};
            bool visible{}, doubled{};
        };
        module_state state_{true, {-1, -1, 160, 34}, 120, 16};
        const world_state* data_{};
        std::array<module_text_profile, 3> text_{};
        bool time_{true}, day_{true}, weather_{true}, date_{};
        int selected_text_{}, layout_{}, selected_part_{};
        std::array<module_state, 4> parts_{{
            {true, {-1, -1, 60, 16}, 16, 14}, {true, {-1, -1, 100, 16}, 16, 14},
            {true, {-1, -1, 160, 16}, 16, 14}, {false, {-1, -1, 120, 16}, 16, 14}}};
        auto Contents(void) const -> std::array<part_content, 4>;
        auto PartEnabled(size_t index) const -> bool;
        auto PartSize(const part_content& part, const module_render_context& context) const -> ui::point;
        auto DrawPart(const part_content& part, const ui::rect& bounds, const module_render_context& context) const -> void;
        auto RenderPart(size_t index, const module_render_context& context) -> void;
    };

    class world_info_part_module final : public hud_module
    {
    public:
        world_info_part_module(world_info_module& parent, size_t index) : parent_(parent), index_(index) {}
        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override { this->parent_.parts_[this->index_].enabled = this->parent_.PartEnabled(this->index_); return this->parent_.parts_[this->index_]; }
        auto State(void) const -> const module_state& override { return const_cast<world_info_part_module*>(this)->State(); }
        auto HasEditableBounds(void) const -> bool override { return this->parent_.Independent(); }
        auto PreviewVisible(void) const -> bool override { return this->HasEditableBounds() && this->parent_.PartEnabled(this->index_); }
        auto PositionBoundsReady(void) const -> bool override { return this->State().bounds.x >= 0 && this->State().bounds.y >= 0; }
        auto ResetDefaults(const ui::theme&) -> void override { this->State().content_bounds = {}; }
        auto ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void override;
        auto Load(const settings_store&) -> void override {}
        auto Save(const settings_store&) const -> void override {}
        auto ContentOptionCount(void) const -> size_t override { return this->parent_.ContentOptionCount(); }
        auto ContentOption(size_t index) -> module_option override { return this->parent_.Option(index); }
        auto ConfigSections(void) const -> std::span<const module_config_section> override { return this->parent_.ConfigSections(); }
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override { return this->parent_.ConfigSectionContains(section, index); }
        auto OnContentOptionChanged(size_t index) -> void override { this->parent_.OnOptionChanged(index); }
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override { this->parent_.RenderPart(this->index_, context); }
    private:
        world_info_module& parent_;
        size_t index_{};
    };
}
