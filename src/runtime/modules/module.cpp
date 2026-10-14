#include "module.hpp"
#include "settings/text_style_settings.hpp"
#include "ui/button_renderer.hpp"
#include "ui/tp_meter.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    auto PositionText(const xiui::ui::rect& bounds, const xiui::ui::rect& placement, const xiui::module_text_profile& profile)
        -> std::pair<xiui::ui::rect, xiui::ui::text_style>
    {
        auto positioned = bounds;
        auto style = profile.style;
        positioned.y += profile.offset_y;
        if (profile.placement == 0) positioned.x += profile.offset_x;
        else
        {
            positioned.x = placement.x + 4 + profile.offset_x;
            positioned.width = std::max(1.0f, placement.width - 8);
            style.alignment = profile.placement == 1 ? xiui::ui::text_alignment::left :
                profile.placement == 2 ? xiui::ui::text_alignment::center : xiui::ui::text_alignment::right;
        }
        return {positioned, style};
    }
}

namespace xiui
{
    auto module_option::Element(const char* label, const char* description, int* value, const char* const* choices, const size_t count, const module_option_group group) -> module_option
    {
        auto option = Choice(label, description, value, choices, count, group);
        option.selector = module_selector::element;
        return option;
    }

    auto module_option::EditScope(const char* label, const char* description, int* value, const char* const* choices, const size_t count) -> module_option
    {
        auto option = Element(label, description, value, choices, count);
        option.selector = module_selector::scope;
        return option;
    }

    auto module_option::Dropdown(const char* label, const char* description, int* value, const char* const* choices, const size_t count) -> module_option
    {
        auto result = Choice(label, description, value, choices, count);
        result.kind = module_option_kind::dropdown;
        return result;
    }

    auto module_option::OverrideColor(const char* label, const char* description, bool* enabled, D3DCOLOR* color) -> module_option
    {
        auto result = Color(label, description, color);
        result.kind = module_option_kind::color_override;
        result.toggle = enabled;
        return result;
    }

    auto module_option::Text(const char* label, const char* description, std::string* value, const size_t limit) -> module_option
    {
        module_option result{};
        result.kind = module_option_kind::text_input;
        result.label = label;
        result.description = description;
        result.text = value;
        result.text_limit = limit;
        return result;
    }

    auto module_render_context::DrawFadingContainer(const ui::rect& bounds, const ui::container_style& style, const bool anchoredRight) const -> void
    {
        const ui::horizontal_fade fade{bounds, anchoredRight ? 0.0f : 1.0f, anchoredRight ? 1.0f : 0.0f};
        if (this->containers_ != nullptr) this->containers_->Draw(this->design_, bounds, style, {}, &fade);
    }

    auto module_option::Action(const char* label, const char* description, const char* buttonText, const module_option_group group) -> module_option
    {
        module_option result{};
        result.kind = module_option_kind::action;
        result.group = group;
        result.label = label;
        result.description = description;
        result.action_text = buttonText;
        return result;
    }

    auto module_option::Toggle(const char* label, const char* description, bool* value, const module_option_group group) -> module_option
    {
        module_option result{};
        result.kind = module_option_kind::toggle;
        result.group = group;
        result.label = label;
        result.description = description;
        result.toggle = value;
        return result;
    }

    auto module_option::Color(const char* label, const char* description, D3DCOLOR* value, const module_option_group group) -> module_option
    {
        module_option result{};
        result.kind = module_option_kind::color;
        result.group = group;
        result.label = label;
        result.description = description;
        result.color = value;
        return result;
    }

    auto module_option::Slider(
        const char* label,
        const char* description,
        float* value,
        const float minimum,
        const float maximum,
        const float step,
        const module_option_group group) -> module_option
    {
        module_option result{};
        result.kind = module_option_kind::slider;
        result.group = group;
        result.label = label;
        result.description = description;
        result.slider = value;
        result.minimum = minimum;
        result.maximum = maximum;
        result.step = step;
        return result;
    }

