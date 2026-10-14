#include "game_hud_module.hpp"

#include <string>

namespace xiui
{
    auto game_hud_module::ResetDefaults(const ui::theme&) -> void
    {
        this->preferences_ = {};
    }

    auto game_hud_module::Load(const settings_store& store) -> void
    {
        this->preferences_.enabled = store.ReadBool("game_hud", "enabled", true);
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            const auto key = std::string{"show_"} + native_hud_options[index].key;
            this->preferences_.shown[index] = store.ReadBool("game_hud", key.c_str(), true);
        }
    }

    auto game_hud_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("game_hud", "enabled", this->preferences_.enabled);
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            const auto key = std::string{"show_"} + native_hud_options[index].key;
            store.WriteBool("game_hud", key.c_str(), this->preferences_.shown[index]);
        }
    }

    auto game_hud_module::Option(const size_t index) -> module_option
    {
        if (index == 0)
            return module_option::Toggle("Game HUD controls", "Apply the native HUD visibility choices below.", &this->preferences_.enabled);
        if (index > native_hud_options.size())
            return {};
        const size_t element = index - 1;
        const bool available = (this->available_ & (uint32_t{1} << element)) != 0;
        auto result = module_option::Toggle(native_hud_options[element].label,
            available ? native_hud_options[element].description : "Unavailable on this client.", &this->preferences_.shown[element]);
        result.available = available;
        return result;
    }

    auto game_hud_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Choose which native game windows are visible."}};
        return sections;
    }

    auto game_hud_module::SetShown(const std::string_view key, const bool shown) -> bool
    {
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            if (key == native_hud_options[index].key && (this->available_ & (uint32_t{1} << index)) != 0)
            {
                this->preferences_.shown[index] = shown;
                return true;
            }
        }
        return false;
    }
} // namespace xiui
