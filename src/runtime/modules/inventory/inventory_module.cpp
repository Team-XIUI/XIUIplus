#include "inventory_module.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    inventory_module::inventory_module(const size_t group) : group_(std::min(group, inventory_groups.size() - 1))
    {
        this->state_.enabled = this->group_ == 0;
        this->warning_slots_ = inventory_groups[this->group_].warning_slots;
        this->critical_slots_ = inventory_groups[this->group_].critical_slots;
        if (this->group_ == 5) { this->columns_ = 10; this->rows_ = 8; }
    }

    auto inventory_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.caption_text}, module_text_profile{theme.meter_text}};
        this->text_[0].style.scale = 0.72f;
        this->text_[1].style.scale = 0.75f;
        this->text_[1].style.alignment = ui::text_alignment::right;
        this->normal_ = theme.colors.accent;
        this->empty_ = (theme.colors.muted & 0xFFFFFF) | 0x70000000;
    }

    auto inventory_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        const auto* moduleEnabled = this->module_enabled_;
        *this = inventory_module{this->group_};
        this->state_.bounds = bounds;
        this->source_ = source;
        this->module_enabled_ = moduleEnabled;
        this->ApplyTextDefaults(theme);
    }

    auto inventory_module::Load(const settings_store& store) -> void
    {
        const auto* section = this->Id();
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const float value = store.ReadFloat(section, key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->state_.enabled = store.ReadBool(section, "enabled", this->group_ == 0);
        this->state_.bounds = store.ReadRect((std::string{"layout."} + section).c_str(), this->state_.bounds);
        this->dots_ = store.ReadBool(section, "dots", true);
        this->count_ = store.ReadBool(section, "count", true);
        this->label_ = store.ReadBool(section, "label", true);
        this->split_ = store.ReadBool(section, "separate_containers", false);
        this->text_tint_ = store.ReadBool(section, "text_threshold_color", false);
        this->background_ = store.ReadBool(section, "background", false);
        this->count_mode_ = std::clamp(store.ReadInt(section, "count_mode", 0), 0, 1);
        this->threshold_mode_ = std::clamp(store.ReadInt(section, "threshold_mode", 0), 0, 1);
        this->text_position_ = std::clamp(store.ReadInt(section, "text_position", 0), 0, 1);
        this->selected_text_ = std::clamp(store.ReadInt(section, "selected_text", 0), 0, 1);
        this->warning_percent_ = number("warning_percent", 75, 0, 100);
        this->critical_percent_ = number("critical_percent", 95, this->warning_percent_, 100);
        this->warning_slots_ = number("warning_slots", inventory_groups[this->group_].warning_slots, 0, 640);
        this->critical_slots_ = number("critical_slots", inventory_groups[this->group_].critical_slots, this->warning_slots_, 640);
        this->columns_ = std::round(number("columns", this->group_ == 5 ? 10.0f : 5.0f, 1, 80));
        this->rows_ = std::round(number("rows", this->group_ == 5 ? 8.0f : 6.0f, 1, 80));
        this->dot_size_ = number("dot_size", 7, 3, 24);
        this->dot_gap_ = number("dot_gap", 2, 0, 12);
        this->group_gap_ = number("group_gap", 8, 0, 40);
        this->section_gap_ = number("section_gap", 10, 0, 40);
        this->container_.opacity = number("opacity", 1, 0, 1);
        this->container_.padding = number("padding", 8, 0, 24);
        this->normal_ = store.ReadArgb(section, "normal_color", this->normal_);
        this->empty_ = store.ReadArgb(section, "empty_color", this->empty_);
        this->warning_ = store.ReadArgb(section, "warning_color", this->warning_);
        this->critical_ = store.ReadArgb(section, "critical_color", this->critical_);
        this->text_[0] = LoadTextProfile(store, (std::string{"text."} + section + ".label").c_str(), this->text_[0]);
        this->text_[1] = LoadTextProfile(store, (std::string{"text."} + section + ".count").c_str(), this->text_[1]);
    }

    auto inventory_module::Save(const settings_store& store) const -> void
    {
        const auto* section = this->Id();
        store.WriteBool(section, "enabled", this->state_.enabled);
        store.WriteRect((std::string{"layout."} + section).c_str(), this->state_.bounds);
        store.WriteBool(section, "dots", this->dots_);
        store.WriteBool(section, "count", this->count_);
        store.WriteBool(section, "label", this->label_);
        store.WriteBool(section, "separate_containers", this->split_);
        store.WriteBool(section, "text_threshold_color", this->text_tint_);
        store.WriteBool(section, "background", this->background_);
        store.WriteInt(section, "count_mode", this->count_mode_);
        store.WriteInt(section, "threshold_mode", this->threshold_mode_);
        store.WriteInt(section, "text_position", this->text_position_);
        store.WriteInt(section, "selected_text", this->selected_text_);
        store.WriteFloat(section, "warning_percent", this->warning_percent_);
        store.WriteFloat(section, "critical_percent", this->critical_percent_);
        store.WriteFloat(section, "warning_slots", this->warning_slots_);
        store.WriteFloat(section, "critical_slots", this->critical_slots_);
        store.WriteFloat(section, "columns", this->columns_);
        store.WriteFloat(section, "rows", this->rows_);
        store.WriteFloat(section, "dot_size", this->dot_size_);
        store.WriteFloat(section, "dot_gap", this->dot_gap_);
        store.WriteFloat(section, "group_gap", this->group_gap_);
        store.WriteFloat(section, "section_gap", this->section_gap_);
        store.WriteFloat(section, "opacity", this->container_.opacity);
        store.WriteFloat(section, "padding", this->container_.padding);
        store.WriteArgb(section, "normal_color", this->normal_);
        store.WriteArgb(section, "empty_color", this->empty_);
        store.WriteArgb(section, "warning_color", this->warning_);
        store.WriteArgb(section, "critical_color", this->critical_);
        SaveTextProfile(store, (std::string{"text."} + section + ".label").c_str(), this->text_[0]);
        SaveTextProfile(store, (std::string{"text."} + section + ".count").c_str(), this->text_[1]);
    }

    auto inventory_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Choose what this storage tracker displays."},
            {"Layout", "Arrange slot dots into groups; extra groups wrap to fit the screen."},
            {"Fullness", "Used-slot warning and critical thresholds, with independent colors."},
            {"Appearance", "Optional container using the game's current Window Type."},
            {"Text", "Label and count profiles retain the shared HUD text effects."}};
        return sections;
    }

    auto inventory_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount() || index == hide_menu) return false;
        if (index <= count_mode) return section == 0;
        if (index <= critical || (index >= normal_color && index <= critical_color)) return section == 2;
        if (index <= text_position) return section == 1;
        if (index <= padding) return section == 3;
        return section == 4;
    }

    auto inventory_module::Option(const size_t index) -> module_option
    {
        static constexpr const char* counts[]{"Used / capacity", "Free slots"};
        static constexpr const char* modes[]{"Percent full", "Used slots"};
        static constexpr const char* positions[]{"Above dots", "Below dots"};
        static constexpr const char* texts[]{"Label", "Count"};
        const float slotLimit = this->Usage().capacity > 0 ? static_cast<float>(this->Usage().capacity) : static_cast<float>(inventory_groups[this->group_].count * 80);
        const bool fullness = this->dots_ || (this->count_ && this->text_tint_);
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Independently movable storage tracker; locked bags are hidden.", &this->state_.enabled).Ordered(-50);
        case dots: return module_option::Toggle("Slot dots", "One dot per inventory slot, including empty slots.", &this->dots_);
        case count: return module_option::Toggle("Count", "Show occupied slots and capacity, or remaining free slots.", &this->count_);
        case label: return module_option::Toggle("Label", "Show the storage name alongside the count.", &this->label_);
        case split: return module_option::Toggle("Separate containers", "Give each unlocked Safe or Wardrobe its own grid within this tracker.", &this->split_).VisibleWhen(inventory_groups[this->group_].count > 1);
        case hide_menu: return {};
        case text_tint: return module_option::Toggle("Fullness text color", "Tint the count with its fullness color, retaining its text effects.", &this->text_tint_).VisibleWhen(this->count_);
        case count_mode: return module_option::Choice("Count display", "Fullness always uses occupied slots, even when showing free slots.", &this->count_mode_, counts, 2).VisibleWhen(this->count_);
        case threshold_mode: return module_option::Choice("Threshold units", "Percent adapts to bag capacity; Used slots uses fixed counts.", &this->threshold_mode_, modes, 2, module_option_group::colors).VisibleWhen(fullness);
        case warning: return module_option::Slider("Warning threshold", "Used amount at which occupied dots take the warning color.", this->threshold_mode_ == 0 ? &this->warning_percent_ : &this->warning_slots_, 0, this->threshold_mode_ == 0 ? 100.0f : slotLimit, 1, module_option_group::colors).VisibleWhen(fullness);
        case critical: return module_option::Slider("Critical threshold", "Used amount at which occupied dots take the critical color; full bags are always critical.", this->threshold_mode_ == 0 ? &this->critical_percent_ : &this->critical_slots_, 0, this->threshold_mode_ == 0 ? 100.0f : slotLimit, 1, module_option_group::colors).VisibleWhen(fullness);
        case columns: return module_option::Slider("Columns", "Dots across each group; narrows automatically to stay inside the viewport.", &this->columns_, 1, 80, 1).Ordered(120, true).VisibleWhen(this->dots_);
        case rows: return module_option::Slider("Rows per group", "Start another group after this many rows; every slot remains visible.", &this->rows_, 1, 80, 1).Ordered(121).VisibleWhen(this->dots_);
        case dot_size: return module_option::Slider("Dot size", "Actual dot diameter in pixels.", &this->dot_size_, 3, 24, 1).Ordered(110).VisibleWhen(this->dots_);
        case dot_gap: return module_option::Slider("Dot spacing", "Pixels between neighboring dots.", &this->dot_gap_, 0, 12, 1).Ordered(300, true).VisibleWhen(this->dots_);
        case group_gap: return module_option::Slider("Group spacing", "Pixels between groups of rows and columns.", &this->group_gap_, 0, 40, 1).Ordered(302).VisibleWhen(this->dots_);
        case section_gap: return module_option::Slider("Container spacing", "Vertical pixels between separately displayed Safes or Wardrobes.", &this->section_gap_, 0, 40, 1).Ordered(303).VisibleWhen(this->split_ && inventory_groups[this->group_].count > 1);
        case text_position: return module_option::Choice("Text position", "Place the label and count above or below the dot grid.", &this->text_position_, positions, 2).Ordered(200).VisibleWhen(this->dots_ && (this->label_ || this->count_));
        case background: return module_option::Toggle("Container", "Draw a shared native DAT container behind each storage grid.", &this->background_, module_option_group::appearance).Ordered(-10);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the optional container, including its border.", &this->container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450).VisibleWhen(this->background_);
        case padding: return module_option::Slider("Container padding", "Space between the native frame and the dot grid.", &this->container_.padding, 0, 24, 1, module_option_group::appearance).Ordered(402).VisibleWhen(this->background_);
        case normal_color: return module_option::Color("Occupied", "Normal occupied-slot tint.", &this->normal_).VisibleWhen(fullness);
        case empty_color: return module_option::Color("Empty", "Unoccupied slots retain this muted tint at every fullness level.", &this->empty_).VisibleWhen(this->dots_);
        case warning_color: return module_option::Color("Warning", "Occupied-slot tint at the warning threshold.", &this->warning_).VisibleWhen(fullness);
        case critical_color: return module_option::Color("Critical / full", "Occupied-slot tint at the critical threshold or full capacity.", &this->critical_).VisibleWhen(fullness);
        case selected_text: return module_option::Element("Selected text", "Configure the label and count independently.", &this->selected_text_, texts, 2, module_option_group::text);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]).VisibleWhen(this->selected_text_ == 0 ? this->label_ || (!this->dots_ && !this->count_) : this->count_);
        }
    }

    auto inventory_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == warning)
        {
            this->critical_percent_ = std::max(this->critical_percent_, this->warning_percent_);
            this->critical_slots_ = std::max(this->critical_slots_, this->warning_slots_);
        }
        if (index == critical)
        {
            this->warning_percent_ = std::min(this->warning_percent_, this->critical_percent_);
            this->warning_slots_ = std::min(this->warning_slots_, this->critical_slots_);
        }
    }

    auto inventory_module::Usage(void) const -> inventory_usage
    {
        return this->source_ != nullptr ? InventoryTotal(this->source_->State(), inventory_groups[this->group_]) : inventory_usage{};
    }

    auto inventory_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->PreviewVisible() && this->state_.enabled && (this->dots_ || this->count_ || this->label_) && this->Usage().ready;
    }

    auto inventory_module::Fullness(const inventory_usage& usage, const int mode, const float warning, const float critical) -> int
    {
        if (usage.capacity == 0 || !usage.ready || usage.used == 0) return 0;
        if (usage.used >= usage.capacity) return 2;
        const float value = mode == 0 ? 100.0f * usage.used / usage.capacity : static_cast<float>(usage.used);
        return value >= critical ? 2 : value >= warning ? 1 : 0;
    }

    auto inventory_module::DotPosition(const uint32_t slot, const uint32_t columns, const uint32_t rows, const uint32_t groupsAcross,
        const float size, const float gap, const float groupGap) -> ui::point
    {
        const auto groupSize = columns * rows, group = slot / groupSize, inGroup = slot % groupSize;
        return {static_cast<float>(group % groupsAcross) * (columns * (size + gap) - gap + groupGap) + (inGroup % columns) * (size + gap),
            static_cast<float>(group / groupsAcross) * (rows * (size + gap) - gap + groupGap) + (inGroup / columns) * (size + gap)};
    }

    auto inventory_module::Describe(void) const -> std::string
    {
        const auto usage = this->Usage();
        return std::string{this->DisplayName()} + ": " + std::to_string(usage.used) + "/" + std::to_string(usage.capacity) +
            (usage.ready ? " ready" : " unavailable/loading") + (this->state_.enabled ? " (enabled)" : " (disabled)");
    }

    auto inventory_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        struct panel { inventory_usage usage; const char* name; };
        std::array<panel, 8> panels{};
        size_t panelCount{};
        const auto& group = inventory_groups[this->group_];
        const bool live = this->Usage().ready;
        static constexpr const char* wardrobeNames[]{"Wardrobe 1", "Wardrobe 2", "Wardrobe 3", "Wardrobe 4", "Wardrobe 5", "Wardrobe 6", "Wardrobe 7", "Wardrobe 8"};
        for (size_t i = 0; i < (this->split_ ? group.count : 1); ++i)
        {
            const auto usage = live ? (this->split_ ? this->source_->State().bags[group.bags[i]] : this->Usage()) :
                inventory_usage{static_cast<uint32_t>((45 + i * 5) * (this->split_ ? 1 : group.count)), static_cast<uint32_t>(80 * (this->split_ ? 1 : group.count)), true};
            if (usage.capacity == 0) continue;
            const char* name = !this->split_ || group.count == 1 ? this->DisplayName() : this->group_ == 3 ? (i == 0 ? "Mog Safe" : "Mog Safe 2") : wardrobeNames[i];
            panels[panelCount++] = {usage, name};
        }
        const auto& design = context.Design();
        const float inset = this->background_ ? this->container_.edge_size + this->container_.padding : 0;
        const float available = std::max(24.0f, context.Viewport().width - 24 - inset * 2);
        const auto columnCount = std::min(static_cast<uint32_t>(std::round(this->columns_)),
            std::max(1u, static_cast<uint32_t>((available + this->dot_gap_) / (this->dot_size_ + this->dot_gap_))));
        const auto rowCount = static_cast<uint32_t>(std::round(this->rows_));
        std::array<ui::slot_dot, 640> dots{};
        float width{}, y = this->state_.bounds.y;
        for (size_t p = 0; p < panelCount; ++p)
        {
            const auto& panel = panels[p];
            const auto groups = (panel.usage.capacity + columnCount * rowCount - 1) / (columnCount * rowCount);
            const float groupWidth = columnCount * (this->dot_size_ + this->dot_gap_) - this->dot_gap_;
            const auto across = std::max(1u, std::min(groups, static_cast<uint32_t>((available + this->group_gap_) / (groupWidth + this->group_gap_))));
            float gridWidth{}, gridHeight{};
            if (this->dots_)
                for (uint32_t slot = 0; slot < panel.usage.capacity; ++slot)
                {
                    const auto at = DotPosition(slot, columnCount, rowCount, across, this->dot_size_, this->dot_gap_, this->group_gap_);
                    dots[slot].bounds = {at.x, at.y, this->dot_size_, this->dot_size_};
                    gridWidth = std::max(gridWidth, at.x + this->dot_size_);
                    gridHeight = std::max(gridHeight, at.y + this->dot_size_);
                }
            char count[32]{};
            if (this->count_mode_ == 0) std::snprintf(count, sizeof(count), "%u/%u", panel.usage.used, panel.usage.capacity);
            else std::snprintf(count, sizeof(count), "%u free", panel.usage.capacity - panel.usage.used);
            const float labelWidth = this->label_ ? design.MeasureText(panel.name, this->text_[0].style) : 0;
            const float countWidth = this->count_ ? design.MeasureText(count, this->text_[1].style) : 0;
            const float textWidth = labelWidth + countWidth + (this->label_ && this->count_ ? 12 : 0);
            const float textHeight = this->label_ || this->count_ ? std::max(this->label_ ? design.TextHeight(this->text_[0].style) : 0,
                this->count_ ? design.TextHeight(this->text_[1].style) : 0) + 4 : 0;
            const float gap = textHeight > 0 && gridHeight > 0 ? 4.0f : 0;
            const float contentWidth = std::max({24.0f, gridWidth, textWidth});
            const float contentHeight = std::max(16.0f, gridHeight + textHeight + gap);
            const ui::rect bounds{this->state_.bounds.x, y, contentWidth + inset * 2, contentHeight + inset * 2};
            if (this->background_) context.DrawContainer(bounds, this->container_);
            const float left = bounds.x + inset, top = bounds.y + inset;
            const int fullness = Fullness(panel.usage, this->threshold_mode_, this->threshold_mode_ == 0 ? this->warning_percent_ : this->warning_slots_,
                this->threshold_mode_ == 0 ? this->critical_percent_ : this->critical_slots_);
            const auto usedColor = fullness == 2 ? this->critical_ : fullness == 1 ? this->warning_ : this->normal_;
            if (this->dots_)
            {
                const float dotsTop = top + (this->text_position_ == 0 ? textHeight + gap : 0);
                for (uint32_t slot = 0; slot < panel.usage.capacity; ++slot)
                {
                    dots[slot].bounds.x += left;
                    dots[slot].bounds.y += dotsTop;
                    dots[slot].color = slot < panel.usage.used ? usedColor : this->empty_;
                }
                design.DrawSlotDots({dots.data(), panel.usage.capacity});
            }
            const float textTop = top + (this->text_position_ == 1 ? gridHeight + gap : 0);
            if (this->label_)
            {
                const ui::rect at{left, textTop, this->count_ ? std::max(0.0f, contentWidth - countWidth - 8) : contentWidth, textHeight};
                context.DrawText(panel.name, at, at, this->text_[0]);
            }
            if (this->count_)
            {
                const ui::rect at{left + (this->label_ ? labelWidth + 8 : 0), textTop, contentWidth - (this->label_ ? labelWidth + 8 : 0), textHeight};
                context.DrawText(count, at, at, this->text_tint_ ? TintTextProfile(this->text_[1], usedColor) : this->text_[1]);
            }
            if (!this->dots_ && !this->label_ && !this->count_ && context.IsEditing())
                context.DrawText(this->DisplayName(), bounds, bounds, this->text_[0]);
            width = std::max(width, bounds.width);
            y += bounds.height + this->section_gap_;
        }
        this->state_.bounds.width = width;
        this->state_.bounds.height = std::max(16.0f, y - this->state_.bounds.y - this->section_gap_);
    }
}
