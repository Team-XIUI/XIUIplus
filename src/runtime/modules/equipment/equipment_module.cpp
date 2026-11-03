#include "localization/localization.hpp"
#include "equipment_module.hpp"
#include "ui/game_icon_cache.hpp"
#include "ui/item_slot_frame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    auto equipment_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.caption_text},
            module_text_profile{theme.meter_text}, module_text_profile{theme.body_text}};
        this->text_[0].style.scale = 0.8f;
        this->text_[1].style.scale = 0.55f;
        this->text_[1].style.alignment = ui::text_alignment::center;
        this->text_[2].style.scale = 0.65f;
        this->text_[2].style.alignment = ui::text_alignment::right;
        this->text_[3].style.scale = 0.8f;
    }

    auto equipment_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        *this = {};
        this->state_.bounds = bounds;
        this->source_ = source;
        this->ApplyTextDefaults(theme);
    }

    auto equipment_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("equipment", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.equipment", this->state_.bounds);
        this->heading_ = store.ReadBool("equipment", "heading", true);
        this->ammo_ = store.ReadBool("equipment", "ammo_count", true);
        this->empty_labels_ = store.ReadBool("equipment", "empty_labels", true);
        this->slot_labels_ = store.ReadBool("equipment", "slot_labels", false);
        this->tooltips_ = store.ReadBool("equipment", "tooltips", true);
        this->background_ = store.ReadBool("equipment", "background", true);
        this->slot_frames_ = store.ReadBool("equipment", "slot_frames", true);
        if (store.ReadInt("equipment", "container_version", 0) < 1 && this->slot_frames_)
            this->background_ = true;
        this->layout_ = std::clamp(store.ReadInt("equipment", "layout", 0), 0, 2);
        this->selected_text_ = std::clamp(store.ReadInt("equipment", "selected_text", 0), 0, 3);
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const float value = store.ReadFloat("equipment", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->icon_size_ = number("icon_size", 32, 16, 64);
        this->spacing_ = number("spacing", 4, 0, 24);
        this->inset_ = number("icon_inset", 2, 0, 12);
        this->container_.opacity = number("opacity", 1, 0, 1);
        this->container_.padding = number("padding", 6, 0, 24);
        this->icon_opacity_ = number("icon_opacity", 1, 0.1f, 1);
        static constexpr const char* profiles[]{"text.equipment.heading", "text.equipment.labels", "text.equipment.ammo", "text.equipment.tooltip"};
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, profiles[i], this->text_[i]);
    }

    auto equipment_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("equipment", "enabled", this->state_.enabled);
        store.WriteRect("layout.equipment", this->state_.bounds);
        store.WriteBool("equipment", "heading", this->heading_);
        store.WriteBool("equipment", "ammo_count", this->ammo_);
        store.WriteBool("equipment", "empty_labels", this->empty_labels_);
        store.WriteBool("equipment", "slot_labels", this->slot_labels_);
        store.WriteBool("equipment", "tooltips", this->tooltips_);
        store.WriteBool("equipment", "background", this->background_);
        store.WriteInt("equipment", "container_version", 1);
        store.WriteBool("equipment", "slot_frames", this->slot_frames_);
        store.WriteInt("equipment", "layout", this->layout_);
        store.WriteInt("equipment", "selected_text", this->selected_text_);
        store.WriteFloat("equipment", "icon_size", this->icon_size_);
        store.WriteFloat("equipment", "spacing", this->spacing_);
        store.WriteFloat("equipment", "icon_inset", this->inset_);
        store.WriteFloat("equipment", "opacity", this->container_.opacity);
        store.WriteFloat("equipment", "padding", this->container_.padding);
        store.WriteFloat("equipment", "icon_opacity", this->icon_opacity_);
        static constexpr const char* profiles[]{"text.equipment.heading", "text.equipment.labels", "text.equipment.ammo", "text.equipment.tooltip"};
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, profiles[i], this->text_[i]);
    }

    auto equipment_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Your actual equipped items, including items equipped from Wardrobes.", "general"},
            {"Layout", "The native equipment order, arranged into a compact movable grid.", "layout"},
            {"Appearance", "Native DAT containers and cached game item icons.", "appearance"},
            {"Text", "Heading, slot labels, ammo counts and item-name tooltips.", "text"}};
        return sections;
    }

    auto equipment_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        if (index <= tooltips) return section == 0;
        if (index <= inset) return section == 1;
        if (index <= icon_opacity) return section == 2;
        return section == 3;
    }

    auto equipment_module::ContentOption(const size_t index) -> module_option
    {
        static constexpr const char* layouts[]{"4 x 4", "8 x 2", "2 x 8"};
        static constexpr const char* profiles[]{"Heading", "Slot labels", "Ammo count", "Item tooltip"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show all sixteen equipment slots with their native item icons.", &this->state_.enabled).Ordered(-50);
        case heading: return module_option::Toggle("Heading", "Show Equipment above the grid.", &this->heading_);
        case ammo: return module_option::Toggle("Ammo count", "Show the actual stack count over the Ammo slot.", &this->ammo_);
        case empty_labels: return module_option::Toggle("Empty-slot labels", "Show a slot name when no item is equipped.", &this->empty_labels_).VisibleWhen(!this->slot_labels_);
        case slot_labels: return module_option::Toggle("Slot labels", "Show each slot's name below its icon.", &this->slot_labels_);
        case tooltips: return module_option::Toggle("Item tooltips", "Hover a slot to see its item name and source bag; does not capture input.", &this->tooltips_);
        case icon_size: return module_option::Slider("Icon size", "Native item icon size in pixels.", &this->icon_size_, 16, 64, 1).Ordered(110);
        case layout: return module_option::Choice("Grid", "Keep the native equipment order while changing row length.", &this->layout_, layouts, 3).Ordered(-30);
        case spacing: return module_option::Slider("Slot spacing", "Pixels between equipment slots.", &this->spacing_, 0, 24, 1).Ordered(300, true);
        case inset: return module_option::Slider("Icon padding", "Pixels between the slot outline and item icon.", &this->inset_, 0, 12, 1).Ordered(402);
        case background: return module_option::Toggle("Container", "Use the shared container for the complete equipment window.", &this->background_, module_option_group::appearance).Ordered(-10);
        case slot_frames: return module_option::Toggle("Slot frames", "Simple rounded slot backgrounds and outlines. Native item quality borders remain part of each icon.", &this->slot_frames_, module_option_group::appearance).Ordered(470);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the outer container and slot frames.", &this->container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450).VisibleWhen(this->background_ || this->slot_frames_);
        case padding: return module_option::Slider("Container padding", "Space inside the equipment window's native frame.", &this->container_.padding, 0, 24, 1, module_option_group::appearance).Ordered(402).VisibleWhen(this->background_);
        case icon_opacity: return module_option::Slider("Icon opacity", "Opacity of the equipment icons.", &this->icon_opacity_, 0.1f, 1, 0.05f, module_option_group::appearance).Ordered(450);
        case selected_text: return module_option::Element("Selected text", "Configure each text element independently.", &this->selected_text_, profiles, 4, module_option_group::text);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]).VisibleWhen(
            this->selected_text_ == 0 ? this->heading_ : this->selected_text_ == 2 ? this->ammo_ : this->selected_text_ == 3 ? this->tooltips_ : true);
        }
    }

    auto equipment_module::HeadingHeight(void) const -> float { return this->heading_ ? std::max(16.0f, this->text_[0].style.scale * 18) + 6 : 0; }
    auto equipment_module::LabelHeight(void) const -> float { return this->slot_labels_ ? std::max(12.0f, this->text_[1].style.scale * 18) + 3 : 0; }

    auto equipment_module::SlotBounds(const size_t gridIndex) const -> ui::rect
    {
        return {this->state_.bounds.x + this->OuterInset() + (gridIndex % this->Columns()) * (this->CellSize() + this->spacing_),
            this->state_.bounds.y + this->OuterInset() + this->HeadingHeight() + (gridIndex / this->Columns()) * (this->CellSize() + this->LabelHeight() + this->spacing_),
            this->CellSize(), this->CellSize()};
    }

    auto equipment_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const auto rows = equipment_grid_order.size() / this->Columns();
        this->state_.bounds.width = this->OuterInset() * 2 + this->Columns() * (this->CellSize() + this->spacing_) - this->spacing_;
        this->state_.bounds.height = this->OuterInset() * 2 + this->HeadingHeight() + rows * (this->CellSize() + this->LabelHeight() + this->spacing_) - this->spacing_;
        if (this->background_) context.DrawContainer(this->state_.bounds, this->container_);
        if (this->heading_)
        {
            const ui::rect at{this->state_.bounds.x + this->OuterInset(), this->state_.bounds.y + this->OuterInset(),
                this->state_.bounds.width - this->OuterInset() * 2, this->HeadingHeight() - 4};
            context.DrawText(i18n::Text("Equipment"), at, at, this->text_[0]);
        }
        const auto& design = context.Design();
        const ui::item_slot_frame slotFrame{design, this->CellSize(), this->container_.opacity};
        for (size_t cell = 0; cell < equipment_grid_order.size(); ++cell)
        {
            const auto slot = equipment_grid_order[cell];
            const auto bounds = this->SlotBounds(cell);
            const auto item = this->source_ != nullptr && this->source_->Available() ? &this->source_->Items()[slot] : nullptr;
            if (this->slot_frames_) slotFrame.Draw(bounds);
            const ui::rect icon{bounds.x + 1 + this->inset_, bounds.y + 1 + this->inset_, this->icon_size_, this->icon_size_};
            auto* texture = item != nullptr && item->id != 0 && context.Icons() != nullptr ? context.Icons()->Item(item->id) : nullptr;
            if (texture != nullptr) design.DrawTexture(texture, icon, (static_cast<uint32_t>(std::round(this->icon_opacity_ * 255)) << 24) | 0xFFFFFF);
            else if ((item != nullptr && item->id != 0) || (this->empty_labels_ && !this->slot_labels_))
                context.DrawText(equipment_slot_names[slot], bounds, bounds, this->text_[1]);
            if (this->slot_labels_)
            {
                const ui::rect at{bounds.x, bounds.y + bounds.height, bounds.width, this->LabelHeight()};
                context.DrawText(equipment_slot_names[slot], at, at, this->text_[1]);
            }
            if (this->ammo_ && slot == static_cast<uint8_t>(Ashita::FFXI::Enums::EquipmentSlot::Ammo) && item != nullptr && item->id != 0)
            {
                char value[20]{}; std::snprintf(value, sizeof(value), "%u", item->count);
                const float height = std::max(12.0f, context.Design().TextHeight(this->text_[2].style) + 2);
                const ui::rect at{icon.x, icon.y + icon.height - height, icon.width, height};
                context.DrawText(value, at, at, this->text_[2]);
            }
        }
    }

    auto equipment_module::RenderTooltip(const module_render_context& context, const float x, const float y) const -> void
    {
        if (!this->tooltips_) return;
        static constexpr const char* bags[]{"Inventory", "Mog Safe", "Storage", "Temporary", "Locker", "Satchel", "Sack", "Case",
            "Wardrobe 1", "Mog Safe 2", "Wardrobe 2", "Wardrobe 3", "Wardrobe 4", "Wardrobe 5", "Wardrobe 6", "Wardrobe 7", "Wardrobe 8"};
        for (size_t cell = 0; cell < equipment_grid_order.size(); ++cell)
        {
            const auto at = this->state_.transform.Map(this->SlotBounds(cell));
            if (x < at.x || y < at.y || x >= at.x + at.width || y >= at.y + at.height) continue;
            const auto slot = equipment_grid_order[cell];
            const auto* item = this->source_ != nullptr && this->source_->Available() ? &this->source_->Items()[slot] : nullptr;
            const char* name = item == nullptr || !item->ready ? i18n::Text("Loading...") : item->id == 0 ? i18n::Text("Empty slot") : item->name.c_str();
            const std::string details = std::string{equipment_slot_names[slot]} + (item != nullptr && item->id != 0 ? std::string{" | "} + i18n::Text(bags[item->bag]) : "");
            const auto& viewport = context.Viewport();
            const float inset = 13;
            const float lineHeight = std::max(18.0f, context.Design().TextHeight(this->text_[3].style) + 4);
            const float captionHeight = std::max(14.0f, context.Design().TextHeight(this->text_[1].style) + 4);
            const float width = std::min(std::max(40.0f, viewport.width - 16), std::max(150.0f, std::min(360.0f,
                std::max(context.Design().MeasureText(name, this->text_[3].style), context.Design().MeasureText(details.c_str(), this->text_[1].style)) + inset * 2)));
            const float height = inset * 2 + lineHeight + captionHeight;
            const float left = std::clamp(at.x + at.width + 8, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
            const float top = std::clamp(at.y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
            const auto content = context.DrawTooltip({left, top, width, height});
            context.DrawText(name, {content.x, content.y, content.width, lineHeight}, content, this->text_[3]);
            auto caption = this->text_[1]; caption.style.alignment = ui::text_alignment::left;
            context.DrawText(details.c_str(), {content.x, content.y + lineHeight, content.width, captionHeight}, content, caption);
            return;
        }
    }
}
