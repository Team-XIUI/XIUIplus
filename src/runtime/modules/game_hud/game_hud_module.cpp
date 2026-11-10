#include "game_hud_module.hpp"

#include <string>

namespace xiui
{
    auto game_hud_module::ResetDefaults(const ui::theme&) -> void
    {
        this->preferences_ = this->defaults_;
        this->instant_macro_bar_ = this->default_instant_macro_bar_;
        this->visibility_edits_ |= (uint32_t{1} << native_hud_window_count) - 1;
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
        if (index == reset_positions_option)
            return module_option::Action("Reset All Native Windows", "Restore default positions for every native HUD window, menu and dialog. Visibility choices are kept.", "Reset all").Ordered(5);
        if (index == instant_macro_option + 7) return this->names_ != nullptr ? this->names_->Option(this->name_option_) : module_option{};
        if (index == instant_macro_option + 8) return this->macros_ != nullptr ? this->macros_->Option(this->macro_option_) : module_option{};
        if (index == instant_macro_option + 1)
        {
            static const auto labels = [] { std::array<const char*, native_hud_window_count> result{}; for (size_t i = 0; i < result.size(); ++i) result[i] = native_hud_options[i].label; return result; }();
            return module_option::Dropdown("Native window", "Choose a native HUD window. Position controls require a rectangle captured from the actual game UI.", &this->selected_window_, labels.data(), labels.size()).Ordered(-100);
        }
        if (index > instant_macro_option + 1)
            return this->SelectedWindow() != nullptr ? this->SelectedWindow()->Option(index - instant_macro_option - 2) : module_option{};
        if (index == 0)
            return module_option::Toggle("Apply visibility choices", "Apply the native HUD visibility choices below.", &this->preferences_.enabled).Ordered(-100);
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
        if (index == 0 || index == hide_all_option || index == show_all_option) this->visibility_edits_ |= (uint32_t{1} << native_hud_window_count) - 1;
        else if (index >= 1 && index <= native_hud_window_count) this->visibility_edits_ |= uint32_t{1} << (index - 1);
        if (index == reset_positions_option)
            for (auto& window : this->windows_) window.OnOptionChanged(4);
        else if (index == instant_macro_option + 7 && this->names_ != nullptr) this->names_->OnOptionChanged(this->name_option_);
        else if (index == instant_macro_option + 8 && this->macros_ != nullptr) this->macros_->OnOptionChanged(this->macro_option_);
        else if (index > instant_macro_option + 1 && this->SelectedWindow() != nullptr) this->SelectedWindow()->OnOptionChanged(index - instant_macro_option - 2);
        if (index == hide_all_option) this->HideAll();
        else if (index == show_all_option) this->ShowAll();
    }

    auto game_hud_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"Visibility", "Choose which native game windows are visible.", "general"},
            {"Window Placement", "Select a native HUD window, set its position or restore its normal placement. Native UI Edit Mode provides draggable outlines.", "placement"},
            {"Native Macro Bar", "Control the native macro palette and its replacement by XIUI+ action bars.", "macros"}};
        return sections;
    }

    auto game_hud_module::SetShown(const std::string_view key, const bool shown) -> bool
    {
        for (size_t index = 0; index < native_hud_options.size(); ++index)
        {
            if (key == native_hud_options[index].key)
            {
                this->preferences_.shown[index] = shown;
                this->visibility_edits_ |= uint32_t{1} << index;
                return true;
            }
        }
        return false;
    }
} // namespace xiui
