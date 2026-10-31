#pragma once

#include "ui/combat_timing_display.hpp"

namespace xiui
{
    struct party_ticker_style
    {
        bool enabled{true}, other_members{true}, hide_full{true};
        int display{}, direction{};
        float height{3}, gap{2};
        D3DCOLOR color{0xFF78C4EE}, ready_color{0xFFFFCA78};
        module_text_profile text{ui::theme::CreateDefault().caption_text};
    };

    struct party_timing_settings
    {
        static constexpr size_t option_count = 10 + text_profile_option_count;
        std::array<party_ticker_style, 2> tickers{};
        int selected{};

        party_timing_settings(void) { this->tickers[1].color = 0xFFBFE07D; }
        auto Load(const settings_store& store, const char* module) -> void;
        auto Save(const settings_store& store, const char* module) const -> void;
        auto Option(size_t index) -> module_option;
        auto ApplyTextDefaults(const ui::theme& theme) -> void;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void;
        auto CountdownWidth(size_t ticker, const ui::text_style* resolved = nullptr) const -> float;
    };
}
