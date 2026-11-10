#include "localization/localization.hpp"
#include "command_menu_module.hpp"

#include <algorithm>
#include <vector>

namespace xiui
{
    auto command_menu_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto native = this->native_;
        const auto windows = this->windows_;
        *this = {};
        this->windows_ = windows;
        this->native_ = native;
        this->ApplyTextDefaults(theme);
    }

    auto command_menu_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("commands", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.commands", this->state_.bounds);
        this->position_initialized_ = store.ReadBool("commands", "position_initialized", false);
        this->anchor_ = std::clamp(store.ReadInt("commands", "window_anchor", 1), 0, 1);
        this->text_ = LoadTextProfile(store, "text.commands.preview", this->text_);
        this->selected_windows_.fill(0);
        for (size_t category = 0; category < this->selected_windows_.size(); ++category)
        {
            const auto key = store.ReadString("commands.windows", native_dialog_keys[category]);
            int ordinal{};
            for (const auto& window : native_menu_windows)
                if (static_cast<size_t>(window.category) == category)
                {
                    if (key == window.key) { this->selected_windows_[category] = ordinal; break; }
                    ++ordinal;
                }
        }
    }

    auto command_menu_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("commands", "enabled", this->state_.enabled);
        store.WriteRect("layout.commands", this->state_.bounds);
        store.WriteBool("commands", "position_initialized", this->position_initialized_);
        store.WriteInt("commands", "window_anchor", this->anchor_);
        SaveTextProfile(store, "text.commands.preview", this->text_);
        for (size_t category = 0; category < this->selected_windows_.size(); ++category)
        {
            int ordinal{};
            for (const auto& window : native_menu_windows)
                if (static_cast<size_t>(window.category) == category && ordinal++ == this->selected_windows_[category])
                { store.WriteString("commands.windows", native_dialog_keys[category], window.key); break; }
        }
    }

    namespace
    {
        struct menu_choices
        {
            std::vector<size_t> indices;
            std::vector<const char*> labels;
        };
        auto MenuChoices(const size_t category) -> const menu_choices&
        {
            static const auto choices = [] {
                std::array<menu_choices, static_cast<size_t>(native_dialog::count)> result{};
                for (size_t index = 0; index < std::size(native_menu_windows); ++index)
                {
                    auto& value = result[static_cast<size_t>(native_menu_windows[index].category)];
                    value.indices.push_back(index); value.labels.push_back(native_menu_windows[index].label);
                }
                return result;
            }();
            return choices[category];
        }
    }

    auto command_menu_module::SelectedWindow(void) const -> native_window_module*
    {
        const auto& choices = MenuChoices(this->section_);
        const auto selected = static_cast<size_t>(std::max(0, this->selected_windows_[this->section_]));
        return selected < choices.indices.size() && choices.indices[selected] < this->windows_.size() ? &this->windows_[choices.indices[selected]] : nullptr;
    }

    auto command_menu_module::SelectWindow(const size_t window) -> void
    {
        if (window >= std::size(native_menu_windows)) return;
        this->section_ = static_cast<size_t>(native_menu_windows[window].category);
        const auto& choices = MenuChoices(this->section_).indices;
        const auto selected = std::find(choices.begin(), choices.end(), window);
        if (selected != choices.end()) this->selected_windows_[this->section_] = static_cast<int>(selected - choices.begin());
    }

    auto command_menu_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static const auto sections = [] {
            std::array<module_config_section, static_cast<size_t>(native_dialog::count)> result{};
            for (size_t index = 0; index < result.size(); ++index)
                result[index] = {native_dialog_labels[index], index == static_cast<size_t>(native_dialog::status) ?
                    "The selector and result panels move independently. Character Status includes its EXP bar. Play Time appears in native chat." :
                    "Choose an individual native window. Moving or hiding it leaves other windows in this category unchanged.", native_dialog_keys[index]};
            return result;
        }();
        return sections;
    }

    auto command_menu_module::ContentOption(const size_t index) -> module_option
    {
        if (index >= 3) return this->SelectedWindow() != nullptr ? this->SelectedWindow()->Option(index - 3) : module_option{};
        if (index == 2)
        {
            const auto& choices = MenuChoices(this->section_);
            return module_option::Dropdown("Native window", "Select the exact window to edit. The submenu and each result panel have their own positions and visibility.",
                &this->selected_windows_[this->section_], choices.labels.data(), choices.labels.size()).Ordered(-30);
        }
        return {};
    }

    auto command_menu_module::Update(const native_ui_state& value, const float viewportWidth, const float viewportHeight) -> void
    {
        this->native_ = value;
        this->viewport_width_ = viewportWidth; this->viewport_height_ = viewportHeight;
    }

    auto command_menu_module::Layout(const bool) const -> native_menu_layout
    {
        const auto& bounds = this->state_.bounds;
        return {false,
            bounds.x, bounds.y + bounds.height, this->viewport_width_, this->viewport_height_, this->AnchorsTop(), bounds.y};
    }

    auto command_menu_module::Render(const hud_snapshot&, const module_render_context&) -> void {}
}
