#pragma once

#include "modules/module.hpp"
#include "combat/combat_timing_source.hpp"

namespace xiui::ui
{
    struct player_timing_settings
    {
        static constexpr size_t option_count = 31;
        bool attack{true}, resting{true}, rounds{true}, hits{}, tp_time{true}, gains{true}, recovery_time{true}, hide_full{true}, flash{true}, resource_times{};
        int tp_goal{}, sound{}, display{}, direction{};
        int attack_layout{2}, mini_text{};
        int recovery_layout{2}, recovery_mini_text{};
        float goal{100}, height{6}, offset_x{}, offset_y{}, spacing{4}, ready_seconds{0.5f};
        float mini_height{3}, mini_gap{2};
        float recovery_mini_height{3}, recovery_mini_gap{2};
        D3DCOLOR attack_color{0xFF78C4EE}, rest_color{0xFFBFE07D}, ready_color{0xFFFFCA78};
        module_text_profile attack_text{theme::CreateDefault().caption_text}, rest_text{theme::CreateDefault().caption_text};
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        static auto RecoveryOption(size_t index) -> bool;
        auto Goal(void) const -> uint32_t { return static_cast<uint32_t>((this->tp_goal + 1) * 1000); }
    };

    struct enemy_timing_settings
    {
        static constexpr size_t option_count = 6;
        bool enabled{}, text_only{}, drain{};
        float height{4};
        D3DCOLOR color{0xFFFFCA78}, ready_color{0xFFFF9B8E};
        module_text_profile text{theme::CreateDefault().caption_text};
        auto Load(const settings_store& store, const char* section) -> void;
        auto Save(const settings_store& store, const char* section) const -> void;
        auto Option(size_t index) -> module_option;
    };

    auto TimingSections(bool recovery = false) -> std::span<const module_config_section>;
    auto TimingHeight(const module_text_profile& text, float height, bool textOnly, bool details = false) -> float;
    auto DrawTiming(const attack_prediction& timer, const char* name, const char* details, const rect& bounds,
        const meter_style& meter, const module_text_profile& text, float height, bool textOnly, bool drain,
        D3DCOLOR color, D3DCOLOR ready, float readySeconds, const module_render_context& context) -> float;
    auto DrawMiniTiming(const attack_prediction& timer, const char* details, const rect& bounds, const meter_style& meter,
        const module_text_profile& profile, int textMode, bool drain, D3DCOLOR color, const module_render_context& context) -> float;
    auto TPDetails(const tp_prediction& prediction, uint32_t goal, const player_timing_settings& settings, bool compact = false) -> std::string;
    auto RecoveryDetails(const recovery_clock& recovery, double now, const player_timing_settings& settings) -> std::string;
}
