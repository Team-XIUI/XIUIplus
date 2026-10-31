#pragma once

#include "../module.hpp"
#include "notifications/notification_renderer.hpp"

namespace xiui
{
    struct notification_group
    {
        module_state state{true, {700, 500, 400, 110}, 220, 80};
        notification_style style{};
        module_text_profile text{style};
        bool screen_anchor{true};
        float offset_x{}, offset_y{};
        int anchor{1}, direction{};
        notification_draw_result drawn{};
    };

    class notification_module final : public hud_module
    {
    public:
        static constexpr size_t maximum_groups = 6;
        inline static constexpr const char* group_names[]{"Group 1", "Group 2", "Group 3", "Group 4", "Group 5", "Group 6"};
        notification_module(void);
        auto Id(void) const -> const char* override { return "notifications"; }
        auto DisplayName(void) const -> const char* override { return "Notifications"; }
        auto State(void) -> module_state& override { return this->groups_[0].state; }
        auto State(void) const -> const module_state& override { return this->groups_[0].state; }
        auto Expire(uint64_t now) -> void;
        auto Center(void) -> notification_center& { return this->center_; }
        auto Demo(uint64_t now) -> void;
        auto BeginRenderFrame(void) -> void { for (auto& group : this->groups_) group.drawn = {}; }
        auto IsScreenAnchored(void) const -> bool { return this->groups_[0].screen_anchor; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { for (auto& group : this->groups_) visit(group.text.style); }
        auto ResetDefaults(const ui::theme&) -> void override;
        auto ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void override
        { this->groups_[0] = static_cast<const notification_module&>(defaults).groups_[0]; }
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return 48; }
        auto ContentOption(size_t index) -> module_option override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto OnPositionChanged(void) -> void override { this->groups_[0].screen_anchor = false; }
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->GroupVisible(0); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override { this->RenderGroup(0, context); }
        auto Group(size_t index) -> notification_group& { return this->groups_[std::min(index, maximum_groups - 1)]; }
        auto Group(size_t index) const -> const notification_group& { return this->groups_[std::min(index, maximum_groups - 1)]; }
        auto ConfigScaleState(void) -> module_state& override { return this->Group(this->SelectedGroup()).state; }
        auto GroupCount(void) const -> size_t { return static_cast<size_t>(this->group_count_ + 2); }
        auto SelectedGroup(void) const -> size_t { return static_cast<size_t>(std::clamp(this->selected_group_, 0, static_cast<int>(this->GroupCount()) - 1)); }
        auto SelectGroup(size_t group) -> void { this->selected_group_ = static_cast<int>(std::min(group, this->GroupCount() - 1)); }
        auto GroupFor(notification_kind kind) const -> int;
        auto GroupVisible(size_t group) const -> bool;
        auto RenderGroup(size_t group, const module_render_context& context) -> void;
        auto DetailAt(float x, float y) const -> std::optional<notification_draw_result::hit>;
        auto Detail(uint64_t id) const -> std::optional<notification_draw_result::hit>;
        auto Details(uint64_t id) const -> std::string;
        auto FirstDetail(void) const -> uint64_t;
        auto NextDetail(uint64_t id, int direction) const -> uint64_t;

    private:
        std::array<notification_group, maximum_groups> groups_{};
        notification_center center_{};
        std::array<int, 9> routing_{};
        int group_count_{}, selected_group_{}, selected_kind_{};
        auto LoadGroup(const settings_store& store, size_t group) -> void;
        auto SaveGroup(const settings_store& store, size_t group) const -> void;
        auto Style(size_t group) const -> notification_style;
    };

    class notification_group_module final : public hud_module
    {
    public:
        notification_group_module(notification_module& owner, size_t group) : owner_(owner), group_(group) {}
        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override { return notification_module::group_names[this->group_]; }
        auto State(void) -> module_state& override { return this->owner_.Group(this->group_).state; }
        auto State(void) const -> const module_state& override { return this->owner_.Group(this->group_).state; }
        auto HasEditableBounds(void) const -> bool override { return this->group_ < this->owner_.GroupCount(); }
        auto PreviewVisible(void) const -> bool override { return this->HasEditableBounds() && this->owner_.State().enabled; }
        auto ResetDefaults(const ui::theme&) -> void override {}
        auto ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void override
        { const auto& source = static_cast<const notification_group_module&>(defaults); this->owner_.Group(this->group_) = source.owner_.Group(source.group_); }
        auto Load(const settings_store&) -> void override {}
        auto Save(const settings_store&) const -> void override {}
        auto ContentOptionCount(void) const -> size_t override { return 0; }
        auto ContentOption(size_t) -> module_option override { return {}; }
        auto OnPositionChanged(void) -> void override { this->owner_.Group(this->group_).screen_anchor = false; }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->owner_.GroupVisible(this->group_); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override { this->owner_.RenderGroup(this->group_, context); }
        auto Index(void) const -> size_t { return this->group_; }
    private:
        notification_module& owner_;
        size_t group_{};
    };
}