    auto module_option::Choice(
        const char* label,
        const char* description,
        int* value,
        const char* const* choices,
        const size_t count,
        const module_option_group group) -> module_option
    {
        module_option result{};
        result.kind = module_option_kind::choice;
        result.group = group;
        result.label = label;
        result.description = description;
        result.choice = value;
        result.choices = choices;
        result.choice_count = count;
        return result;
    }

    auto ModuleWidthOption(module_state& state) -> module_option
    {
        return module_option::Slider("Width", "HUD width in pixels.", &state.bounds.width, state.minimum_width, state.maximum_width, 1.0f, module_option_group::appearance);
    }

    auto TextProfileOption(const size_t index, module_text_profile& profile) -> module_option
    {
        static constexpr const char* placements[] = {"Default", "Left", "Center", "Right"};
        switch (index)
        {
        case 0: return module_option::Choice("Placement", "Horizontal text anchor.", &profile.placement, placements, 4, module_option_group::text);
        case 1: return TextSizeOption("Size (px)", "Font size in pixels, rendered at its native resolution.", &profile.style.scale);
        case 2: return module_option::Slider("X offset", "Horizontal adjustment.", &profile.offset_x, -200.0f, 200.0f, 1.0f, module_option_group::text);
        case 3: return module_option::Slider("Y offset", "Vertical adjustment.", &profile.offset_y, -100.0f, 100.0f, 1.0f, module_option_group::text);
        case 4: return module_option::Slider("Letter spacing", "Space between glyphs.", &profile.style.letter_spacing, -1.0f, 6.0f, 0.25f, module_option_group::text);
        case 5: return module_option::Color("Top color", "Upper text gradient.", &profile.style.top_color, module_option_group::text);
        case 6: return module_option::Color("Bottom color", "Lower text gradient.", &profile.style.bottom_color, module_option_group::text);
        case 7: return module_option::Toggle("Shadow", "Text drop shadow.", &profile.style.shadow.enabled, module_option_group::text);
        case 8: return module_option::Color("Shadow color", "Text shadow tint.", &profile.style.shadow.color, module_option_group::text);
        case 9: return module_option::Toggle("Soft shadow", "Blur the text shadow.", &profile.style.shadow.soft, module_option_group::text);
        case 10: return module_option::Toggle("Stroke", "Outline text glyphs.", &profile.style.stroke.enabled, module_option_group::text);
        case 11: return module_option::Color("Stroke color", "Text outline tint.", &profile.style.stroke.color, module_option_group::text);
        case 12: return module_option::Toggle("Glow", "Glow around text.", &profile.style.glow.enabled, module_option_group::text);
        case 13: return module_option::Color("Glow color", "Text glow tint.", &profile.style.glow.color, module_option_group::text);
        case 14:
        {
            auto option = module_option::Dropdown("Font", "Font face for this text element.", &profile.style.font, ui::font_names, ui::font_count);
            option.group = module_option_group::text;
            return option;
        }
        default: return {};
        }
    }

    auto TintTextProfile(const module_text_profile& profile, const D3DCOLOR tint) -> module_text_profile
    {
        auto result = profile;
        const auto multiply = [tint](D3DCOLOR color) {
            D3DCOLOR tinted{};
            for (uint32_t shift = 0; shift < 32; shift += 8)
                tinted |= ((((color >> shift) & 0xFF) * ((tint >> shift) & 0xFF) + 127) / 255) << shift;
            return tinted;
        };
        result.style.top_color = multiply(profile.style.top_color);
        result.style.bottom_color = multiply(profile.style.bottom_color);
        return result;
    }

