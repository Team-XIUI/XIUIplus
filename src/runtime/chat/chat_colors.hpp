#pragma once

#include "chat_model.hpp"
#include "settings/settings_store.hpp"

namespace xiui
{
    class chat_color_overrides
    {
    public:
        std::array<bool, std::size(chat_types)> enabled{};
        std::array<D3DCOLOR, std::size(chat_types)> colors{
            0xFFB8A0FF, 0xFFB8A0FF, 0xFFFFFFFF, 0xFF80E8FF, 0xFF90E890, 0xFFFFC080,
            0xFFFFB080, 0xFFFFB080, 0xFFFFC080, 0xFFB0B0D0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Resolve(uint32_t mode, D3DCOLOR nativeColor, D3DCOLOR inlineColor = 0) const -> D3DCOLOR;
    };
}
