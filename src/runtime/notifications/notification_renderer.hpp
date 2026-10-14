#pragma once

#include "notification_center.hpp"
#include "design_system.hpp"
#include "ui/game_icon_cache.hpp"

namespace xiui { class module_render_context; }

namespace xiui
{
    struct notification_style : ui::text_style
    {
        notification_style(void)
        {
            this->scale = 0.95f;
            this->alignment = ui::text_alignment::center;
            this->stroke.enabled = true;
            this->stroke.color = 0xE8000000;
            this->shadow.color = 0xB0000000;
        }
        float duration{3.2f};
        float rise{36.0f};
        float visible_count{5.0f};
        float row_spacing{4.0f};
        bool center_vertically{};
        bool items{true};
        bool experience{true};
        bool skills{true};
        bool party_invites{true}, trade_invites{true}, key_items{true}, gil{true}, treasure{true};
        bool background{};
        D3DCOLOR item_color{0xFFF2F3FA};
        D3DCOLOR experience_color{0xFFFFDD83};
        D3DCOLOR skill_color{0xFF96D5FF};
        D3DCOLOR party_color{0xFFF2F3FA}, trade_color{0xFFF2F3FA}, key_item_color{0xFFF2F3FA}, gil_color{0xFFF2F3FA}, treasure_color{0xFFF2F3FA};
    };

    class notification_renderer
    {
    public:
        static auto KindEnabled(notification_kind kind, const notification_style& style) -> bool;
        static auto KindColor(notification_kind kind, const notification_style& style) -> D3DCOLOR;
        static auto ScreenBounds(const ui::rect& viewport, const ui::rect& bounds, float offsetX, float offsetY, ui::text_alignment alignment = ui::text_alignment::right) -> ui::rect;
        static auto Draw(const ui::design_system& design, const notification_center& center, const ui::rect& bounds, const notification_style& style, uint64_t now, ui::game_icon_cache* icons = nullptr, const module_render_context* context = nullptr) -> void;
    };
} // namespace xiui