    auto LoadTextProfile(const settings_store& settings, const char* section, const module_text_profile& fallback) -> module_text_profile
    {
        module_text_profile result = fallback;
        result.style = LoadTextStyle(settings, section, InheritGlobalTextStyle(settings, fallback.style));
        result.style.top_color = settings.ReadArgb(section, "top_color", result.style.top_color);
        result.style.bottom_color = settings.ReadArgb(section, "bottom_color", result.style.bottom_color);
        result.offset_x = std::clamp(settings.ReadFloat(section, "offset_x", result.offset_x), -200.0f, 200.0f);
        result.offset_y = std::clamp(settings.ReadFloat(section, "offset_y", result.offset_y), -100.0f, 100.0f);
        result.placement = std::clamp(settings.ReadInt(section, "placement", result.placement), 0, 3);
        return result;
    }

    auto SaveTextProfile(const settings_store& settings, const char* section, const module_text_profile& profile) -> void
    {
        settings.WriteInt(section, "style_version", 1);
        SaveTextStyle(settings, section, profile.style);
        settings.WriteArgb(section, "top_color", profile.style.top_color);
        settings.WriteArgb(section, "bottom_color", profile.style.bottom_color);
        settings.WriteFloat(section, "offset_x", profile.offset_x);
        settings.WriteFloat(section, "offset_y", profile.offset_y);
        settings.WriteInt(section, "placement", profile.placement);
    }

    auto MeterGradientTop(const D3DCOLOR color) -> D3DCOLOR
    {
        const auto red = static_cast<uint32_t>(static_cast<float>((color >> 16) & 0xFF) * 0.45f);
        const auto green = static_cast<uint32_t>(static_cast<float>((color >> 8) & 0xFF) * 0.45f);
        const auto blue = static_cast<uint32_t>(static_cast<float>(color & 0xFF) * 0.45f);
        return D3DCOLOR_ARGB((color >> 24) & 0xFF, red, green, blue);
    }

