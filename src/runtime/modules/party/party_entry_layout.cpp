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
        const float iconInset = this->job_icons_ ? this->icon_size_ + 7 : 0;
        const float x = bounds.x + iconInset, width = std::max(0.0f, bounds.width - iconInset);
        result.header = {x, bounds.y, width, this->HeaderHeight(slots.casting, details)};
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
            include(track); text(track, this->text_[5]);
        };
        if (slots.casting && this->cast_placement_ == cast_name) cast(result.header);
        else text(result.header, this->text_[0]);
        if (details) text(result.header, this->text_[4]);
        if (inZone && this->jobs_ && this->job_placement_ == jobs_name) text(result.header, this->text_[1]);
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
            result.zone = {x, y + 4, width, this->ZoneInfoHeight()};
            include(result.zone);
            const auto content = ui::container_renderer::ContentBounds(result.zone, this->ZoneStyle());
            text(content, this->text_[1]);
        }
        else
        {
            y += this->HeaderPadding(slots.casting);
            if (this->jobs_ && this->job_placement_ == jobs_below)
            {
                result.jobs = {x, y, width, LineHeight(this->text_[1])};
                text(result.jobs, this->text_[1]); y += result.jobs.height;
            }
            y += (this->labels_ ? LineHeight(this->text_[2]) * 0.5f : 0) + 1;
            result.resources = this->LayoutResources({x, y, width, 0}, slots);
            for (size_t i = 0; i < result.resources.bars.size(); ++i)
            {
                if ((i == 1 && !slots.mp) || (i == 2 && !slots.tp)) continue;
                const auto& at = result.resources.bars[i];
                if (slots.casting && ((i == 1 && this->cast_placement_ == cast_mp) || (i == 2 && this->cast_placement_ == cast_tp)))
                { cast(at); continue; }
                if ((i == 1 && !slots.mp_values) || (i == 2 && !slots.tp_values)) continue;
                const bool numberOnly = i == 2 && this->tp_display_ == 1;
                const auto track = numberOnly ? at : ui::LayoutMeterTrack(at, this->meters_[i]);
                if (!numberOnly) include(track);
                const auto labels = ui::LayoutMeterText(track, std::max(16.0f, LineHeight(this->text_[2])), LineHeight(this->text_[3]), 32);
                if (this->labels_) text(labels.label, this->text_[2]);
                if (i == 2 || (i == 0 ? this->hp_display_ : this->mp_display_) != 5) text(labels.value, this->text_[3]);
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
        return result;
    }

    auto party_module::MemberHeight(const party_member& member) const -> float
    {
        const auto container = this->LayoutContainer();
        const float width = container.canvas_width + (this->job_icons_ ? this->icon_size_ + 7 : 0);
        const auto entry = this->LayoutEntry({0, 0, width, 0}, member.in_zone, this->ResourceSlots(member), !this->DetailText(member).empty());
        return this->StatusHeight(entry.height, member.in_zone, member.buff_count);
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
        if (!this->icon_follow_name_) return entry.icon;
        return {name.x - leaderSpace - 7 - this->icon_size_, name.y + (name.height - this->icon_size_) * 0.5f, this->icon_size_, this->icon_size_};
    }
}
