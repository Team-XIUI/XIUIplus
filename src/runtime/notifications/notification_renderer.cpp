#include "notification_renderer.hpp"
#include "modules/module.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace
{
    auto FadeColor(const D3DCOLOR color, const float opacity) -> D3DCOLOR
    {
        return (color & 0x00FFFFFFu) | (static_cast<uint32_t>(static_cast<float>((color >> 24) & 0xFF) * opacity) << 24);
    }
}

namespace xiui
{
    auto notification_renderer::KindEnabled(const notification_kind kind, const notification_style& style) -> bool
    {
        switch (kind) {
        case notification_kind::item: return style.items;
        case notification_kind::experience: return style.experience;
        case notification_kind::skill: return style.skills;
        case notification_kind::party_invite: return style.party_invites;
        case notification_kind::trade_invite: return style.trade_invites;
        case notification_kind::key_item: return style.key_items;
        case notification_kind::gil: return style.gil;
        case notification_kind::treasure: return style.treasure;
        default: return true;
        }
    }

    auto notification_renderer::KindColor(const notification_kind kind, const notification_style& style) -> D3DCOLOR
    {
        switch (kind) {
        case notification_kind::experience: return style.experience_color;
        case notification_kind::skill: return style.skill_color;
        case notification_kind::party_invite: return style.party_color;
        case notification_kind::trade_invite: return style.trade_color;
        case notification_kind::key_item: return style.key_item_color;
        case notification_kind::gil: return style.gil_color;
        case notification_kind::treasure: return style.treasure_color;
        default: return style.item_color;
        }
    }

    auto notification_renderer::ScreenBounds(const ui::rect& viewport, const ui::rect& bounds, const float offsetX, const float offsetY, const ui::text_alignment alignment) -> ui::rect
    {
        const float inset = alignment == ui::text_alignment::left ? 0 : alignment == ui::text_alignment::center ? bounds.width * 0.5f : bounds.width;
        return {viewport.x + viewport.width * 0.5f - inset - 72.0f + offsetX,
            viewport.y + viewport.height * 0.55f + offsetY, bounds.width, bounds.height};
    }

    auto notification_renderer::Rows(const notification_center& center, const notification_style& style, const uint64_t now,
        const std::span<const int> routing, const int group) -> notification_rows
    {
        notification_rows rows;
        const auto lifetime = static_cast<uint64_t>(std::max(1.0f, style.duration) * 1000);
        const auto maximum = static_cast<size_t>(std::clamp(static_cast<int>(style.visible_count), 1, 8));
        size_t regular{};
        for (const bool invitations : {true, false})
            for (auto entry = center.Entries().rbegin(); entry != center.Entries().rend() && rows.count < rows.entries.size(); ++entry)
            {
                const auto kind = static_cast<size_t>(entry->event.kind);
                if (IsInvitation(entry->event.kind) != invitations || (!invitations && regular >= maximum) ||
                    (!routing.empty() && (kind >= routing.size() || routing[kind] != group)) ||
                    !KindEnabled(entry->event.kind, style) || entry->Remaining(now, lifetime) == 0) continue;
                rows.entries[rows.count++] = &*entry;
                if (!invitations) ++regular;
            }
        return rows;
    }