    auto MeterStyleOption(const size_t index, ui::meter_style& style) -> module_option
    {
        constexpr static const char* appearances[]{"Window theme", "Modern"};
        if (index == 0)
            return module_option::Choice("Bar style", "Follow the game's Window Type or use modern bars.", &style.appearance, appearances, std::size(appearances), module_option_group::appearance);
        if (index == 1)
            return module_option::Slider("Bar height", "Bar track thickness in pixels; text keeps its own size.", &style.height, 1.0f, 80.0f, 1.0f, module_option_group::appearance);
        const auto modern = [&style](module_option option) {
            option.available = style.appearance == ui::meter_style::modern;
            return option;
        };
        switch (index - 2)
        {
        case 0: return module_option::Toggle("Bookends", "Show end caps for the selected bar style.",
            style.appearance == ui::meter_style::window_theme ? &style.themed_bookends : &style.bookends, module_option_group::appearance);
        case 1: return modern(module_option::Slider("Corner radius", "Modern bars: rounding without caps.", &style.corner_radius, 0.0f, 50.0f, 0.5f, module_option_group::appearance));
        case 2: return modern(module_option::Slider("Border width", "Modern bars: outer outline thickness.", &style.border_width, 0.0f, 8.0f, 0.5f, module_option_group::appearance));
        case 3: return modern(module_option::Slider("Bookend width", "Modern bars: width of each end cap.", &style.bookend_width, 0.0f, 50.0f, 1.0f, module_option_group::appearance));
        case 4: return module_option::Toggle("Drop shadow", "Offset shadow behind bar.", &style.shadow_enabled, module_option_group::effects);
        case 5: return module_option::Slider("Shadow strength", "Shadow opacity.", &style.shadow_strength, 0.0f, 1.0f, 0.05f, module_option_group::effects);
        case 6: return module_option::Slider("Shadow X", "Horizontal shadow offset.", &style.shadow_offset_x, -10.0f, 10.0f, 1.0f, module_option_group::effects);
        case 7: return module_option::Slider("Shadow Y", "Vertical shadow offset.", &style.shadow_offset_y, -10.0f, 10.0f, 1.0f, module_option_group::effects);
        case 8: return module_option::Slider("Shadow radius", "Shadow blur radius.", &style.shadow_blur, 0.0f, 24.0f, 1.0f, module_option_group::effects);
        case 9: return module_option::Toggle("Outer glow", "Glow around meter edge.", &style.glow_enabled, module_option_group::appearance);
        case 10: return module_option::Slider("Glow size", "Outer glow spread.", &style.glow_size, 0.0f, 12.0f, 1.0f, module_option_group::appearance);
        case 11: return module_option::Toggle("Animated sheen", "Moving highlight sweep.", &style.sheen_enabled, module_option_group::effects);
        case 12: return module_option::Slider("Sheen width", "Highlight sweep width.", &style.sheen_width, 2.0f, 120.0f, 1.0f, module_option_group::effects);
        case 13: return module_option::Slider("Sheen speed", "Highlight cycles per second.", &style.sheen_speed, 0.0f, 2.0f, 0.05f, module_option_group::effects);
        case 14: return module_option::Toggle("Smooth fill", "Animate value changes.", &style.animation_enabled, module_option_group::appearance);
        case 15: return module_option::Slider("Fill response", "Fill animation speed.", &style.animation_speed, 1.0f, 30.0f, 0.5f, module_option_group::appearance);
        case 16: return modern(module_option::Color("Background top", "Modern bars: upper background gradient.", &style.track_top, module_option_group::colors));
        case 17: return modern(module_option::Color("Background bottom", "Modern bars: lower background gradient.", &style.track_bottom, module_option_group::colors));
        case 18: return modern(module_option::Color("Border color", "Modern bars: outer meter outline.", &style.border, module_option_group::colors));
        case 19: return modern(module_option::Color("Bookend top", "Modern bars: upper cap gradient.", &style.bookend_top, module_option_group::colors));
        case 20: return modern(module_option::Color("Bookend bottom", "Modern bars: lower cap gradient.", &style.bookend_bottom, module_option_group::colors));
        case 21: return module_option::Color("Shadow color", "Drop shadow tint.", &style.shadow, module_option_group::colors);
        case 22: return module_option::Color("Glow color", "Outer glow tint.", &style.glow, module_option_group::colors);
        case 23: return module_option::Color("Sheen color", "Animated highlight tint.", &style.sheen, module_option_group::colors);
        case 24: return module_option::Slider("Background opacity", "Background transparency.", &style.track_opacity, 0.0f, 1.0f, 0.05f, module_option_group::colors);
        case 25: return module_option::Toggle("Fill glow", "Glow follows the filled amount.", &style.fill_glow_enabled, module_option_group::effects);
        case 26: return module_option::Slider("Fill glow strength", "Fill glow opacity.", &style.fill_glow_strength, 0.0f, 1.0f, 0.05f, module_option_group::effects);
        case 27: return module_option::Slider("Fill glow radius", "Fill glow spread.", &style.fill_glow_radius, 0.0f, 20.0f, 1.0f, module_option_group::effects);
        default: return {};
        }
    }

