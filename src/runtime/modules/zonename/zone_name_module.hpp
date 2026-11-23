#pragma once

#include "modules/module.hpp"
#include "modules/party/party_source.hpp"

#include <algorithm>
#include <string>

namespace xiui
{
    // offset is the slide in text heights: below the resting line on the way in and again on the way out.
    struct zone_name_frame { bool visible{}; float alpha{}, offset{}; };

    inline auto ZoneNameFrame(const uint64_t elapsed, const float in, const float hold, const float out) -> zone_name_frame
    {
        const auto seconds = static_cast<float>(elapsed) / 1000.0f;
        if (seconds < in)
        {
            const auto rest = 1 - seconds / in;
            const auto eased = 1 - rest * rest * rest;
            return {true, eased, 1 - eased};
        }
        if (seconds < in + hold) return {true, 1, 0};
        if (out <= 0 || seconds >= in + hold + out) return {};
        const auto progress = (seconds - in - hold) / out;
        return {true, 1 - progress * progress, progress * progress};
    }

    class zone_name_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "zonename"; }
        auto DisplayName(void) const -> const char* override { return "Zone Name"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const party_source* source) -> void { this->source_ = source; }
        auto Update(uint64_t now, bool configOpen) -> void;
        auto Replay(void) -> void;
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& settings) -> void override;
        auto Save(const settings_store& settings) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        enum option : size_t { enabled, width, fade_in, hold, fade_out, preview, base_options };
        module_state state_{true, {-1, -1, 720, 48}, 160, 16};
        const party_source* source_{};
        module_text_profile text_{};
        std::string name_{};
        float fade_in_{0.9f}, hold_{3.5f}, fade_out_{1.2f};
        bool playing_{}, preview_{};
        uint64_t started_{}, now_{};
        uint32_t zone_{};
        auto Frame(void) const -> zone_name_frame;
    };
}
