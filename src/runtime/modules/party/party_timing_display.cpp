#include "party_module.hpp"

#include <cmath>
#include <cstdio>

namespace xiui
{
    auto party_module::LayoutTickerText(resource_layout& layout, const resource_slots& slots, const float top) const -> void
    {
        if (!slots.swing && !slots.resting) return;
        const size_t kind = slots.swing ? 0 : 1;
        const auto& ticker = this->tickers_.tickers[kind];
        if (ticker.display == 0) return;
        const auto hp = ui::LayoutMeterTrack(layout.bars[0], this->meters_[0]);
        auto& at = layout.ticker_text;
        if (ticker.display == 1)
            at = {hp.x + hp.width + 6, layout.bars[0].y, this->tickers_.CountdownWidth(kind), layout.bars[0].height};
        else
        {
            auto& mp = layout.bars[1];
            if (slots.mp && mp.y + mp.height * 0.5f > hp.y + hp.height * 0.5f)
            {
                constexpr float gap = 4;
                const float needed = hp.x + this->tickers_.CountdownWidth(kind) + gap - mp.x;
                const float inset = std::min(std::max(0.0f, needed), std::max(0.0f, mp.width - 24));
                mp.x += inset; mp.width -= inset;
                at = {hp.x, mp.y, std::max(0.0f, mp.x - hp.x - gap), mp.height};
            }
            else at = {hp.x, hp.y + hp.height + 2, hp.width, LineHeight(ticker.text)};
        }
        layout.height = std::max(layout.height, at.y + at.height + std::max(0.0f, ticker.text.offset_y) - top);
    }

    auto party_module::Ticker(const party_member& member, const size_t kind, const uint64_t now) const -> attack_prediction
    {
        const auto& ticker = this->tickers_.tickers[kind];
        if (!ticker.enabled || !member.in_zone) return {};
        if (member.preview)
        {
            if (!ticker.other_members && member.slot != 0) return {};
            if (member.status != (kind == 0 ? 1u : 33u)) return {};
            const float interval = kind == 0 ? 4.0f : 9.0f;
            const float elapsed = std::fmod(static_cast<float>(now % 36000) / 1000, interval);
            return {true, true, true, elapsed / interval, interval - elapsed, interval};
        }
        if (this->timing_ == nullptr || (!ticker.other_members && !this->timing_->IsPlayer(member.id))) return {};
        if (kind == 1 && ticker.hide_full && this->timing_->Full(member.id)) return {};
        auto prediction = kind == 0 ? this->timing_->Swing(member.id, now / 1000.0) : this->timing_->Resting(member.id, now / 1000.0);
        if (!prediction.known) prediction.ratio = 0;
        return prediction;
    }

    auto party_module::DrawTickers(const party_member& member, const entry_layout& entry, const resource_slots& slots,
        const uint64_t now, const module_render_context& context) const -> void
    {
        for (size_t kind = 0; kind < entry.tickers.size(); ++kind)
        {
            if (!(kind == 0 ? slots.swing : slots.resting)) continue;
            const auto timer = this->Ticker(member, kind, now);
            const auto& ticker = this->tickers_.tickers[kind];
            if (ticker.display == 0 || entry.tickers[kind].width <= 0) continue;
            char countdown[32]{};
            if (timer.state == "Ready") std::snprintf(countdown, sizeof(countdown), "Ready");
            else if (timer.known) std::snprintf(countdown, sizeof(countdown), "~%.1fs", timer.seconds);
            else std::snprintf(countdown, sizeof(countdown), "--");
            const bool ready = timer.state == "Ready" || (timer.known && timer.state.empty() && timer.seconds <= 0.5f);
            auto text = ready ? TintTextProfile(ticker.text, ticker.ready_color) : ticker.text;
            text.style.alignment = ui::text_alignment::left;
            text.style.tabular_numbers = true;
            const auto& at = entry.tickers[kind];
            context.DrawText(countdown, at, at, text);
        }
    }
}