    auto LoadMeterStyle(const settings_store& settings, const char* section, const ui::meter_style& fallback) -> ui::meter_style
    {
        ui::meter_style result = fallback;
        result.appearance = std::clamp(settings.ReadInt(section, "appearance", result.appearance), ui::meter_style::window_theme, ui::meter_style::modern);
        const int styleVersion = settings.ReadInt(section, "style_version", 0);
        result.height = std::clamp(settings.ReadFloat(section, "height", result.height), 0.0f, 80.0f);
        result.track_top = settings.ReadArgb(section, "track_top", result.track_top);
        result.track_bottom = settings.ReadArgb(section, "track_bottom", result.track_bottom);
        result.border = settings.ReadArgb(section, "border", result.border);
        if (styleVersion < 4 && result.border == D3DCOLOR_ARGB(255, 77, 70, 60))
            result.border = D3DCOLOR_ARGB(255, 0, 0, 0);
        result.bookend_top = settings.ReadArgb(section, "bookend_top", result.bookend_top);
        result.bookend_bottom = settings.ReadArgb(section, "bookend_bottom", result.bookend_bottom);
        result.shadow = settings.ReadArgb(section, "shadow", result.shadow);
        if (styleVersion < 2 && result.shadow == D3DCOLOR_ARGB(130, 0, 0, 0))
            result.shadow = D3DCOLOR_ARGB(255, 0, 0, 0);
        result.glow = settings.ReadArgb(section, "glow", result.glow);
        result.sheen = settings.ReadArgb(section, "sheen", result.sheen);
        result.border_width = std::clamp(settings.ReadFloat(section, "border_width", result.border_width), 0.0f, 8.0f);
        result.corner_radius = std::clamp(settings.ReadFloat(section, "corner_radius", result.corner_radius), 0.0f, 100.0f);
        result.bookend_width = std::clamp(settings.ReadFloat(section, "bookend_width", result.bookend_width), 0.0f, 100.0f);
        result.track_opacity = std::clamp(settings.ReadFloat(section, "track_opacity", result.track_opacity), 0.0f, 1.0f);
        result.shadow_offset_x = std::clamp(settings.ReadFloat(section, "shadow_offset_x", result.shadow_offset_x), -20.0f, 20.0f);
        result.shadow_offset_y = std::clamp(settings.ReadFloat(section, "shadow_offset_y", result.shadow_offset_y), -20.0f, 20.0f);
        result.shadow_blur = std::clamp(settings.ReadFloat(section, "shadow_blur", result.shadow_blur), 0.0f, 24.0f);
        result.shadow_strength = std::clamp(settings.ReadFloat(section, "shadow_strength", result.shadow_strength), 0.0f, 1.0f);
        result.glow_size = std::clamp(settings.ReadFloat(section, "glow_size", result.glow_size), 0.0f, 20.0f);
        result.fill_glow_radius = std::clamp(settings.ReadFloat(section, "fill_glow_radius", result.fill_glow_radius), 0.0f, 20.0f);
        result.fill_glow_strength = std::clamp(settings.ReadFloat(section, "fill_glow_strength", result.fill_glow_strength), 0.0f, 1.0f);
        result.sheen_width = std::clamp(settings.ReadFloat(section, "sheen_width", result.sheen_width), 1.0f, 400.0f);
        result.sheen_speed = std::clamp(settings.ReadFloat(section, "sheen_speed", result.sheen_speed), 0.0f, 5.0f);
        result.animation_speed = std::clamp(settings.ReadFloat(section, "animation_speed", result.animation_speed), 0.1f, 30.0f);
        result.bookends = settings.ReadBool(section, "bookends", result.bookends);
        result.themed_bookends = settings.ReadBool(section, "themed_bookends", result.themed_bookends);
        result.shadow_enabled = settings.ReadBool(section, "shadow_enabled", result.shadow_enabled);
        result.glow_enabled = settings.ReadBool(section, "glow_enabled", result.glow_enabled);
        result.fill_glow_enabled = settings.ReadBool(section, "fill_glow_enabled", result.fill_glow_enabled);
        result.sheen_enabled = settings.ReadBool(section, "sheen_enabled", result.sheen_enabled);
        result.animation_enabled = settings.ReadBool(section, "animation_enabled", result.animation_enabled);

        if (styleVersion > 0 && styleVersion < 3)
        {
            const float topAlpha = static_cast<float>((result.track_top >> 24) & 0xFF) / 255.0f;
            const float bottomAlpha = static_cast<float>((result.track_bottom >> 24) & 0xFF) / 255.0f;
            result.track_opacity = (topAlpha + bottomAlpha) * 0.5f;
            result.track_top |= 0xFF000000u;
            result.track_bottom |= 0xFF000000u;
            if (std::abs(result.corner_radius) < 0.001f)
                result.corner_radius = 8.0f;
            if (std::abs(result.border_width - 1.0f) < 0.001f)
                result.border_width = 2.0f;
            result.shadow_enabled = true;
            if (std::abs(result.shadow_strength - 0.55f) < 0.001f)
                result.shadow_strength = 0.2f;
            if (std::abs(result.shadow_offset_x - 2.0f) < 0.001f)
                result.shadow_offset_x = 0.0f;
            if (std::abs(result.shadow_offset_y - 2.0f) < 0.001f)
                result.shadow_offset_y = 1.0f;
            if (std::abs(result.shadow_blur - 2.0f) < 0.001f)
                result.shadow_blur = 4.0f;
        }
        return result;
    }

