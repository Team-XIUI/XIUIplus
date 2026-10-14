#include "chat_colors.hpp"

namespace xiui
{
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
