#pragma once

#include "modules/module.hpp"
#include "ui/game_icon_cache.hpp"
#include "combat/combat_source.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>

namespace xiui::ui
{
    struct status_strip_settings
    {
        static constexpr size_t option_count = 18;
        bool enabled{true}, timers{true}, crowd_control_only{}, uncertainty{true}, tooltips{true}, overflow{true};
        float size{20}, spacing{2}, limit{32}, offset_x{}, offset_y{}, timer_scale{0.72f}, columns{}, opacity{1};
        float row_spacing{2};
        int placement{}, timer_placement{}, icon_theme{};
        auto Load(const settings_store& store, const char* section, const float minimumSize = 12) -> void
        {
            this->enabled = store.ReadBool(section, "enabled", this->enabled); this->timers = store.ReadBool(section, "timers", this->timers);
            this->crowd_control_only = store.ReadBool(section, "crowd_control_only", this->crowd_control_only); this->uncertainty = store.ReadBool(section, "uncertainty", this->uncertainty);
            this->tooltips = store.ReadBool(section, "tooltips", this->tooltips); this->overflow = store.ReadBool(section, "overflow", this->overflow);
            const auto value = [&](const char* key, float fallback, float minimum, float maximum) {
                const auto result = store.ReadFloat(section, key, fallback);
                return std::isfinite(result) ? std::clamp(result, minimum, maximum) : fallback;
            };
            this->size = value("size", this->size, minimumSize, 48); this->spacing = value("spacing", this->spacing, 0, 16);
            this->row_spacing = value("row_spacing", this->spacing, 0, 32);
            this->limit = value("limit", this->limit, 1, 32); this->timer_scale = value("timer_scale", this->timer_scale, 0.5f, 1.5f);
            this->offset_x = value("offset_x", this->offset_x, -500, 500); this->offset_y = value("offset_y", this->offset_y, -100, 100);
            this->columns = value("columns", this->columns, 0, 32); this->opacity = value("opacity", this->opacity, 0, 1);
            this->placement = std::clamp(store.ReadInt(section, "placement", 0), 0, 2);
            this->timer_placement = std::clamp(store.ReadInt(section, "timer_placement", 0), 0, 2);
            this->icon_theme = store.ReadInt(section, "icon_theme", 0);
        }
        auto Save(const settings_store& store, const char* section) const -> void
        {
            store.WriteBool(section, "enabled", this->enabled); store.WriteBool(section, "timers", this->timers);
            store.WriteBool(section, "crowd_control_only", this->crowd_control_only); store.WriteBool(section, "uncertainty", this->uncertainty);
            store.WriteBool(section, "tooltips", this->tooltips); store.WriteBool(section, "overflow", this->overflow);
            store.WriteFloat(section, "size", this->size); store.WriteFloat(section, "spacing", this->spacing);
            store.WriteFloat(section, "row_spacing", this->row_spacing);
            store.WriteFloat(section, "limit", this->limit); store.WriteFloat(section, "timer_scale", this->timer_scale);
            store.WriteFloat(section, "offset_x", this->offset_x); store.WriteFloat(section, "offset_y", this->offset_y);
            store.WriteFloat(section, "columns", this->columns); store.WriteFloat(section, "opacity", this->opacity);
            store.WriteInt(section, "placement", this->placement);
            store.WriteInt(section, "timer_placement", this->timer_placement);
            store.WriteInt(section, "icon_theme", this->icon_theme);
        }
        auto Option(size_t index, const float minimumSize = 12) -> module_option
        {
            static constexpr const char* placements[]{"Left", "Center", "Right"};
            static constexpr const char* timerPlacements[]{"Below icon", "Above icon", "Over icon"};
            switch (index)
            {
            case 0: return module_option::Toggle("Status icons", "Show available native or observed status effects.", &this->enabled).Ordered(-10);
            case 1: return module_option::Slider("Status icon size", "Size in pixels.", &this->size, minimumSize, 48, 1).Ordered(110).VisibleWhen(this->enabled);
            case 2: return module_option::Slider("Status icon spacing", "Horizontal space between status icons.", &this->spacing, 0, 16, 1).Ordered(300, true).VisibleWhen(this->enabled);
            case 3: return module_option::Slider("Status limit", "Maximum visible icons, up to the client's 32 effects.", &this->limit, 1, 32, 1).Ordered(121).VisibleWhen(this->enabled);
            case 4: return module_option::Choice("Status alignment", "Align the strip within the available width.", &this->placement, placements, 3).Ordered(202).VisibleWhen(this->enabled);
            case 5: return module_option::Slider("Status X offset", "Horizontal offset in pixels.", &this->offset_x, -500, 500, 1).Ordered(210, true).VisibleWhen(this->enabled);
            case 6: return module_option::Slider("Status Y offset", "Vertical offset in pixels.", &this->offset_y, -100, 100, 1).Ordered(211).VisibleWhen(this->enabled);
            case 7: return module_option::Toggle("Status timers", "Show countdowns. Tooltips identify XIUI+ duration estimates; local effects use native expiry times.", &this->timers).Ordered(850).VisibleWhen(this->enabled);
            case 8: return module_option::Toggle("Crowd-control timers only", "Limit countdown labels to sleep, petrification, stun, charm and terror.", &this->crowd_control_only).Ordered(851).VisibleWhen(this->enabled && this->timers);
            case 9: return module_option::Toggle("Unknown timer markers", "Show ? when neither a native timer nor an XIUI+ duration is available.", &this->uncertainty).Ordered(852).VisibleWhen(this->enabled && this->timers);
            case 10: return module_option::Toggle("Status tooltips", "Show game effect names and descriptions on hover or deliberate controller inspection.", &this->tooltips).Ordered(870).VisibleWhen(this->enabled);
            case 11: return module_option::Toggle("Status overflow count", "Show how many effects are hidden by the icon limit.", &this->overflow).Ordered(870).VisibleWhen(this->enabled);
            case 12: return module_option::Slider("Status timer scale", "Scale countdown text while retaining its normal effects.", &this->timer_scale, 0.5f, 1.5f, 0.05f).Ordered(861).VisibleWhen(this->enabled && this->timers);
            case 13: return module_option::Slider("Statuses per row", "Zero wraps automatically at the available width.", &this->columns, 0, 32, 1).Ordered(120, true).VisibleWhen(this->enabled);
            case 14: return module_option::Slider("Status opacity", "Icon and countdown opacity.", &this->opacity, 0, 1, 0.05f).Ordered(450).VisibleWhen(this->enabled);
            case 15: return module_option::Choice("Timer placement", "Place countdown text below, above or over the effect icon. Timer text X/Y offsets fine-tune its position.", &this->timer_placement, timerPlacements, 3).Ordered(860).VisibleWhen(this->enabled && this->timers);
            case 16: return module_option::Slider("Status row spacing", "Vertical gap between wrapped rows, independent of icon spacing.", &this->row_spacing, 0, 32, 1).Ordered(301).VisibleWhen(this->enabled);
            default: return module_option::Choice("Status icon theme", "Follow the global icon theme or choose artwork for this list. Missing effects use native artwork.", &this->icon_theme, status_icon_overrides, std::size(status_icon_overrides)).Ordered(440).VisibleWhen(this->enabled);
            }
        }
    };

