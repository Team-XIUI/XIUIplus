#include "command_menu_module.hpp"

#include <algorithm>

namespace xiui
{
    auto command_menu_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto native = this->native_;
        *this = {};
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
    }

    auto command_menu_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("commands", "enabled", this->state_.enabled);
        store.WriteRect("layout.commands", this->state_.bounds);
        store.WriteBool("commands", "position_initialized", this->position_initialized_);
        store.WriteInt("commands", "window_anchor", this->anchor_);
        SaveTextProfile(store, "text.commands.preview", this->text_);
    }

    auto command_menu_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Move command menus and align each connected window to the selected top or bottom edge.", "general"}};
        return sections;
    }

    auto command_menu_module::Option(const size_t index) -> module_option
    {
        static constexpr const char* anchors[]{"Top", "Bottom"};
        if (index == 1)
            return module_option::Choice("Window anchor", "Align command, magic, ability and detail windows along their top or bottom edges as their heights change.", &this->anchor_, anchors, 2).Ordered(200);
        if (index != 0) return {};
        auto option = module_option::Toggle("Move native menus", "Commands, item details and spell panels share this anchor. Off restores the game's position.", &this->state_.enabled).Ordered(-50);
        option.available = this->native_.menu_available;
        return option;
    }

    auto command_menu_module::Update(const native_ui_state& value, const float viewportWidth, const float viewportHeight) -> void
    {
        this->native_ = value;
        this->viewport_width_ = viewportWidth; this->viewport_height_ = viewportHeight;
        const float scaleX = value.reference_width > 0 ? viewportWidth / value.reference_width : 1;
        const float scaleY = value.reference_height > 0 ? viewportHeight / value.reference_height : 1;
        auto& bounds = this->state_.bounds;
        if (!this->position_initialized_ && value.menu_visible)
        {
            bounds = {value.x * scaleX, value.y * scaleY, value.width * scaleX, value.height * scaleY};
            this->position_initialized_ = true;
        }
        if (bounds.x < 0) { bounds.x = 16 * scaleX; bounds.width = 112 * scaleX; }
        if (bounds.y < 0) { bounds.height = 168 * scaleY; bounds.y = std::max(0.0f, viewportHeight - 152 * scaleY - bounds.height); }
        if (value.menu_visible)
        {
            const float edge = this->AnchorsTop() ? bounds.y : bounds.y + bounds.height;
            bounds.width = std::max(32.0f, value.width * scaleX);
            bounds.height = std::max(24.0f, value.height * scaleY);
            bounds.y = this->AnchorsTop() ? edge : std::max(0.0f, edge - bounds.height);
        }
    }

    auto command_menu_module::Layout(const bool visible) const -> native_menu_layout
    {
        const auto& bounds = this->state_.bounds;
        return {visible && this->state_.enabled && this->position_initialized_ && this->native_.menu_available && bounds.x >= 0 && bounds.y >= 0,
            bounds.x, bounds.y + bounds.height, this->viewport_width_, this->viewport_height_, this->AnchorsTop(), bounds.y};
    }

    auto command_menu_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        if (!context.IsEditing()) return;
        const auto& bounds = this->state_.bounds;
        if (!this->native_.menu_visible || !this->state_.enabled)
        {
            context.DrawContainer(bounds);
            auto text = this->text_;
            text.style.alignment = ui::text_alignment::center;
            context.DrawText("COMMAND MENU", bounds, bounds, text);
        }
        // Native draws precede the runtime; retain the whole panel as its edit hit area.
        this->state_.content_bounds = {};
    }
}
