#pragma once

#include "Ashita.h"

namespace xiui::ui
{
    class design_system;
    struct rect;

    enum class icon_glyph : uint8_t
    {
        player, target, enemies, experience, notifications, container, gamehud, chat, chat2,
        compass, world, commands, reset, general, party, inventory, equipment, extras, gil, readycheck, castbar, castcost,
        chevron_down, chevron_up, add, close, more, play, attribution, github, target_arrow, eye, eye_closed,
        arrow_left, arrow_right, statuses, keyboard, controller, pet, minimap, search, center_x, center_y,
        unknown
    };

    class icon_atlas
    {
    public:
        ~icon_atlas(void) { this->Shutdown(); }
        icon_atlas(void) = default;
        icon_atlas(const icon_atlas&) = delete;
        auto operator=(const icon_atlas&) -> icon_atlas& = delete;
        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Shutdown(void) -> void;
        auto Draw(const design_system& design, const rect& bounds, icon_glyph icon, D3DCOLOR color, int quarterTurns = 0, bool flipX = false) const -> void;

    private:
        IDirect3DDevice8* device_{};
        IDirect3DTexture8* texture_{};
    };
}