    struct status_strip_layout
    {
        rect bounds{};
        size_t count{}, overflow{}, columns{};
        float height{}, size{}, spacing{}, pitch{}, icon_offset_y{};
        auto Icon(const size_t index) const -> rect
        { return {this->bounds.x + static_cast<float>(index % this->columns) * (this->size + this->spacing),
            this->bounds.y + this->icon_offset_y + static_cast<float>(index / this->columns) * this->pitch, this->size, this->size}; }
    };

    inline auto LayoutStatusStrip(const size_t count, const rect& bounds, const status_strip_settings& settings) -> status_strip_layout
    {
        status_strip_layout result{};
        if (!settings.enabled || count == 0) return result;
        result.count = std::min(count, static_cast<size_t>(std::clamp(settings.limit, 1.0f, 32.0f)));
        result.overflow = settings.overflow ? count - result.count : 0;
        result.size = settings.size; result.spacing = settings.spacing;
        const auto slots = result.count + (result.overflow != 0 ? 1 : 0);
        result.columns = std::min(slots, static_cast<size_t>(std::max(1.0f, std::floor((bounds.width + settings.spacing) / (settings.size + settings.spacing)))));
        if (settings.columns >= 1) result.columns = std::min(result.columns, static_cast<size_t>(settings.columns));
        const auto rows = (slots + result.columns - 1) / result.columns;
        const float caption = settings.timers && settings.timer_placement != 2 ? std::max(14.0f, settings.size * settings.timer_scale) : 0;
        result.icon_offset_y = settings.timer_placement == 1 ? caption : 0;
        result.pitch = settings.size + caption + settings.row_spacing;
        result.height = static_cast<float>(rows) * result.pitch - settings.row_spacing + 2;
        const auto width = static_cast<float>(result.columns) * (settings.size + settings.spacing) - settings.spacing;
        const auto alignment = settings.placement == 1 ? 0.5f : settings.placement == 2 ? 1.0f : 0.0f;
        result.bounds = {bounds.x + settings.offset_x + std::max(0.0f, bounds.width - width) * alignment, bounds.y + settings.offset_y, width, result.height};
        return result;
    }

