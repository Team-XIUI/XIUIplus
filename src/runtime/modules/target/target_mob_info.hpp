#pragma once

#include "modules/module.hpp"
#include "modules/mobinfo/mob_database.hpp"

namespace xiui
{
    enum class mob_info_icon { none, physical, element, status, detection };
    struct mob_info_value
    {
        std::string label{}, value{};
        mob_info_icon icon{};
        uint16_t id{};
        int element{-1};
    };
    enum class mob_info_group : size_t { behavior, links, detection, physical, elements, element_ranks, status_ranks, immunities, server_id, respawn, count };
    constexpr size_t mob_info_group_count = static_cast<size_t>(mob_info_group::count);
    struct mob_info_row { std::string label{}; std::vector<mob_info_value> values{}; mob_info_group group{}; };

    struct mob_info_group_settings
    {
        enum { inline_row, own_row, independent };
        module_state state{true, {24, 218, 300, 24}, 64, 16};
        int placement{own_row};
        bool follow_text{true}, headings{true}, positioned{};
        float width{}, icon_size{}, cell_spacing{12}, row_spacing{2};
        module_text_profile text{ui::theme::CreateDefault().caption_text};
        ui::rect attached_bounds{};
    };

    struct target_mob_info
    {
        static constexpr size_t group_options = 8 + text_profile_option_count, option_count = 3 + group_options;
        bool enabled{};
        int selected_group{};
        float spacing{2};
        module_text_profile text{ui::theme::CreateDefault().caption_text};
        std::array<mob_info_group_settings, mob_info_group_count> groups{};

        target_mob_info();
        static auto GroupName(size_t index) -> const char*;
        static auto GroupId(size_t index) -> const char*;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void;
        auto ApplyTextDefaults(const ui::theme& theme) -> void;
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        auto GroupOption(size_t group, size_t index) -> module_option;
        auto OptionVisible(size_t index) const -> bool;
        auto OnOptionChanged(size_t index, const ui::rect& target) -> void;
        auto Rows(const mob_record* mob) const -> std::vector<mob_info_row>;
        auto Draw(const mob_record* mob, const ui::rect& area, const module_render_context& context, size_t onlyGroup = mob_info_group_count) -> float;
    };

    class target_module;
    // Movement adapters; configuration and persistence stay owned by Target.
    class target_mob_info_module final : public hud_module
    {
    public:
        target_mob_info_module(target_module& parent, size_t group) : parent_(parent), group_(group) {}
        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto HasEditableBounds(void) const -> bool override;
        auto PreviewVisible(void) const -> bool override;
        auto PositionBoundsReady(void) const -> bool override { return this->PreviewVisible(); }
        auto ResetDefaults(const ui::theme&) -> void override {}
        auto ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void override;
        auto Load(const settings_store&) -> void override {}
        auto Save(const settings_store&) const -> void override {}
        auto ContentOptionCount(void) const -> size_t override { return 0; }
        auto ContentOption(size_t) -> module_option override { return {}; }
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
    private:
        target_module& parent_;
        size_t group_{};
    };
}
