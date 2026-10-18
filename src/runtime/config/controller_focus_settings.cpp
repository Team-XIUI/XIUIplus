#include "controller_focus_settings.hpp"

#include "modules/module.hpp"

#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    namespace { constexpr auto section = "controller.focus"; }

    auto controller_focus_settings::Reset(const D3DCOLOR highlight) -> void
    {
        this->default_color_ = highlight;
        this->style = {};
        this->style.color = this->style.glow_color = highlight;
    }

    auto controller_focus_settings::Load(const settings_store& store) -> void
    {
        this->Reset(this->default_color_);
        const auto read = [&](const char* key, const float value, const float minimum, const float maximum) {
            const auto saved = store.ReadFloat(section, key, value);
            return std::isfinite(saved) ? std::clamp(saved, minimum, maximum) : value;
        };
        auto& frame = this->style;
        frame.color = store.ReadArgb(section, "color", frame.color);
        frame.thickness = read("thickness", frame.thickness, 1, 6);
        frame.radius = read("radius", frame.radius, 0, 12);
        frame.padding = read("padding", frame.padding, 0, 8);
        frame.glow = store.ReadBool(section, "glow", frame.glow);
        frame.glow_color = store.ReadArgb(section, "glow_color", frame.glow_color);
        if (store.ReadInt(section, "color_version", 0) < 1)
        {
            if (frame.color == 0xFFF0F5FF) frame.color = this->default_color_;
            if (frame.glow_color == 0xFFA8CCFF) frame.glow_color = this->default_color_;
        }
        frame.glow_opacity = read("glow_opacity", frame.glow_opacity, 0, 1);
        frame.glow_size = read("glow_size", frame.glow_size, 0, 12);
        frame.shadow = store.ReadBool(section, "shadow", frame.shadow);
        frame.shadow_color = store.ReadArgb(section, "shadow_color", frame.shadow_color);
        frame.shadow_opacity = read("shadow_opacity", frame.shadow_opacity, 0, 1);
        frame.shadow_size = read("shadow_size", frame.shadow_size, 0, 12);
        frame.shadow_x = read("shadow_x", frame.shadow_x, -8, 8);
        frame.shadow_y = read("shadow_y", frame.shadow_y, -8, 8);
    }

    auto controller_focus_settings::Save(const settings_store& store) const -> void
    {
        const auto& frame = this->style;
        store.WriteInt(section, "color_version", 1);
        store.WriteArgb(section, "color", frame.color);
        store.WriteFloat(section, "thickness", frame.thickness);
        store.WriteFloat(section, "radius", frame.radius);
        store.WriteFloat(section, "padding", frame.padding);
        store.WriteBool(section, "glow", frame.glow);
        store.WriteArgb(section, "glow_color", frame.glow_color);
        store.WriteFloat(section, "glow_opacity", frame.glow_opacity);
        store.WriteFloat(section, "glow_size", frame.glow_size);
        store.WriteBool(section, "shadow", frame.shadow);
        store.WriteArgb(section, "shadow_color", frame.shadow_color);
        store.WriteFloat(section, "shadow_opacity", frame.shadow_opacity);
        store.WriteFloat(section, "shadow_size", frame.shadow_size);
        store.WriteFloat(section, "shadow_x", frame.shadow_x);
        store.WriteFloat(section, "shadow_y", frame.shadow_y);
    }

    auto controller_focus_settings::Option(const size_t index) -> module_option
    {
        auto& frame = this->style;
        constexpr auto scope = "Shared by controller focus in the HUD and configuration. Changes apply immediately.";
        switch (index)
        {
        case 0: return module_option::Color("Border color", scope, &frame.color).Ordered(530);
        case 1: return module_option::Slider("Border thickness", scope, &frame.thickness, 1, 6, 0.5f).Ordered(461);
        case 2: return module_option::Slider("Corner radius", scope, &frame.radius, 0, 12, 1).Ordered(460);
        case 3: return module_option::Slider("Border padding", "Space around the focused item, in pixels.", &frame.padding, 0, 8, 1).Ordered(402);
        case 4: return module_option::Toggle("Glow", scope, &frame.glow).Ordered(700, true);
        case 5: return module_option::Color("Glow color", scope, &frame.glow_color).Ordered(701).VisibleWhen(frame.glow);
        case 6: return module_option::Slider("Glow opacity", scope, &frame.glow_opacity, 0, 1, 0.05f).Ordered(702).VisibleWhen(frame.glow);
        case 7: return module_option::Slider("Glow spread", scope, &frame.glow_size, 0, 12, 1).Ordered(702).VisibleWhen(frame.glow);
        case 8: return module_option::Toggle("Drop shadow", scope, &frame.shadow).Ordered(600, true);
        case 9: return module_option::Color("Shadow color", scope, &frame.shadow_color).Ordered(601).VisibleWhen(frame.shadow);
        case 10: return module_option::Slider("Shadow opacity", scope, &frame.shadow_opacity, 0, 1, 0.05f).Ordered(602).VisibleWhen(frame.shadow);
        case 11: return module_option::Slider("Shadow softness", scope, &frame.shadow_size, 0, 12, 1).Ordered(603).VisibleWhen(frame.shadow);
        case 12: return module_option::Slider("Shadow X offset", scope, &frame.shadow_x, -8, 8, 1).Ordered(606, true).VisibleWhen(frame.shadow);
        case 13: return module_option::Slider("Shadow Y offset", scope, &frame.shadow_y, -8, 8, 1).Ordered(607).VisibleWhen(frame.shadow);
        default: return {};
        }
    }
}