    inline auto StatusStripColumns(const float width) -> size_t { return static_cast<size_t>(std::max(1.0f, std::floor((width + 2) / 22))); }

    inline auto StatusStripHeight(const size_t count, const float width, const status_strip_settings& settings = {}) -> float
    { const auto layout = LayoutStatusStrip(count, {0, 0, width, 0}, settings); return layout.height == 0 ? 0 : std::max(0.0f, settings.offset_y) + layout.height; }

    inline auto CrowdControlStatus(const uint16_t id) -> bool
    { return id == 2 || id == 7 || id == 10 || id == 14 || id == 17 || id == 18 || id == 19 || id == 28 || id == 193; }

    inline auto StatusTimerText(const combat_status& status, const uint64_t now, const status_strip_settings& settings) -> std::string
    {
        if (!settings.timers || (settings.crowd_control_only && !CrowdControlStatus(status.id))) return {};
        if (status.expires <= now) return settings.uncertainty && (status.observed != 0 || status.uncertain) ? "?" : "";
        const auto seconds = static_cast<unsigned>((status.expires - now + 999) / 1000);
        char text[24]{};
        if (status.estimated && status.earliest != 0)
        {
            if (status.earliest > now) std::snprintf(text, sizeof(text), "%u-%u", static_cast<unsigned>((status.earliest - now + 999) / 1000), seconds);
            else std::snprintf(text, sizeof(text), "<%u", seconds);
            return text;
        }
        if (seconds >= 3600) std::snprintf(text, sizeof(text), "%uh%02um", seconds / 3600, seconds / 60 % 60);
        else if (seconds >= 60) std::snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
        else std::snprintf(text, sizeof(text), "%u", seconds);
        return text;
    }

    inline auto StatusTimerBounds(const rect& icon, const status_strip_settings& settings, const module_text_profile& profile) -> rect
    {
        const float height = std::max(14.0f, settings.size * settings.timer_scale);
        const float y = settings.timer_placement == 1 ? icon.y - height :
            settings.timer_placement == 2 ? icon.y + (icon.height - height) * 0.5f : icon.y + icon.height;
        return {icon.x - 2 + profile.offset_x, y + profile.offset_y, icon.width + 4, height};
    }

