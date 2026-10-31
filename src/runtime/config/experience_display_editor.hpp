#pragma once

#include "config_controls.hpp"
#include "modules/experience/experience_module.hpp"
#include "input/config_controller_focus.hpp"

namespace xiui::ui
{
    class experience_display_editor
    {
    public:
        enum class result : uint8_t { none, navigation, changed };
        auto View(void) const -> experience_display_view { return this->view_; }
        auto SelectView(experience_display_view view) -> void { this->view_ = view; }
        auto Target(const experience_module& module, const config_layout& layout, float x, float y) const -> uint32_t;
        auto Click(experience_module& module, const config_layout& layout, float x, float y) -> result;
        auto Activate(uint32_t target, experience_module& module, const config_layout& layout) -> result;
        auto ControllerItems(const experience_module& module, const config_layout& layout) const -> std::vector<config_controller_item>;
        auto RevealControllerTarget(uint32_t target, const experience_module& module, const config_layout& layout) -> void;
        auto Scroll(const experience_module& module, const config_layout& layout, float x, float y, int steps) -> bool;
        auto Draw(const experience_module& module, const config_controls& controls, const design_system& design, const config_layout& layout, float x, float y) const -> void;

        struct list { std::array<size_t, experience_readout_count> items{}; size_t count{}; };
        struct geometry
        {
            rect shown{}, available{}, actions{}, filters{}, details{}, presets{}, shown_title{}, available_title{};
            bool stacked{};
            int rows{};
            static auto MinimumHeight(float width) -> float { return width < 560 ? 246.0f : 196.0f; }
            explicit geometry(const config_layout& layout);
            auto Row(bool enabled, int index) const -> rect;
        };

    private:
        experience_display_view view_{};
        size_t selected_{experience_readout_count};
        size_t filter_{};
        int shown_scroll_{}, available_scroll_{};
        auto Items(const experience_module& module, bool shown) const -> list;
        auto Selected(const list& shown) const -> size_t;
        auto Reveal(const experience_module& module, const config_layout& layout) -> void;
        static auto First(int scroll, size_t count, int rows) -> int;
    };
}
