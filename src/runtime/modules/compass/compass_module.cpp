#include "compass_module.hpp"

#include <algorithm>
#include <cstdio>

namespace xiui
{
    auto compass_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->labels_ = {theme.meter_text}; this->labels_.style.scale = 0.7f;
        this->heading_text_ = {theme.caption_text}; this->heading_text_.style.alignment = ui::text_alignment::center;
    }

    auto compass_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds; const auto* data = this->data_;
        *this = {}; this->state_.bounds = bounds; this->data_ = data;
        this->ApplyTextDefaults(theme);
    }

    auto compass_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("compass", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.compass", this->state_.bounds);
        this->labels_visible_ = store.ReadBool("compass", "labels", true);
        this->heading_visible_ = store.ReadBool("compass", "heading", false);
        if (store.ReadInt("compass", "layout_version", 0) < 2 && this->state_.bounds.width == 144)
        {
            this->state_.bounds.width = 128;
            this->heading_visible_ = false;
        }
        this->markers_ = store.ReadBool("compass", "markers", true);
        this->range_ = std::clamp(store.ReadFloat("compass", "range", 50), 10.0f, 200.0f);
        this->opacity_ = std::clamp(store.ReadFloat("compass", "opacity", 1), 0.1f, 1.0f);
        this->labels_ = LoadTextProfile(store, "text.compass.labels", this->labels_);
        this->heading_text_ = LoadTextProfile(store, "text.compass.heading", this->heading_text_);
    }

    auto compass_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("compass", "enabled", this->state_.enabled);
        store.WriteRect("layout.compass", this->state_.bounds);
        store.WriteBool("compass", "labels", this->labels_visible_);
        store.WriteBool("compass", "heading", this->heading_visible_);
        store.WriteBool("compass", "markers", this->markers_);
        store.WriteFloat("compass", "range", this->range_);
        store.WriteFloat("compass", "opacity", this->opacity_);
        store.WriteInt("compass", "layout_version", 2);
        SaveTextProfile(store, "text.compass.labels", this->labels_);
        SaveTextProfile(store, "text.compass.heading", this->heading_text_);
    }

    auto compass_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Compass visibility, direction labels and heading."},
            {"Markers", "Party, alliance and target markers with their display range."},
            {"Appearance", "Native compass size and opacity."},
            {"Heading Text", "Heading text size, colors, effects and offsets."}};
        return sections;
    }

    auto compass_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= 7) return section == 3;
        if (index == 1 || index == 6) return section == 2;
        if (index == 4 || index == 5) return section == 1;
        return section == 0;
    }

    auto compass_module::Option(const size_t index) -> module_option
    {
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Movable compass using native game artwork.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Slider("Size", "Width of the native tilted compass.", &this->state_.bounds.width, 80, 360, 1, module_option_group::appearance).Ordered(110);
        case 2: return module_option::Toggle("Directions", "Native direction glyphs outside the rim.", &this->labels_visible_);
        case 3: return module_option::Toggle("Heading", "Show facing direction and degrees.", &this->heading_visible_);
        case 4: return module_option::Toggle("Markers", "Show party, alliance, and your selected target.", &this->markers_);
        case 5: return module_option::Slider("Range", "Marker radius in yalms.", &this->range_, 10, 200, 5).VisibleWhen(this->markers_);
        case 6: return module_option::Slider("Opacity", "Native compass artwork opacity.", &this->opacity_, 0.1f, 1, 0.05f, module_option_group::appearance).Ordered(450);
        default: return TextProfileOption(index - 7, this->heading_text_).VisibleWhen(this->heading_visible_);
        }
    }

    auto compass_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const float size = this->state_.bounds.width, scale = size / 128.0f;
        const float compassHeight = size * (0.875f * compass_flatten + 0.125f);
        const float headingHeight = this->heading_visible_ ? std::max(20.0f, context.Design().TextHeight(this->heading_text_.style) + 4) : 0;
        this->state_.bounds.height = compassHeight + headingHeight;
        const auto& bounds = this->state_.bounds;
        const float centerX = bounds.x + size * 0.5f, centerY = bounds.y + compassHeight * 0.5f;
        const auto& colors = context.Design().GetTheme().colors;
        auto* assets = context.NativeAssets();
        const float rotation = this->data_ != nullptr && this->data_->compass_rotation_valid ? this->data_->compass_rotation : 3.141592654f;
        if (assets == nullptr || !assets->DrawCompassPart(context.Design(), 0, centerX, centerY, scale * 0.4f, rotation, compass_flatten, this->opacity_))
        {
            for (int segment = 0; segment < 64; ++segment)
            {
                const float a = segment * 6.283185307f / 64, b = (segment + 1) * 6.283185307f / 64;
                context.Design().DrawTriangle(centerX, centerY, centerX + std::cos(a) * size * 0.4f, centerY + std::sin(a) * size * 0.4f * compass_flatten,
                    centerX + std::cos(b) * size * 0.4f, centerY + std::sin(b) * size * 0.4f * compass_flatten, colors.surface_raised);
            }
        }
        const auto marker = [&](const float x, const float y, const D3DCOLOR color, const float markerScale) {
            if (assets == nullptr || !assets->DrawCompassPart(context.Design(), 5, x, y, markerScale, 0, 1, this->opacity_, color))
                context.Design().FillRect({x - markerScale * 2, y - markerScale * 2, markerScale * 4, markerScale * 4}, color);
        };
        if (this->markers_ && this->data_ != nullptr)
            for (const auto& point : this->data_->markers)
            {
                if (std::hypot(point.x, point.y) > this->range_) continue;
                const auto offset = ProjectCompassPoint(-point.x, point.y, rotation, size * 0.37f / this->range_);
                marker(centerX + offset[0], centerY + offset[1], point.color, scale * (point.selected ? 1.35f : 1.0f));
            }
        marker(centerX, centerY, colors.text, scale);
        if (this->labels_visible_)
        {
            constexpr const char* labels[]{"N", "E", "S", "W"};
            constexpr size_t groups[]{4, 1, 3, 2};
            constexpr float xs[]{0, -140, 0, 140}, ys[]{140, 0, -140, 0};
            for (size_t i = 0; i < 4; ++i)
            {
                const auto offset = ProjectCompassPoint(xs[i], ys[i], rotation, scale * 0.4f);
                const float x = centerX + offset[0], y = centerY + offset[1];
                if (assets != nullptr && assets->DrawCompassPart(context.Design(), groups[i], x, y, scale, 0, 1, this->opacity_)) continue;
                auto profile = this->labels_; profile.style.alignment = ui::text_alignment::center;
                const ui::rect label{x - 10 * scale, y - 9 * scale, 20 * scale, 18 * scale};
                context.DrawText(labels[i], label, label, profile);
            }
        }
        if (this->heading_visible_)
        {
            const float heading = this->data_ != nullptr && this->data_->heading_valid ? this->data_->heading : -1.570796327f;
            char text[32]{}; std::snprintf(text, sizeof(text), "%s  %03.0f", HeadingName(heading), std::fmod(std::round(HeadingDegrees(heading)), 360.0f));
            const ui::rect line{bounds.x, bounds.y + compassHeight, size, headingHeight};
            context.DrawText(text, line, line, this->heading_text_);
        }
    }
}
