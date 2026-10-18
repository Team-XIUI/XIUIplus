#include "party_module.hpp"
#include "ui/text_rasterizer.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace xiui
{
    auto party_module::LineHeight(const module_text_profile& profile) -> float
    { return std::max(14.0f, ui::NativeFontMetrics(profile.style.font, ui::TextPixelSize(profile.style.scale)).height); }

    auto party_module::LayoutEntry(const ui::rect& bounds, const bool inZone, const resource_slots& slots, const bool details) const -> entry_layout
    {
        entry_layout result{};
        const float x = bounds.x, width = bounds.width;
        result.header = {x, bounds.y, width, this->HeaderHeight(slots.casting, details, inZone)};
        float top = std::numeric_limits<float>::max(), bottom = std::numeric_limits<float>::lowest();
        const auto include = [&](const ui::rect& at) {
            if (at.height <= 0) return;
            top = std::min(top, at.y); bottom = std::max(bottom, at.y + at.height);
        };
        const auto text = [&](const ui::rect& at, const module_text_profile& profile) {
            const float height = ui::NativeFontMetrics(profile.style.font, ui::TextPixelSize(profile.style.scale)).height;
            include({at.x, at.y + profile.offset_y + std::max(0.0f, (at.height - height) * 0.5f), at.width, height});
        };
        const auto cast = [&](const ui::rect& area) {
            const auto at = this->CastBounds(area);
            const auto track = ui::LayoutMeterTrack(at, this->meters_[3]);
            auto label = this->text_[5];
            label.offset_y = 0;
            include(track); text(track, label);
        };
        const auto jobText = [&](ui::rect area) {
            if (this->stack_jobs_ && this->sub_job_)
            {
                area.y += std::max(0.0f,(area.height-this->JobTextHeight())*0.5f);
                area.height = LineHeight(this->text_[1]);
                text(area,this->text_[1]); area.y += area.height;
            }
            text(area,this->text_[1]);
        };
        if (slots.casting && this->cast_placement_ == cast_name)
            cast(this->NameCastArea(result.header, result.header.x + this->text_[0].offset_x));
        else text(result.header, this->text_[0]);
        if (details) text(result.header, this->text_[4]);
        if (inZone && this->jobs_ && this->job_anchor_ == job_anchor_name && this->job_placement_ == jobs_name)
        {
            auto jobs = result.header; jobs.y += this->text_[0].offset_y;
            jobText(jobs);
        }
        if (inZone && this->job_icons_)
        {
            result.icon = {bounds.x, bounds.y, this->icon_size_, this->icon_size_};
            if (this->icon_follow_name_)
                result.icon.y += this->text_[0].offset_y + (result.header.height - this->icon_size_) * 0.5f;
            include(result.icon);
        }
        float y = bounds.y + result.header.height;
        if (!inZone)
        {
            const float zoneX = std::max(bounds.x, x);
            result.zone = {zoneX, y + 4, std::max(0.0f, bounds.x + bounds.width - zoneX), this->ZoneInfoHeight()};
            include(result.zone);
            const auto content = ui::container_renderer::ContentBounds(result.zone, this->ZoneStyle());
            text(content, this->text_[zone_text]);
        }
        else
        {
            y += this->HeaderPadding(slots.casting);
            if (this->jobs_ && this->job_anchor_ == job_anchor_name && this->job_placement_ == jobs_below)
            {
                result.jobs = {x, y, width, this->JobTextHeight()};
                jobText(result.jobs); y += result.jobs.height;
            }
            y += (this->labels_ ? LineHeight(this->text_[2]) * 0.5f : 0) + 1;
            result.resources = this->LayoutResources({x, y, width, 0}, slots);
            if (this->jobs_ && this->job_anchor_ == job_anchor_hp)
                jobText(this->HPJobTextBounds(result.resources.bars[0]));
            for (size_t i = 0; i < result.resources.bars.size(); ++i)
            {
                if ((i == 1 && !slots.mp) || (i == 2 && !slots.tp)) continue;
                const auto& at = result.resources.bars[i];
                if (slots.casting && ((i == 1 && this->cast_placement_ == cast_mp) || (i == 2 && this->cast_placement_ == cast_tp)))
                { cast(at); continue; }
                if ((i == 1 && !slots.mp_values) || (i == 2 && !slots.tp_values)) continue;
                const bool numberOnly = i == 2 && this->tp_display_ == 1;
                const auto& valueProfile = this->text_[i == 2 ? tp_text : i == 1 ? mp_text : 3];
                const auto track = numberOnly ? at : ui::LayoutMeterTrack(at, this->meters_[i]);
                if (!numberOnly) include(track);
                const auto labels = ui::LayoutMeterText(track, std::max(16.0f, LineHeight(this->text_[2])), LineHeight(valueProfile), 32);
                if (this->labels_) text(labels.label, this->text_[2]);
                if (i == 2 || (i == 0 ? this->hp_display_ : this->mp_display_) != 5)
                    text(numberOnly ? this->TPNumberBounds(at) : labels.value, valueProfile);
            }
            for (size_t kind = 0; kind < result.tickers.size(); ++kind)
            {
                if (!(kind == 0 ? slots.swing : slots.resting)) continue;
                const auto& ticker = this->tickers_.tickers[kind];
                const auto track = ui::LayoutMeterTrack(result.resources.bars[0], this->meters_[0]);
                auto& at = result.tickers[kind];
                at = ticker.display == 0 ? ui::LayoutMeterStrip(track, ticker.height, ticker.gap) :
                    result.resources.ticker_text;
                if (ticker.display != 0) text(at, ticker.text);
            }
            if (slots.casting && this->cast_placement_ == cast_separate)
            {
                result.cast = {x, y + result.resources.height + 7, width, this->CastHeight()};
                cast(result.cast);
            }
        }
        result.height = std::ceil(std::max(1.0f, bottom - top));
        const float shift = bounds.y - top;
        for (auto* at : {&result.header, &result.jobs, &result.zone, &result.cast, &result.icon}) at->y += shift;
        for (auto& bar : result.resources.bars) bar.y += shift;
        result.resources.ticker_text.y += shift;
        for (auto& ticker : result.tickers) ticker.y += shift;
        return result;
    }

    auto party_module::MemberHeight(const party_member& member) const -> float
    {
        const auto container = this->LayoutContainer();
        const float width = container.canvas_width;
        const auto entry = this->LayoutEntry({0, 0, width, 0}, member.in_zone, this->ResourceSlots(member), !this->DetailText(member).empty());
        return this->StatusHeight(entry.height, member.in_zone, member.buff_count, &member);
    }

    auto party_module::DetailText(const party_member& member) const -> std::string
    {
        std::string detail;
        if (this->show_sync_ && member.sync) detail = "SYNC";
        if (this->distance_ && member.distance_known)
        {
            char buffer[24]{}; std::snprintf(buffer, sizeof(buffer), "%.1fy", member.distance);
            if (!detail.empty()) detail += "  ";
            detail += buffer;
        }
        return detail;
    }

    auto party_module::JobIconBounds(const entry_layout& entry, const ui::rect& name, const float leaderSpace) const -> ui::rect
    {
        auto result = this->icon_follow_name_ ? ui::rect{name.x - leaderSpace - 7 - this->icon_size_,
            name.y + (name.height - this->icon_size_) * 0.5f, this->icon_size_, this->icon_size_} : entry.icon;
        result.x += this->icon_offset_x_; result.y += this->icon_offset_y_;
        return result;
    }

    auto party_module::JobTextBounds(const ui::rect& header, const ui::rect& name, const float right) const -> ui::rect
    {
        const float x = name.x + name.width + 6;
        return {x, header.y + this->text_[0].offset_y, std::max(0.0f, right - x), header.height};
    }

    auto party_module::JobTextHeight(void) const -> float
    { return LineHeight(this->text_[1])*(this->stack_jobs_ && this->sub_job_ ? 2 : 1); }

    auto party_module::HPJobTextBounds(const ui::rect& hp) const -> ui::rect
    {
        const auto track=ui::LayoutMeterTrack(hp,this->meters_[0]);
        return {track.x,track.y,track.width,this->JobTextHeight()};
    }

    auto party_module::NameCastArea(const ui::rect& bounds, const float nameX) const -> ui::rect
    {
        return {nameX, bounds.y + this->text_[0].offset_y,
            std::max(0.0f, bounds.x + bounds.width - nameX), bounds.height};
    }

    auto party_module::TPNumberBounds(const ui::rect& bounds) const -> ui::rect
    {
        auto result = bounds;
        const auto& style = this->text_[tp_text].style;
        const auto metrics = ui::NativeFontMetrics(style.font, ui::TextPixelSize(style.scale));
        // Explicit text placement reserves four pixels at each edge.
        const float padding = this->text_[tp_text].placement != 0 ? 8.0f : 0;
        result.width = std::max(bounds.width, std::ceil(metrics.digit_advance * 4 + std::max(0.0f, style.letter_spacing) * 3) + padding);
        return result;
    }
}