    auto SaveMeterStyle(const settings_store& settings, const char* section, const ui::meter_style& style) -> void
    {
        settings.WriteInt(section, "style_version", 5);
        settings.WriteInt(section, "appearance", style.appearance);
        settings.WriteFloat(section, "height", style.height);
        settings.WriteArgb(section, "track_top", style.track_top);
        settings.WriteArgb(section, "track_bottom", style.track_bottom);
        settings.WriteArgb(section, "border", style.border);
        settings.WriteArgb(section, "bookend_top", style.bookend_top);
        settings.WriteArgb(section, "bookend_bottom", style.bookend_bottom);
        settings.WriteArgb(section, "shadow", style.shadow);
        settings.WriteArgb(section, "glow", style.glow);
        settings.WriteArgb(section, "sheen", style.sheen);
        settings.WriteFloat(section, "border_width", style.border_width);
        settings.WriteFloat(section, "corner_radius", style.corner_radius);
        settings.WriteFloat(section, "bookend_width", style.bookend_width);
        settings.WriteFloat(section, "track_opacity", style.track_opacity);
        settings.WriteFloat(section, "shadow_offset_x", style.shadow_offset_x);
        settings.WriteFloat(section, "shadow_offset_y", style.shadow_offset_y);
        settings.WriteFloat(section, "shadow_blur", style.shadow_blur);
        settings.WriteFloat(section, "shadow_strength", style.shadow_strength);
        settings.WriteFloat(section, "glow_size", style.glow_size);
        settings.WriteFloat(section, "fill_glow_radius", style.fill_glow_radius);
        settings.WriteFloat(section, "fill_glow_strength", style.fill_glow_strength);
        settings.WriteFloat(section, "sheen_width", style.sheen_width);
        settings.WriteFloat(section, "sheen_speed", style.sheen_speed);
        settings.WriteFloat(section, "animation_speed", style.animation_speed);
        settings.WriteBool(section, "bookends", style.bookends);
        settings.WriteBool(section, "themed_bookends", style.themed_bookends);
        settings.WriteBool(section, "shadow_enabled", style.shadow_enabled);
        settings.WriteBool(section, "glow_enabled", style.glow_enabled);
        settings.WriteBool(section, "fill_glow_enabled", style.fill_glow_enabled);
        settings.WriteBool(section, "sheen_enabled", style.sheen_enabled);
        settings.WriteBool(section, "animation_enabled", style.animation_enabled);
    }

    module_render_context::module_render_context(ui::design_system& design, ui::container_renderer* containers, const bool editing, ui::game_icon_cache* icons, const ui::rect& viewport, ui::native_ui_assets* nativeAssets)
        : design_(design), containers_(containers), icons_(icons), native_assets_(nativeAssets), editing_(editing), viewport_(viewport) {}

    auto module_render_context::DrawButton(const ui::rect& bounds, const char* label, const bool highlighted, const module_text_profile& profile) const -> void
    {
        ui::button_renderer{this->design_, this->native_assets_}.Face(bounds, highlighted);
        auto text = profile;
        text.style = ui::button_renderer::TextStyle(this->design_.GetTheme(), text.style, highlighted);
        const ui::rect content{bounds.x + 5, bounds.y, std::max(0.0f, bounds.width - 10), bounds.height};
        this->DrawText(label, content, content, text);
    }

