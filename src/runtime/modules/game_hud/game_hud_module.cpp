#include "game_hud_module.hpp"

#include <string>

namespace xiui
{
    auto game_hud_module::ResetDefaults(const ui::theme&) -> void
    {
        this->preferences_ = this->defaults_;
        this->instant_macro_bar_ = this->default_instant_macro_bar_;
    }

    auto game_hud_module::SetDefaults(const settings_store& store) -> void
    {
        this->defaults_.enabled = store.ReadBool("game_hud", "enabled", true);
        this->default_instant_macro_bar_ = store.ReadBool("game_hud", "instant_macro_bar", false);
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            const auto key = std::string{"show_"} + native_hud_options[index].key;
            this->defaults_.shown[index] = store.ReadBool("game_hud", key.c_str(), true);
        }
    }

    auto game_hud_module::Load(const settings_store& store) -> void
    {
        this->preferences_.enabled = store.ReadBool("game_hud", "enabled", this->defaults_.enabled);
        this->instant_macro_bar_ = store.ReadBool("game_hud", "instant_macro_bar", this->default_instant_macro_bar_);
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            const auto key = std::string{"show_"} + native_hud_options[index].key;
            this->preferences_.shown[index] = store.ReadBool("game_hud", key.c_str(), this->defaults_.shown[index]);
        }
    }

    auto game_hud_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("game_hud", "enabled", this->preferences_.enabled);
        store.WriteBool("game_hud", "instant_macro_bar", this->instant_macro_bar_);
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            const auto key = std::string{"show_"} + native_hud_options[index].key;
            store.WriteBool("game_hud", key.c_str(), this->preferences_.shown[index]);
        }
    }

    auto game_hud_module::ContentOption(const size_t index) -> module_option
    {
        if (index == 0)
            return module_option::Toggle("Native menu controls", "Apply the native HUD visibility choices below.", &this->preferences_.enabled).Ordered(-100);
        if (index == instant_macro_option)
        {
            const bool available = (this->available_ & native_instant_macro_palette) != 0;
            auto result = module_option::Toggle("Instant native macro bar", available ?
                "Open the native Ctrl/Alt macro bar immediately. Applies while Replace native macros is off; command waits and cooldowns stay unchanged." :
                "Unavailable on this client or another addon changed the macro bar code.", &this->instant_macro_bar_).Ordered(-80);
            result.available = available;
            return result;
        }
        if (index == hide_all_option || index == show_all_option)
        {
            auto result = index == hide_all_option ?
                module_option::Action("Hide native UI", "Hide every supported native HUD element, including native chat. XIUI+ modules keep their own visibility settings.", "Hide all").Ordered(-90) :
                module_option::Action("Restore native UI", "Show every supported native HUD element again.", "Show all").Ordered(-89);
            result.available = this->available_ != 0;
            return result;
        }
        if (index > native_hud_options.size())
            return {};
        const size_t element = index - 1;
        const bool available = (this->available_ & (uint32_t{1} << element)) != 0;
        auto result = module_option::Toggle(native_hud_options[element].label,
            available ? native_hud_options[element].description : "Unavailable on this client.", &this->preferences_.shown[element]);
        result.available = available;
        return result;
    }

    auto game_hud_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (index == hide_all_option) this->HideAll();
        else if (index == show_all_option) this->ShowAll();
    }

    auto game_hud_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Choose which native game windows are visible.", "general"}};
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
