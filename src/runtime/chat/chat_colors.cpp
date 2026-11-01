#include "chat_colors.hpp"

namespace xiui
{
    auto ChatNameColor(const D3DCOLOR color) -> D3DCOLOR
    {
        const auto red = (color >> 16) & 255, green = (color >> 8) & 255, blue = color & 255;
        const uint32_t peak = std::max({red, green, blue});
        if (peak == 0) return color;
        const auto gain = std::min(1080u, 255000u / peak);
        const auto lift = [gain](const uint32_t value) { return (value * gain + 500) / 1000; };
        return (color & 0xFF000000) | (lift(red) << 16) | (lift(green) << 8) | lift(blue);
    }

    auto ChatThemeColor(const uint32_t mode, const D3DCOLOR nativeColor, const bool xiv1) -> D3DCOLOR
    {
        if (!xiv1) return nativeColor;
        constexpr D3DCOLOR ivory = 0xFFEDE7D8, gold = 0xFFE0D17E, lavender = 0xFFBCA0D6, peach = 0xFFE3AF99;
        constexpr D3DCOLOR pink = 0xFFE7A1DF, purple = 0xFFB99AE8;
        constexpr std::array<D3DCOLOR, std::size(chat_types)> palette{
            pink, pink, ivory, 0xFFB4D4E1, 0xFFB5D0AF, 0xFFD8C6A0,
            peach, peach, 0xFFD3D4AB, purple, ivory, gold, gold};
        auto color = (mode & 255) == 8 ? peach : palette[ChatTypeIndex(mode)];
        const auto red = (nativeColor >> 16) & 255, green = (nativeColor >> 8) & 255, blue = nativeColor & 255;
        if (ChatTypeIndex(mode) >= 11 && blue > green + 20 && red > green + 10) color = lavender;
        return (color & 0xFFFFFF) | (nativeColor != 0 ? nativeColor & 0xFF000000 : 0xFF000000);
    }

    auto chat_color_overrides::Load(const settings_store& store) -> void
    {
        const chat_color_overrides defaults;
        for (size_t i = 0; i < this->enabled.size(); ++i)
        {
            const auto section = std::string{"chat.color."} + chat_types[i].key;
            this->enabled[i] = store.ReadBool(section.c_str(), "override", false);
            this->colors[i] = store.ReadArgb(section.c_str(), "color", defaults.colors[i]);
        }
    }

    auto chat_color_overrides::Save(const settings_store& store) const -> void
    {
        for (size_t i = 0; i < this->enabled.size(); ++i)
        {
            const auto section = std::string{"chat.color."} + chat_types[i].key;
            store.WriteBool(section.c_str(), "override", this->enabled[i]);
            store.WriteArgb(section.c_str(), "color", this->colors[i]);
        }
    }

    auto chat_color_overrides::Resolve(const uint32_t mode, const D3DCOLOR nativeColor, const D3DCOLOR inlineColor) const -> D3DCOLOR
    {
        const auto type = ChatTypeIndex(mode);
        return this->enabled[type] ? this->colors[type] : inlineColor != 0 ? inlineColor : nativeColor;
    }
}