    auto module_render_context::DrawContainer(const ui::rect& bounds, const ui::container_style& style) const -> ui::rect
    {
        return this->containers_ != nullptr ? this->containers_->Draw(this->design_, bounds, style) : ui::container_renderer::ContentBounds(bounds, style);
    }

    auto module_render_context::DrawSelection(const ui::rect& bounds, const bool subtarget, const ui::selection_style& style) const -> void
    {
        if (this->native_assets_ == nullptr) return;
        const auto color = subtarget ? style.subtarget_color : style.target_color;
        this->native_assets_->DrawSelectionFrame(this->design_, bounds, color, style.background_opacity);
        if (style.arrow) this->native_assets_->DrawSelectionArrow(this->design_, bounds.x - 1, bounds.y + bounds.height * 0.5f, color);
    }

    auto module_render_context::DrawText(
        const char* text,
        const ui::rect& bounds,
        const ui::rect& placementBounds,
        const module_text_profile& profile) const -> void
    {
        const auto [positioned, style] = PositionText(bounds, placementBounds, profile);
        this->design_.DrawText(text, positioned, style);
    }

    auto module_render_context::TextBounds(const char* text, const ui::rect& bounds, const ui::rect& placementBounds, const module_text_profile& profile) const -> ui::rect
    {
        auto [positioned, style] = PositionText(bounds, placementBounds, profile);
        const float measured = this->design_.MeasureText(text, style), height = this->design_.TextHeight(style);
        const float factor = style.alignment == ui::text_alignment::right ? 1.0f : style.alignment == ui::text_alignment::center ? 0.5f : 0;
        positioned.x += std::max(0.0f, positioned.width - measured) * factor;
        positioned.y += std::max(0.0f, (positioned.height - height) * 0.5f);
        return {positioned.x, positioned.y, std::min(measured, std::max(0.0f, positioned.width)), height};
    }

    auto module_render_context::MeterSkin(const ui::meter_style& style) const -> std::optional<ui::meter_skin>
    {
        auto skin = style.appearance == ui::meter_style::window_theme && this->native_assets_ != nullptr && this->containers_ != nullptr
            ? this->native_assets_->MeterSkin() : std::nullopt;
        if (skin)
        {
            skin->background = this->containers_->MeterBackground();
            if (skin->background.texture == nullptr) skin.reset();
        }
        return skin;
    }

    auto module_render_context::DrawBar(const ui::rect& bounds, const float ratio, const D3DCOLOR fillTop, const D3DCOLOR fillBottom,
        const float phase, const ui::meter_style& style, const bool fillFromRight) const -> void
    {
        const auto skin = this->MeterSkin(style);
        this->design_.DrawMeter(ui::LayoutMeterTrack(bounds, style), ratio, fillTop, fillBottom, phase, style, skin ? &*skin : nullptr, {}, fillFromRight);
    }

    auto module_render_context::MeterTextBounds(const ui::rect& bounds, const ui::meter_style& style) const -> ui::rect
    {
        const auto skin = this->MeterSkin(style);
        return ui::LayoutMeterText(ui::LayoutMeterTrack(bounds, style), 0, 0, 0, ui::meter_text_anchor::automatic,
            ui::meter_text_anchor::automatic, skin ? &*skin : nullptr, style.themed_bookends).value;
    }