    auto notification_renderer::Draw(const ui::design_system& design, const notification_center& center, const ui::rect& bounds, const notification_style& style, const uint64_t now, ui::game_icon_cache* icons, const module_render_context* context, const std::span<const int> routing, const int group, notification_draw_result* result) -> void
    {
        if (result != nullptr) *result = {};
        const auto defaultLifetime = static_cast<uint64_t>(std::max(1.0f, style.duration) * 1000.0f);
        const auto& sizing = static_cast<const ui::text_style&>(style);
        const float padding = style.background ? 6.0f : 0;
        const auto rows = Rows(center, style, now, routing, group);
        const auto& visible = rows.entries;
        const auto count = rows.count;
        std::array<float, 10> heights{}, offsets{};
        std::array<bool, 10> compact{};
        float total{};
        for (size_t row = 0; row < count; ++row)
        {
            compact[row] = style.pin_invites && IsInvitation(visible[row]->event.kind) &&
                now >= visible[row]->created_at && now - visible[row]->created_at >= static_cast<uint64_t>(style.minify_after * 1000);
            auto text = sizing;
            if (compact[row]) text.scale *= .8f;
            heights[row] = std::max(compact[row] ? 18.0f : 24.0f, design.TextHeight(text));
            offsets[row] = total;
            total += heights[row] + style.row_spacing + padding * 2;
        }
        const float direction = style.stack_down ? 1.0f : -1.0f;
        const float stackOffset = style.center_vertically && count != 0 ?
            -direction * (offsets[count - 1] - heights[0] + style.rise) * .5f : 0;
        float riseOffset{};
        if (count != 0 && !IsInvitation(visible[0]->event.kind))
        {
            const auto* bottom = visible[0];
            riseOffset = std::clamp(static_cast<float>(now - bottom->created_at) / static_cast<float>(bottom->Lifetime(defaultLifetime)), 0.0f, 1.0f) * style.rise;
        }
        // Share vertical movement so row spacing stays fixed, including pinned invitations.
        for (size_t row = 0; row < count; ++row)
        {
            const auto* entry = visible[row];
            const auto& event = entry->event;
            const bool invitation = IsInvitation(event.kind);
            const float rowHeight = heights[row], iconSize = rowHeight;
            const float age = static_cast<float>(now - entry->created_at);
            const auto remainingMs = entry->Remaining(now, defaultLifetime);
            const float remaining = static_cast<float>(remainingMs);
            const float fade = std::clamp(age / 160.0f, 0.0f, 1.0f) * std::clamp(remaining / 450.0f, 0.0f, 1.0f);
            auto text = TintTextProfile({sizing}, KindColor(event.kind, style)).style;
            if (compact[row]) text.scale *= .8f;
            text.top_color = FadeColor(text.top_color, fade);
            text.bottom_color = FadeColor(text.bottom_color, fade);
            text.stroke.color = FadeColor(text.stroke.color, fade);
            text.shadow.color = FadeColor(text.shadow.color, fade);
            text.glow.color = FadeColor(text.glow.color, fade);
            text.sheen.color = FadeColor(text.sheen.color, fade);
            char label[192]{};
            if (invitation)
            {
                const auto seconds = static_cast<unsigned>((remainingMs + 999) / 1000);
                const auto* subject = compact[row] ? (event.kind == notification_kind::party_invite ? "Party / alliance invite" : "Trade request") : event.subject.c_str();
                std::snprintf(label, sizeof(label), "%s (%u:%02u)", subject, seconds / 60, seconds % 60);
            }
            else if (IsTextNotification(event.kind))
                std::snprintf(label, sizeof(label), "%s", event.subject.c_str());
            else if (event.kind == notification_kind::skill)
                std::snprintf(label, sizeof(label), "%s +%.1f", event.subject.c_str(), event.amount);
            else
                std::snprintf(label, sizeof(label), "+%.0f %s", event.amount, event.subject.c_str());
            auto* icon = (event.kind == notification_kind::item || event.kind == notification_kind::treasure || event.kind == notification_kind::gil) && icons != nullptr ? icons->Item(event.key) : nullptr;
            ui::rect iconUv{0, 0, 1, 1};
            const int tile = event.kind == notification_kind::party_invite ? 0
                : event.kind == notification_kind::trade_invite ? 1 : event.kind == notification_kind::key_item ? 2 : -1;
            if (tile >= 0 && icons != nullptr)
            {
                icon = icons->NotificationAtlas();
                iconUv = {static_cast<float>(tile) * 0.25f, 0, 0.25f, 1};
            }
            const float iconWidth = icon != nullptr ? iconSize + 6.0f : 0.0f;
            const float textWidth = std::max(1.0f, bounds.width - iconWidth - padding * 2);
            float measured = design.MeasureText(label, text);
            if (measured > textWidth && measured > 0.0f)
            {
                text.scale = std::max(0.5f, text.scale * textWidth / measured);
                measured = design.MeasureText(label, text);
            }
            measured = std::min(measured, textWidth);
            const float groupWidth = measured + iconWidth;
            float x = bounds.x + padding;
            if (style.alignment == ui::text_alignment::right)
                x = bounds.x + bounds.width - groupWidth - padding;
            else if (style.alignment == ui::text_alignment::center)
                x = bounds.x + (bounds.width - groupWidth) * 0.5f;
            x += (1 - std::clamp(age / 160.0f, 0.0f, 1.0f)) * (style.alignment == ui::text_alignment::left ? -12.0f : 12.0f);
            const float y = bounds.y + stackOffset + direction * (offsets[row] + riseOffset);
            if (result != nullptr && result->count < result->hits.size())
                result->hits[result->count++] = {entry->id, {x - padding, y - padding, groupWidth + padding * 2, rowHeight + padding * 2}};
            if (style.background && context != nullptr)
                context->DrawFadingContainer({x - padding, y - padding, groupWidth + padding * 2, rowHeight + padding * 2},
                    {fade * 0.9f, 5, 0, true}, style.alignment != ui::text_alignment::left);
            const ui::rect iconBounds{x + measured + 6.0f, y + (rowHeight - iconSize) * 0.5f, iconSize, iconSize};
            if (icon != nullptr)
                design.DrawTexture(icon, iconBounds, FadeColor(0xFFFFFFFF, fade), iconUv);
            text.alignment = ui::text_alignment::left;
            design.DrawText(label, {x, y, measured, rowHeight}, text);
        }
    }
} // namespace xiui
