#include "focus_frame.hpp"
#include "rounded_rect.hpp"

#include <algorithm>
#include <array>

namespace
{
    struct focus_vertex { float x, y, z{0}, rhw{1}; D3DCOLOR color; };
    constexpr size_t contour_points = xiui::ui::rounded_rect_points;

    auto contour(const xiui::ui::rect& bounds, const float inset, const float radius, const size_t index) -> xiui::ui::point
    {
        const auto value = xiui::ui::RoundedRectPoint(bounds, radius, index, inset);
        return {value.x - 0.5f, value.y - 0.5f};
    }
}

namespace xiui::ui
{
    auto design_system::DrawFocusFrame(const rect& bounds, const focus_frame_style& style) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || bounds.width <= 0 || bounds.height <= 0) return;
        const rect frame{bounds.x - style.padding, bounds.y - style.padding,
            bounds.width + style.padding * 2, bounds.height + style.padding * 2};
        const float thickness = std::min(std::clamp(style.thickness, 1.0f, 6.0f), std::min(frame.width, frame.height) * 0.5f);
        const float radius = std::clamp(style.radius, 0.0f, std::min(frame.width, frame.height) * 0.5f);
        this->ConfigureShapePipeline();
        const auto stroke = [&](const rect& area, const D3DCOLOR color, const float softness) {
            if ((color >> 24) == 0) return;
            const float feather = std::max(0.5f, softness * 0.5f);
            const float core = std::min(feather, thickness * 0.5f);
            const std::array offsets{-feather, core, thickness - core, thickness + feather};
            const std::array colors{color & 0x00FFFFFF, color, color, color & 0x00FFFFFF};
            std::array<focus_vertex, contour_points * 18> vertices{};
            size_t count{};
            for (size_t band = 0; band < 3; ++band)
                for (size_t index = 0; index < contour_points; ++index)
                {
                    const size_t next = (index + 1) % contour_points;
                    const auto a = contour(area, offsets[band], radius, index), b = contour(area, offsets[band], radius, next);
                    const auto c = contour(area, offsets[band + 1], radius, index), d = contour(area, offsets[band + 1], radius, next);
                    const focus_vertex corners[]{{a.x, a.y, 0, 1, colors[band]}, {b.x, b.y, 0, 1, colors[band]},
                        {c.x, c.y, 0, 1, colors[band + 1]}, {d.x, d.y, 0, 1, colors[band + 1]}};
                    for (const auto vertex : {0, 1, 2, 2, 1, 3}) vertices[count++] = corners[vertex];
                }
            this->device_->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(count / 3), vertices.data(), sizeof(focus_vertex));
        };
        if (style.shadow)
            stroke({frame.x + style.shadow_x, frame.y + style.shadow_y, frame.width, frame.height},
                WithAlpha(style.shadow_color, style.shadow_opacity), style.shadow_size * 2);
        if (style.glow) stroke(frame, WithAlpha(style.glow_color, style.glow_opacity), style.glow_size * 2);
        stroke(frame, style.color, 1);
    }
}
