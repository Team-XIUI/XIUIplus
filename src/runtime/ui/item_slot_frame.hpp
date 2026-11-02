#pragma once

#include "rounded_rect.hpp"

#include <optional>

namespace xiui::ui
{
    class item_slot_frame
    {
    public:
        item_slot_frame(const design_system& design, const float size, const float opacity, const std::optional<D3DCOLOR> background = {}) : design_(design)
        {
            const auto& colors = design.GetTheme().colors;
            this->style_ = {background.value_or(Fade(colors.surface, opacity * .7f)), Fade(colors.border, opacity * .45f), 1,
                1 / std::max(design.Transform().x, design.Transform().y)};
            for (size_t i = 0; i < this->contour_.size(); ++i)
                this->contour_[i] = RoundedRectPoint({0, 0, 1, 1}, 3 / size, i);
        }

        auto Draw(const rect& bounds, const float opacity = 1) const -> void
        {
            auto style = this->style_;
            style.color = Fade(style.color, opacity); style.edge_color = Fade(style.edge_color, opacity);
            this->design_.DrawContour(bounds, this->contour_, style);
        }

        auto Outline(const rect& bounds, const D3DCOLOR color, const float width = 1) const -> void
        {
            this->design_.DrawContour(bounds, this->contour_, {0, color, width, this->style_.feather});
        }

        auto Highlight(const rect& bounds, const D3DCOLOR color) const -> void
        {
            this->design_.DrawContour(bounds, this->contour_, {Fade(color, .14f), Fade(color, .8f), 1.5f, this->style_.feather});
        }

    private:
        static auto Fade(const D3DCOLOR color, const float opacity) -> D3DCOLOR
        { return (color & 0xFFFFFF) | (static_cast<uint32_t>(std::round((color >> 24) * opacity)) << 24); }

        const design_system& design_;
        contour_style style_{};
        std::array<point, rounded_rect_points> contour_{};
    };
}