    inline auto DrawStatusStrip(const std::span<const combat_status> statuses, const rect& bounds, const uint64_t now,
        const module_text_profile& profile, const module_render_context& context, const status_strip_settings& settings = {}) -> void
    {
        const auto layout = LayoutStatusStrip(statuses.size(), bounds, settings);
        auto caption = profile;
        const auto fade = [opacity = settings.opacity](const D3DCOLOR color) {
            return (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * opacity) << 24);
        };
        caption.style.top_color = fade(caption.style.top_color); caption.style.bottom_color = fade(caption.style.bottom_color);
        caption.style.shadow.color = fade(caption.style.shadow.color); caption.style.stroke.color = fade(caption.style.stroke.color);
        caption.style.glow.color = fade(caption.style.glow.color); caption.style.sheen.color = fade(caption.style.sheen.color);
        caption.placement = 0; caption.offset_x = caption.offset_y = 0;
        caption.style.alignment = text_alignment::center;
        caption.style.scale = std::clamp(caption.style.scale * settings.timer_scale, text_min_scale, text_max_scale);
        for (size_t i = 0; i < layout.count; ++i)
        {
            const auto& status = statuses[i];
            const auto icon = layout.Icon(i);
            const auto artwork = context.Icons() != nullptr ? context.Icons()->StatusIcon(status.id, settings.icon_theme) : game_icon_view{};
            if (artwork.texture != nullptr) context.Design().DrawTexture(artwork.texture, icon, (static_cast<uint32_t>(std::round(settings.opacity * 255)) << 24) | 0xFFFFFF, artwork.uv);
            else
            {
                char id[12]{}; std::snprintf(id, sizeof(id), "%u", status.id);
                context.DrawText(id, icon, icon, caption);
            }
            const auto timer = StatusTimerText(status, now, settings);
            if (!timer.empty())
            {
                const auto text = StatusTimerBounds(icon, settings, profile);
                auto fitted = caption;
                while (fitted.style.scale > text_min_scale && context.Design().MeasureText(timer.c_str(), fitted.style) > text.width)
                    fitted.style.scale = std::max(text_min_scale, fitted.style.scale - 1 / text_base_pixels);
                context.DrawText(timer.c_str(), text, text, fitted);
            }
        }
        if (layout.overflow != 0)
        { const auto at = layout.Icon(layout.count); context.DrawText(("+" + std::to_string(layout.overflow)).c_str(), at, at, caption); }
    }

    struct status_tooltip { combat_status status{}; rect bounds{}; uint32_t entity{}; };

    inline auto StatusHitOccurrence(const std::span<const status_tooltip> hits, const size_t index) -> size_t
    {
        size_t occurrence{};
        for (size_t i = 0; i < index; ++i)
            if (hits[i].entity == hits[index].entity && hits[i].status.id == hits[index].status.id) ++occurrence;
        return occurrence;
    }

    inline auto StatusHitIndex(const std::span<const status_tooltip> hits, const uint32_t entity,
        const uint16_t effect, size_t occurrence) -> std::optional<size_t>
    {
        for (size_t i = 0; i < hits.size(); ++i)
            if (hits[i].entity == entity && hits[i].status.id == effect && occurrence-- == 0) return i;
        return std::nullopt;
    }

    inline auto StatusStripHit(const std::span<const combat_status> statuses, const rect& bounds,
        const status_strip_settings& settings, const float x, const float y) -> std::optional<status_tooltip>
    {
        if (!settings.tooltips) return std::nullopt;
        const auto layout = LayoutStatusStrip(statuses.size(), bounds, settings);
        for (size_t index = 0; index < layout.count; ++index)
        {
            const auto icon = layout.Icon(index);
            if (x >= icon.x && x < icon.x + icon.width && y >= icon.y && y < icon.y + icon.height)
                return status_tooltip{statuses[index], icon};
        }
        return std::nullopt;
    }

    inline auto DrawStatusTooltip(const status_tooltip& tooltip, IResourceManager* resources,
        const module_text_profile& profile, const module_render_context& context) -> void
    {
        const auto id = tooltip.status.id;
        const auto* nativeName = resources != nullptr ? resources->GetString("buffs.names", id) : nullptr;
        const auto* icon = resources != nullptr ? resources->GetStatusIconByIndex(id) : nullptr;
        const auto* nativeDescription = icon != nullptr ? (icon->Description[2] != nullptr ? icon->Description[2] : icon->Description[0]) : nullptr;
        auto title = nativeName != nullptr ? FromGameText(nativeName) : std::string{"Status"};
        if (title.empty()) title = "Status";
        bool capitalize = true;
        for (auto& character : title)
        {
            if (capitalize && character >= 'a' && character <= 'z') character -= 'a' - 'A';
            capitalize = character == ' ' || character == '-' || character == '/' || character == '(';
        }
        const auto description = nativeDescription != nullptr ? FromGameText(nativeDescription) : std::string{};
        auto text = profile;
        text.placement = 0; text.offset_x = text.offset_y = 0; text.style.alignment = text_alignment::left;
        auto titleText = text;
        if (font_faces[FontIndex(titleText.style.font)].weight < FW_BOLD) titleText.style.font = 0;
        titleText.style.scale = std::min(text_max_scale, text.style.scale + 1 / text_base_pixels);
        const auto descriptionText = TintTextProfile(text, 0xFFE0E0E0);
        auto estimateText = TintTextProfile(text, 0xFFB8B8B8);
        estimateText.style.scale = std::max(text_min_scale, text.style.scale - 2 / text_base_pixels);
        const auto& viewport = context.Viewport();
        float width = std::min(300.0f, std::max(40.0f, viewport.width - 16));
        const float contentWidth = width - 26;
        struct tooltip_line { std::string value; const module_text_profile* profile; float height; };
        std::vector<tooltip_line> lines;
        const auto appendWrapped = [&](const std::string& value, const module_text_profile& style)
        {
            const float lineHeight = context.Design().TextHeight(style.style) + 4;
            const auto append = [&](const std::string& line) { lines.push_back({line, &style, lineHeight}); };
            std::string line;
            for (size_t start = 0; start < value.size();)
            {
                const auto end = value.find_first_of(" \r\n", start);
                const auto word = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!word.empty())
                {
                    const auto candidate = line.empty() ? word : line + " " + word;
                    if (!line.empty() && context.Design().MeasureText(candidate.c_str(), style.style) > contentWidth)
                    { append(line); line = word; }
                    else line = candidate;
                }
                if (end == std::string::npos) break;
                if (value[end] == '\n' && !line.empty()) { append(line); line.clear(); }
                start = end + 1;
            }
            if (!line.empty()) append(line);
        };
        appendWrapped(title, titleText);
        if (!description.empty()) { lines.back().height += 3; appendWrapped(description, descriptionText); }
        if (tooltip.status.estimated)
        {
            lines.back().height += 3;
            appendWrapped(tooltip.status.earliest != 0 ? "Base duration range: modifiers and resistance can change the result." : "Estimate: Effects may end early", estimateText);
        }
        float textWidth{}, height{26};
        for (const auto& line : lines)
        {
            textWidth = std::max(textWidth, context.Design().MeasureText(line.value.c_str(), line.profile->style));
            height += line.height;
        }
        width = std::min(width, std::max(80.0f, textWidth + 26));
        const auto left = std::clamp(tooltip.bounds.x + tooltip.bounds.width + 8, viewport.x + 8,
            std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
        const auto top = std::clamp(tooltip.bounds.y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
        const auto content = context.DrawTooltip({left, top, width, height});
        float y = content.y;
        for (const auto& line : lines)
        {
            context.DrawText(line.value.c_str(), {content.x, y, content.width, line.height}, content, *line.profile);
            y += line.height;
        }
    }
}
