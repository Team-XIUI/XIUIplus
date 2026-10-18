#include "party_timing_settings.hpp"
#include "ui/text_rasterizer.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    namespace { constexpr const char* names[]{"swing", "resting"}; }

    auto party_timing_settings::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        for (auto& ticker : this->tickers) { ticker.text = {theme.caption_text}; ticker.text.style.scale = 0.75f; }
    }

    auto party_timing_settings::VisitTextSettings(const text_settings_visitor& visit) -> void
    { for (auto& ticker : this->tickers) visit(ticker.text.style); }

    auto party_timing_settings::Load(const settings_store& store, const char* module) -> void
    {
        for (size_t index = 0; index < this->tickers.size(); ++index)
        {
            auto& ticker = this->tickers[index];
            const auto section = std::string{"timing."} + module + "." + names[index];
            const auto read = [&](const char* key, const float fallback, const float minimum, const float maximum) {
                const float value = store.ReadFloat(section.c_str(), key, fallback);
                return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
            };
            ticker.enabled = store.ReadBool(section.c_str(), "enabled", true);
            ticker.other_members = store.ReadBool(section.c_str(), "other_members", true);
            ticker.hide_full = store.ReadBool(section.c_str(), "hide_full", true);
            ticker.display = std::clamp(store.ReadInt(section.c_str(), "display", 0), 0, 2);
            ticker.direction = std::clamp(store.ReadInt(section.c_str(), "direction", 0), 0, 1);
            ticker.height = read("height", 3, 1, 12); ticker.gap = read("gap", 2, 0, 24);
            ticker.color = store.ReadArgb(section.c_str(), "color", index == 0 ? 0xFF78C4EE : 0xFFBFE07D);
            ticker.ready_color = store.ReadArgb(section.c_str(), "ready_color", 0xFFFFCA78);
            ticker.text = LoadTextProfile(store, (std::string{"text."} + module + "." + names[index]).c_str(), ticker.text);
        }
    }

    auto party_timing_settings::Save(const settings_store& store, const char* module) const -> void
    {
        for (size_t index = 0; index < this->tickers.size(); ++index)
        {
            const auto& ticker = this->tickers[index];
            const auto section = std::string{"timing."} + module + "." + names[index];
            store.WriteBool(section.c_str(), "enabled", ticker.enabled);
            store.WriteBool(section.c_str(), "other_members", ticker.other_members);
            store.WriteBool(section.c_str(), "hide_full", ticker.hide_full);
            store.WriteInt(section.c_str(), "display", ticker.display); store.WriteInt(section.c_str(), "direction", ticker.direction);
            store.WriteFloat(section.c_str(), "height", ticker.height); store.WriteFloat(section.c_str(), "gap", ticker.gap);
            store.WriteArgb(section.c_str(), "color", ticker.color); store.WriteArgb(section.c_str(), "ready_color", ticker.ready_color);
            SaveTextProfile(store, (std::string{"text."} + module + "." + names[index]).c_str(), ticker.text);
        }
    }

    auto party_timing_settings::Option(const size_t index) -> module_option
    {
        auto& ticker = this->tickers[static_cast<size_t>(this->selected)];
        static constexpr const char* kinds[]{"Swing", "Resting"}, *displays[]{"Mini bar", "Mini bar + countdown", "Text only"}, *directions[]{"Fill", "Drain"};
        switch (index)
        {
        case 0: return module_option::Element("Ticker", "Edit swing and resting tickers independently.", &this->selected, kinds, 2);
        case 1: return module_option::Toggle("Show ticker", "Draw the ticker inside the HP bar along its top edge.", &ticker.enabled).Ordered(-40);
        case 2: return module_option::Toggle("Other members", "Nearby members use observed timing. Swing waits for several melee rounds; unavailable members have no ticker.", &ticker.other_members).VisibleWhen(ticker.enabled);
        case 3: return module_option::Choice("Display",
            "Mini bar countdowns sit outside the right edge of HP without shortening it. Text only shares the row below HP, to the left of MP.",
            &ticker.display, displays, 3).Ordered(-30).VisibleWhen(ticker.enabled);
        case 4: return module_option::Choice("Direction", "Fill toward the next swing or recovery tick, or drain the remaining time.", &ticker.direction, directions, 2).VisibleWhen(ticker.enabled && ticker.display != 2);
        case 5: return module_option::Slider("Bar height", "Ticker thickness in pixels.", &ticker.height, 1, 12, 1).Ordered(101).VisibleWhen(ticker.enabled && ticker.display != 2);
        case 6: return module_option::Slider("Top inset", "Move the strip down inside HP. Height and inset fit within the HP fill.", &ticker.gap, 0, 24, 1).Ordered(302).VisibleWhen(ticker.enabled && ticker.display != 2);
        case 7: return module_option::Color("Ticker color", "Normal ticker fill; text retains its configured styling.", &ticker.color).VisibleWhen(ticker.enabled && ticker.display != 2);
        case 8: return module_option::Color("Ready color", "Highlight the final half-second of a known countdown.", &ticker.ready_color).VisibleWhen(ticker.enabled);
        case 9:
        {
            auto option = module_option::Toggle("Hide when full", "Hide resting ticks once HP and MP are full.", &ticker.hide_full);
            return option.VisibleWhen(ticker.enabled && this->selected == 1);
        }
        default:
        {
            auto option = TextProfileOption(index - 10, ticker.text);
            return option.Ordered(option.display_order + 1000, option.pair_next).VisibleWhen(ticker.enabled && ticker.display != 0);
        }
        }
    }

    auto party_timing_settings::CountdownWidth(const size_t index) const -> float
    {
        const auto& ticker = this->tickers[index];
        if (ticker.display == 0) return 0;
        const auto& text = ticker.text;
        const auto metrics = ui::NativeFontMetrics(text.style.font, ui::TextPixelSize(text.style.scale));
        return std::ceil(metrics.digit_advance * 7 + std::max(0.0f, text.style.letter_spacing) * 5 +
            std::max(0.0f, text.offset_x) + (text.placement == 0 ? 0 : 8));
    }
}