    auto module_render_context::DrawMeter(
        const ui::rect& bounds,
        const char* label,
        const char* value,
        const float ratio,
        const D3DCOLOR fillTop,
        const D3DCOLOR fillBottom,
        const ui::meter_style& style,
        meter_animation_state& animation,
        const bool overlayLabel,
        const module_text_profile* labelProfile,
        const module_text_profile* valueProfile,
        const ui::meter_text_anchor textAnchor,
        const ui::meter_text_anchor valueAnchor,
        const meter_fill_mode fillMode) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        const bool hasLabel = label != nullptr && label[0] != '\0';
        const float labelWidth = hasLabel && !overlayLabel ? 28.0f : 0.0f;
        if (labelWidth > 0.0f)
        {
            const ui::rect labelBounds{bounds.x, bounds.y, labelWidth - 3.0f, bounds.height};
            if (labelProfile != nullptr)
                this->DrawText(label, labelBounds, bounds, *labelProfile);
            else
                this->design_.DrawText(label, labelBounds, theme.meter_text);
        }

        const uint64_t now = ::GetTickCount64();
        const float target = std::clamp(ratio, 0.0f, 1.0f);
        if (!animation.initialized || !style.animation_enabled)
        {
            animation.displayed_ratio = target;
            animation.last_tick = now;
            animation.initialized = true;
        }
        else
        {
            const float elapsed = std::clamp(static_cast<float>(now - animation.last_tick) * 0.001f, 0.0f, 0.1f);
            const float response = 1.0f - std::exp(-std::max(0.1f, style.animation_speed) * elapsed);
            animation.displayed_ratio += (target - animation.displayed_ratio) * response;
            if (std::abs(target - animation.displayed_ratio) < 0.0005f)
                animation.displayed_ratio = target;
            animation.last_tick = now;
        }

        const auto track = ui::LayoutMeterTrack({bounds.x + labelWidth, bounds.y, bounds.width - labelWidth, bounds.height}, style);
        const float phase = std::fmod(static_cast<float>(now) * 0.001f * style.sheen_speed, 1.0f);
        const auto skin = this->MeterSkin(style);
        if (fillMode == meter_fill_mode::single)
            this->design_.DrawMeter(track, animation.displayed_ratio, fillTop, fillBottom, phase, style, skin ? &*skin : nullptr);
        else
        {
            const auto layers = ui::TPMeterLayers(animation.displayed_ratio, fillTop, fillBottom, now * 0.001,
                fillMode == meter_fill_mode::tp_blink && target >= 1.0f / 3.0f);
            this->design_.DrawMeter(track, animation.displayed_ratio, fillTop, fillBottom, phase, style, skin ? &*skin : nullptr, layers);
        }
        const auto& labelTextStyle = labelProfile != nullptr ? labelProfile->style : theme.meter_text;
        const auto& valueTextStyle = valueProfile != nullptr ? valueProfile->style : theme.meter_text;
        const auto textLayout = ui::LayoutMeterText(track, std::max(16.0f, this->design_.TextHeight(labelTextStyle)),
            std::max(16.0f, this->design_.TextHeight(valueTextStyle)),
            std::max(32.0f, hasLabel ? this->design_.MeasureText(label, labelTextStyle) + 2.0f : 0.0f), textAnchor, valueAnchor,
            skin ? &*skin : nullptr, style.themed_bookends);
        if (hasLabel && overlayLabel)
        {
            if (labelProfile != nullptr)
                this->DrawText(label, textLayout.label, textLayout.placement, *labelProfile);
            else
            {
                auto labelStyle = theme.meter_text;
                labelStyle.alignment = ui::text_alignment::left;
                this->design_.DrawText(label, textLayout.label, labelStyle);
            }
        }
        if (value != nullptr && value[0] != '\0')
        {
            if (valueProfile != nullptr)
                this->DrawText(value, textLayout.value, textLayout.placement, *valueProfile);
            else
            {
                auto valueStyle = theme.meter_text;
                valueStyle.alignment = ui::text_alignment::right;
                this->design_.DrawText(value, textLayout.value, valueStyle);
            }
        }
    }

    auto module_render_context::Ratio(const uint32_t value, const uint32_t maximum) -> float
    {
        return maximum == 0 ? 0.0f : std::clamp(static_cast<float>(value) / static_cast<float>(maximum), 0.0f, 1.0f);
    }
} // namespace xiui
