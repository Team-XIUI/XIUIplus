#include "ui/text_fit.hpp"
#include "enemy_list_module.hpp"
#include "modules/bar_config.hpp"
#include "ui/status_strip.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>

namespace
{
    constexpr std::array text_sections{"text.enemylist.name", "text.enemylist.details", "text.enemylist.target", "text.enemylist.cast"};

    auto LineHeight(const xiui::module_text_profile& profile) -> float { return std::max(14.0f, 18.0f * profile.style.scale); }

    auto Contains(const xiui::ui::rect& bounds, const float x, const float y) -> bool
    { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }

    auto Union(const xiui::ui::rect& a, const xiui::ui::rect& b) -> xiui::ui::rect
    {
        if (b.width <= 0 || b.height <= 0) return a;
        const float x = std::min(a.x, b.x), y = std::min(a.y, b.y);
        return {x, y, std::max(a.x + a.width, b.x + b.width) - x, std::max(a.y + a.height, b.y + b.height) - y};
    }

    auto Offset(xiui::ui::rect& bounds, const float x, const float y) -> void
    { bounds.x += x; bounds.y += y; }

    auto Constrain(xiui::ui::rect& bounds, const float start, const float extent, const bool horizontal) -> float
    {
        if (bounds.width <= 0 || bounds.height <= 0) return 0;
        auto& position = horizontal ? bounds.x : bounds.y;
        const auto size = horizontal ? bounds.width : bounds.height;
        if (size > extent) return 0;
        const auto previous = position;
        position = std::clamp(position, start, start + extent - size);
        return position - previous;
    }

    auto IsLegacyPlainText(const xiui::module_text_profile& profile) -> bool
    {
        const auto& style = profile.style;
        return style.top_color == style.bottom_color && !style.shadow.enabled && !style.stroke.enabled &&
            !style.glow.enabled && !style.sheen.enabled;
    }
}

namespace xiui
{
    auto enemy_list_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->timers_.text = {theme.caption_text};
        this->text_[0] = {theme.title_text};
        this->text_[0].style.scale = 0.9f;
        this->text_[1] = {theme.meter_text};
        this->text_[2] = {theme.caption_text};
        this->text_[2].style.alignment = ui::text_alignment::right;
        this->text_[3] = {theme.meter_text};
        if (this->meter_.height <= 0.0f) this->meter_.height = 10.0f;
    }

    auto enemy_list_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* combat = this->combat_;
        const auto* timing = this->timing_;
        *this = enemy_list_module{};
        this->combat_ = combat; this->timing_ = timing;
        this->state_.bounds = bounds;
        this->ApplyTextDefaults(theme);
    }

    auto enemy_list_module::Load(const settings_store& store) -> void
    {
        this->EndControllerBrowse();
        this->timers_.Load(store, "timing.enemylist");
        this->statuses_.Load(store, "status.enemylist");
        this->state_.enabled = store.ReadBool("enemylist", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.enemylist", this->state_.bounds);
        this->distance_ = store.ReadBool("enemylist", "distance", true);
        this->percent_ = store.ReadBool("enemylist", "hp_percent", true);
        this->targets_ = store.ReadBool("enemylist", "enemy_targets", true);
        this->casts_ = store.ReadBool("enemylist", "casts", true);
        this->include_selected_ = store.ReadBool("enemylist", "include_selected", false);
        this->max_rows_ = std::clamp(store.ReadFloat("enemylist", "rows_per_column", store.ReadFloat("enemylist", "max_rows", 8)), 1.0f, 20.0f);
        this->gap_ = std::clamp(store.ReadFloat("enemylist", "spacing", 6), 0.0f, 24.0f);
        this->max_columns_ = std::clamp(store.ReadFloat("enemylist", "max_columns", 1), 1.0f, 5.0f);
        this->column_gap_ = std::clamp(store.ReadFloat("enemylist", "column_spacing", 12), 0.0f, 64.0f);
        this->target_panel_ = store.ReadBool("enemylist", "target_panel", false);
        this->target_offset_x_ = std::clamp(store.ReadFloat("enemylist", "target_offset_x", 10), -500.0f, 500.0f);
        this->target_offset_y_ = std::clamp(store.ReadFloat("enemylist", "target_offset_y", 0), -500.0f, 500.0f);
        this->target_width_ = std::clamp(store.ReadFloat("enemylist", "target_width", 100), 50.0f, 240.0f);
        this->target_container_ = {.opacity = std::clamp(store.ReadFloat("enemylist", "target_opacity", 1), 0.0f, 1.0f),
            .padding = std::clamp(store.ReadFloat("enemylist", "target_padding", 4), 0.0f, 24.0f)};
        this->borders_ = store.ReadBool("enemylist", "borders", true);
        this->claim_border_ = store.ReadBool("enemylist", "claim_border", false);
        this->click_target_ = store.ReadBool("enemylist", "click_target", true);
        this->target_borders_ = store.ReadBool("enemylist", "target_borders", true);
        this->selected_text_ = std::clamp(store.ReadInt("enemylist", "selected_text", 0), 0, 4);
        this->container_ = {.opacity = std::clamp(store.ReadFloat("enemylist", "opacity", 1), 0.0f, 1.0f),
            .padding = std::clamp(store.ReadFloat("enemylist", "padding", 6), 4.0f, 24.0f)};
        this->meter_ = LoadMeterStyle(store, "meter.enemylist.hp", this->meter_);
        if (this->meter_.height <= 0) this->meter_.height = 10;
        this->hp_top_ = store.ReadArgb("enemylist", "hp_top", this->hp_top_);
        this->hp_bottom_ = store.ReadArgb("enemylist", "hp_bottom", this->hp_bottom_);
        this->aggro_color_ = store.ReadArgb("enemylist", "aggro_color", this->aggro_color_);
        this->cast_color_ = store.ReadArgb("enemylist", "cast_color", this->cast_color_);
        constexpr std::array inheritedText{"text.target.name", "text.player.values", "text.target.distance", "text.player.values"};
        const bool upgradeText = store.ReadInt("enemylist", "text_style_version", 0) < 1;
        for (size_t i = 0; i < this->text_.size(); ++i)
        {
            auto profile = LoadTextProfile(store, text_sections[i], this->text_[i]);
            if (upgradeText && IsLegacyPlainText(profile))
            {
                const auto scale = profile.style.scale;
                const auto alignment = profile.style.alignment;
                profile.style = LoadTextProfile(store, inheritedText[i], this->text_[i]).style;
                profile.style.scale = scale;
                profile.style.alignment = alignment;
            }
            this->text_[i] = profile;
        }
        if (store.ReadInt("enemylist", "layout_version", 0) < 1)
        {
            if (this->state_.bounds.width == 280) this->state_.bounds.width = 220;
            if (this->container_.padding == 8) this->container_.padding = 6;
            if (this->gap_ == 8) this->gap_ = 6;
        }
        this->animations_.clear();
        this->state_.content_bounds = {};
    }

    auto enemy_list_module::Save(const settings_store& store) const -> void
    {
        this->timers_.Save(store, "timing.enemylist");
        this->statuses_.Save(store, "status.enemylist");
        store.WriteBool("enemylist", "enabled", this->state_.enabled);
        store.WriteRect("layout.enemylist", this->state_.bounds);
        store.WriteBool("enemylist", "distance", this->distance_);
        store.WriteBool("enemylist", "hp_percent", this->percent_);
        store.WriteBool("enemylist", "enemy_targets", this->targets_);
        store.WriteBool("enemylist", "casts", this->casts_);
        store.WriteInt("enemylist", "layout_version", 1);
        store.WriteInt("enemylist", "text_style_version", 1);
        store.WriteBool("enemylist", "include_selected", this->include_selected_);
        store.WriteFloat("enemylist", "max_rows", this->max_rows_);
        store.WriteFloat("enemylist", "rows_per_column", this->max_rows_);
        store.WriteFloat("enemylist", "spacing", this->gap_);
        store.WriteFloat("enemylist", "max_columns", this->max_columns_);
        store.WriteFloat("enemylist", "column_spacing", this->column_gap_);
        store.WriteBool("enemylist", "target_panel", this->target_panel_);
        store.WriteFloat("enemylist", "target_offset_x", this->target_offset_x_);
        store.WriteFloat("enemylist", "target_offset_y", this->target_offset_y_);
        store.WriteFloat("enemylist", "target_width", this->target_width_);
        store.WriteFloat("enemylist", "target_opacity", this->target_container_.opacity);
        store.WriteFloat("enemylist", "target_padding", this->target_container_.padding);
        store.WriteBool("enemylist", "borders", this->borders_);
        store.WriteBool("enemylist", "claim_border", this->claim_border_);
        store.WriteBool("enemylist", "click_target", this->click_target_);
        store.WriteBool("enemylist", "target_borders", this->target_borders_);
        store.WriteInt("enemylist", "selected_text", this->selected_text_);
        store.WriteFloat("enemylist", "opacity", this->container_.opacity);
        store.WriteFloat("enemylist", "padding", this->container_.padding);
        SaveMeterStyle(store, "meter.enemylist.hp", this->meter_);
        store.WriteArgb("enemylist", "hp_top", this->hp_top_);
        store.WriteArgb("enemylist", "hp_bottom", this->hp_bottom_);
        store.WriteArgb("enemylist", "aggro_color", this->aggro_color_);
        store.WriteArgb("enemylist", "cast_color", this->cast_color_);
        for (size_t i = 0; i < this->text_.size(); ++i)
            SaveTextProfile(store, text_sections[i], this->text_[i]);
    }

    auto enemy_list_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static const auto sections = [] {
            std::array<module_config_section, 11> result{};
            std::copy(ui::TimingSections().begin(), ui::TimingSections().end(), result.begin());
            result[8] = {"Statuses", "Native status icons, timers, placement and details.", "statuses"};
            result[9] = {"Target Panel", "Separate target panel visibility, width, offsets and container.", "target-panel"};
            result[10] = {"Appearance", "Enemy card container, spacing inside the frame and borders.", "appearance"};
            return result;
        }();
        return sections;
    }

    auto enemy_list_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= status_options_start) return section == 8;
        if (index >= layout_options_start)
        {
            const auto option = index - layout_options_start;
            if (option == 10) return section == static_cast<size_t>(bar_config_section::general);
            if ((option >= 2 && option <= 7) || option == 11) return section == 9;
            if (option == 8 || option == 9) return section == 10;
            return section == static_cast<size_t>(bar_config_section::layout);
        }
        if (index >= timing_options_start) return section == 7;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= 16 && index < 16 + meter_style_option_count) return topic == MeterConfigSection(index - 16);
        if (index >= 16 + meter_style_option_count || index == 6) return topic == bar_config_section::text;
        if (index >= 12) return topic == bar_config_section::colors;
        if (index == 10 || index == 11) return section == 10;
        if (index >= 7) return topic == bar_config_section::layout;
        return topic == bar_config_section::general;
    }

    auto enemy_list_module::ContentOption(const size_t index) -> module_option
    {
        if (index >= status_options_start) return this->statuses_.Option(index - status_options_start);
        if (index >= layout_options_start)
        {
            switch (index - layout_options_start)
            {
            case 0: return module_option::Slider("Maximum columns", "Rows fill each column in tracking order, up to 32 enemies.", &this->max_columns_, 1, 5, 1, module_option_group::appearance).Ordered(120, true);
            case 1: return module_option::Slider("Column spacing", "Gap between complete columns, including target panels.", &this->column_gap_, 0, 64, 1, module_option_group::appearance).Ordered(300, true).VisibleWhen(this->max_columns_ > 1);
            case 2: return module_option::Toggle("Separate target panel", "Put enemy targets in their own panel beside each card.", &this->target_panel_, module_option_group::appearance).Ordered(-50).VisibleWhen(this->targets_);
            case 3: return module_option::Slider("Target X offset", "Horizontal offset from the right edge of each card.", &this->target_offset_x_, -500, 500, 1, module_option_group::appearance).Ordered(210, true).VisibleWhen(this->targets_ && this->target_panel_);
            case 4: return module_option::Slider("Target Y offset", "Vertical offset from the top of each card.", &this->target_offset_y_, -500, 500, 1, module_option_group::appearance).Ordered(211).VisibleWhen(this->targets_ && this->target_panel_);
            case 5: return module_option::Slider("Target width", "Width of each separate target panel.", &this->target_width_, 50, 240, 1, module_option_group::appearance).Ordered(100).VisibleWhen(this->targets_ && this->target_panel_);
            case 6: return module_option::Slider("Target opacity", "Fade the target panel's native window skin.", &this->target_container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450).VisibleWhen(this->targets_ && this->target_panel_);
            case 7: return module_option::Slider("Target padding", "Space inside each target panel.", &this->target_container_.padding, 0, 24, 1, module_option_group::appearance).Ordered(402).VisibleWhen(this->targets_ && this->target_panel_);
            case 8: return module_option::Toggle("Entry borders", "Show the native frame around each enemy card.", &this->borders_, module_option_group::appearance).Ordered(470);
            case 9: return module_option::Toggle("Claim-colored borders", "Tint the entry outline with the enemy's game-state name color.", &this->claim_border_, module_option_group::appearance).Ordered(470).VisibleWhen(this->borders_);
            case 10: return module_option::Toggle("Click to target", "Target an enemy by clicking its card or target panel.", &this->click_target_);
            case 11: return module_option::Toggle("Target borders", "Show the native frame around separate target panels.", &this->target_borders_, module_option_group::appearance).VisibleWhen(this->targets_ && this->target_panel_);
            default: return {};
            }
        }
        if (index >= timing_options_start) return this->timers_.Option(index - timing_options_start);
        static constexpr const char* textNames[]{"Enemy name", "Details", "Enemy target", "Cast", "Attack timing"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show enemies fighting your group.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Distance", "Show distance in yalms.", &this->distance_);
        case 2: return module_option::Toggle("HP percent", "Show enemy health percent.", &this->percent_);
        case 3: return module_option::Toggle("Enemy targets", "Show the enemy's last action target.", &this->targets_);
        case 4: return module_option::Toggle("Casts", "Show enemy spells and abilities.", &this->casts_);
        case 5: return module_option::Toggle("Include selected", "Also list an unclaimed selected enemy.", &this->include_selected_);
        case 6: return module_option::Element("Selected text", "Text element configured in Text.", &this->selected_text_, textNames, 5);
        case 7: return ModuleWidthOption(this->state_);
        case 8: return module_option::Slider("Rows per column", "Maximum enemy cards in each column.", &this->max_rows_, 1, 20, 1, module_option_group::appearance).Ordered(121);
        case 9: return module_option::Slider("Row spacing", "Gap between complete enemy rows.", &this->gap_, 0, 24, 1, module_option_group::appearance).Ordered(301);
        case 10: return module_option::Slider("Opacity", "Fade the native window skin.", &this->container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450);
        case 11: return module_option::Slider("Padding", "Space inside each enemy container.", &this->container_.padding, 4, 24, 1, module_option_group::appearance).Ordered(402);
        case 12: return module_option::Color("HP fill top", "Upper health gradient.", &this->hp_top_, module_option_group::colors).Ordered(500, true);
        case 13: return module_option::Color("HP fill bottom", "Lower health gradient.", &this->hp_bottom_, module_option_group::colors).Ordered(501);
        case 14: return module_option::Color("Attacking you", "Target text when an enemy attacks you.", &this->aggro_color_, module_option_group::colors).VisibleWhen(this->targets_);
        case 15: return module_option::Color("Cast", "Enemy cast progress and name.", &this->cast_color_, module_option_group::colors).VisibleWhen(this->casts_);
        default:
            if (index < 16 + meter_style_option_count) return MeterStyleOption(index - 16, this->meter_);
            const auto textIndex = index - 16 - meter_style_option_count;
            auto option = TextProfileOption(textIndex, this->selected_text_ == 4 ? this->timers_.text : this->text_[this->selected_text_]);
            if (this->selected_text_ == 0 && (textIndex == 5 || textIndex == 6))
            {
                option = option.VisibleWhen(false);
                option.description = "Name color follows the game state.";
            }
            return option.VisibleWhen(this->selected_text_ == 1 ? this->distance_ || this->percent_ || this->statuses_.enabled :
                this->selected_text_ == 2 ? this->targets_ : this->selected_text_ == 3 ? this->casts_ :
                this->selected_text_ == 4 ? this->timers_.enabled : true);
        }
    }

    auto enemy_list_module::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        if (this->state_.enabled) this->source_.Update(core, now, this->include_selected_);
        else this->source_.Clear();
        if (this->source_.Entries().empty()) this->animations_.clear();
        if (this->controller_browsing_) this->ResolveControllerMember();
        if (this->controller_status_browsing_ && this->ControllerStatusBounds().width == 0) this->EndControllerStatus();
    }

    auto enemy_list_module::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> void
    {
        if (this->state_.enabled) this->source_.HandlePacket(core, id, data, now, decoded);
    }

    auto enemy_list_module::HeaderHeight(const enemy_entry& entry) const -> float
    {
        return this->targets_ && !this->target_panel_ && !entry.target_name.empty()
            ? std::max(LineHeight(this->text_[0]), LineHeight(this->text_[2])) : LineHeight(this->text_[0]);
    }

    auto enemy_list_module::RowHeight(const enemy_entry& entry, const uint64_t now, const float width) const -> float
    {
        float height = 2 * (this->container_.edge_size + this->container_.padding) + this->HeaderHeight(entry) + 2 + this->meter_.height + 2;
        if (this->distance_ || this->percent_) height += std::max(16.0f, 18.0f * this->text_[1].style.scale) * 0.5f;
        if (this->casts_ && this->Cast(entry).Active(now)) height += LineHeight(this->text_[3]) + 10;
        if (this->timers_.enabled && (this->timing_preview_ || (this->timing_ != nullptr && this->timing_->Enemy(entry.id, now / 1000.0).active)))
            height += 2 + ui::TimingHeight(this->timers_.text, this->timers_.height, this->timers_.text_only);
        height += ui::StatusStripHeight(this->Statuses(entry.id).size(), (width > 0 ? width : this->state_.bounds.width) - 2 * (this->container_.edge_size + this->container_.padding), this->statuses_);
        return std::ceil(height);
    }

    auto enemy_list_module::StatusBounds(const enemy_entry& entry, const ui::rect& bounds) const -> ui::rect
    {
        const auto content = ui::container_renderer::ContentBounds(bounds, this->container_);
        const float height = ui::StatusStripHeight(this->Statuses(entry.id).size(), content.width, this->statuses_);
        return {content.x, bounds.y + bounds.height - this->container_.edge_size - this->container_.padding - height + 2, content.width, 0};
    }

    auto enemy_list_module::LayoutEntries(const std::span<const enemy_entry> entries, const uint64_t now) const -> entries_layout
    {
        entries_layout layout;
        const auto rows = static_cast<size_t>(std::clamp(this->max_rows_, 1.0f, 20.0f));
        const auto columns = static_cast<size_t>(std::clamp(this->max_columns_, 1.0f, 5.0f));
        const auto count = std::min({entries.size(), rows * columns, enemy_source::capacity});
        float cardWidth = this->state_.bounds.width;
        if (this->viewport_.width > 0) cardWidth = std::min(cardWidth, this->viewport_.width);
        float left{}, right = cardWidth;
        for (size_t index = 0; index < count; ++index)
        {
            auto& row = layout.entries[index];
            row.index = index;
            row.bounds = {0, 0, cardWidth, this->RowHeight(entries[index], now, cardWidth)};
            row.statuses = this->StatusBounds(entries[index], row.bounds);
            auto strip = ui::LayoutStatusStrip(this->Statuses(entries[index].id).size(), row.statuses, this->statuses_);
            if (this->viewport_.width > 0 && strip.count > 0 && strip.bounds.width <= this->viewport_.width)
            {
                const auto x = std::clamp(strip.bounds.x, cardWidth - this->viewport_.width, this->viewport_.width - strip.bounds.width);
                row.statuses.x += x - strip.bounds.x;
                strip.bounds.x = x;
            }
            row.footprint = Union(row.bounds, strip.bounds);
            if (this->targets_ && this->target_panel_ && !entries[index].target_name.empty())
            {
                row.target = {cardWidth + this->target_offset_x_, this->target_offset_y_, this->target_width_,
                    LineHeight(this->text_[2]) + 2 * (this->target_container_.edge_size + this->target_container_.padding)};
                if (this->viewport_.width > 0)
                {
                    row.target.width = std::min(row.target.width, this->viewport_.width);
                    row.target.x = std::clamp(row.target.x, cardWidth - this->viewport_.width, this->viewport_.width - row.target.width);
                }
                row.footprint = Union(row.footprint, row.target);
            }
            left = std::min(left, row.footprint.x);
            right = std::max(right, row.footprint.x + row.footprint.width);
        }
        const bool wide = this->viewport_.width > 0 && right - left > this->viewport_.width;
        if (wide)
        {
            left = std::clamp(left, cardWidth - this->viewport_.width, 0.0f);
            right = left + this->viewport_.width;
        }
        for (size_t index = 0; index < count; ++index)
        {
            auto& row = layout.entries[index];
            auto strip = ui::LayoutStatusStrip(this->Statuses(entries[index].id).size(), row.statuses, this->statuses_);
            if (wide)
            {
                row.statuses.x += Constrain(strip.bounds, left, this->viewport_.width, true);
                Constrain(row.target, left, this->viewport_.width, true);
            }
            if (this->viewport_.height > 0 && row.bounds.height <= this->viewport_.height && row.footprint.height > this->viewport_.height)
            {
                const auto top = std::clamp(row.footprint.y, row.bounds.height - this->viewport_.height, 0.0f);
                row.statuses.y += Constrain(strip.bounds, top, this->viewport_.height, false);
                Constrain(row.target, top, this->viewport_.height, false);
            }
            row.footprint = Union(Union(row.bounds, strip.bounds), row.target);
        }
        const float columnWidth = right - left;
        size_t column{}, rowCount{};
        float y{}, totalHeight{};
        for (size_t index = 0; index < count; ++index)
        {
            auto& row = layout.entries[index];
            const bool fullHeight = this->viewport_.height > 0 && rowCount > 0 && y + row.footprint.height > this->viewport_.height;
            if (rowCount >= rows || fullHeight) { ++column; rowCount = 0; y = 0; }
            if (column >= columns || (this->viewport_.width > 0 && column > 0 && (column + 1) * columnWidth + column * this->column_gap_ > this->viewport_.width)) break;
            row.column = column;
            const float xOffset = column * (columnWidth + this->column_gap_) - left;
            const float yOffset = y - row.footprint.y;
            Offset(row.bounds, xOffset, yOffset); Offset(row.target, xOffset, yOffset);
            Offset(row.statuses, xOffset, yOffset); Offset(row.footprint, xOffset, yOffset);
            y += row.footprint.height + this->gap_;
            totalHeight = std::max(totalHeight, y - this->gap_);
            ++rowCount;
            ++layout.count;
        }
        if (layout.count == 0) return layout;
        const auto lastColumn = layout.entries[layout.count - 1].column;
        layout.bounds = {this->state_.bounds.x, this->state_.bounds.y, (lastColumn + 1) * columnWidth + lastColumn * this->column_gap_, totalHeight};
        if (this->viewport_.width > 0)
            layout.bounds.x = std::clamp(layout.bounds.x, this->viewport_.x, this->viewport_.x + std::max(0.0f, this->viewport_.width - layout.bounds.width));
        if (this->viewport_.height > 0)
            layout.bounds.y = std::clamp(layout.bounds.y, this->viewport_.y, this->viewport_.y + std::max(0.0f, this->viewport_.height - layout.bounds.height));
        for (size_t index = 0; index < layout.count; ++index)
        {
            auto& row = layout.entries[index];
            Offset(row.bounds, layout.bounds.x, layout.bounds.y); Offset(row.target, layout.bounds.x, layout.bounds.y);
            Offset(row.statuses, layout.bounds.x, layout.bounds.y); Offset(row.footprint, layout.bounds.x, layout.bounds.y);
        }
        return layout;
    }

    auto enemy_list_module::Cast(const enemy_entry& entry) const -> combat_cast
    {
        if (const auto* actor = this->combat_ != nullptr ? this->combat_->Find(entry.id) : nullptr) return actor->cast;
        return {entry.cast_name, entry.cast_started, entry.interrupted_at, entry.cast_seconds};
    }

    auto enemy_list_module::Statuses(const uint32_t id) const -> std::span<const combat_status>
    {
        const auto* actor = this->combat_ != nullptr ? this->combat_->Find(id) : nullptr;
        return actor != nullptr ? std::span<const combat_status>{actor->statuses} : std::span<const combat_status>{};
    }

    auto enemy_list_module::HitTarget(const float screenX, const float screenY, const uint64_t now) const -> std::pair<uint32_t, uint32_t>
    {
        const float x = this->State().transform.InverseX(screenX), y = this->State().transform.InverseY(screenY);
        if (!this->state_.enabled || !this->click_target_) return {};
        const auto& entries = this->source_.Entries();
        const auto layout = this->LayoutEntries(entries, now);
        for (size_t i = 0; i < layout.count; ++i)
        {
            const auto& row = layout.entries[i];
            if (Contains(row.bounds, x, y) || Contains(row.target, x, y))
                return {entries[i].id, entries[i].index};
        }
        return {};
    }

    auto enemy_list_module::BeginControllerBrowse(const uint32_t preferredId, const uint32_t preferredIndex) -> bool
    {
        this->EndControllerBrowse();
        if (!this->ShouldRender({})) return false;
        const auto& entries = this->source_.Entries();
        const auto count = this->LayoutEntries(entries, ::GetTickCount64()).count;
        const enemy_entry* candidate{};
        for (size_t index = 0; index < count; ++index)
        {
            const auto& entry = entries[index];
            if (entry.id == 0 || entry.index == 0) continue;
            if (preferredId != 0)
            {
                if (entry.id == preferredId && (preferredIndex == 0 || entry.index == preferredIndex)) { candidate = &entry; break; }
            }
            else if (candidate == nullptr || entry.selected) candidate = &entry;
            if (preferredId == 0 && entry.selected) break;
        }
        if (candidate == nullptr) return false;
        this->controller_browsing_ = true;
        this->controller_enemy_id_ = candidate->id;
        this->controller_enemy_index_ = candidate->index;
        return true;
    }

    auto enemy_list_module::EndControllerBrowse(void) -> void
    {
        this->controller_browsing_ = false;
        this->controller_enemy_id_ = this->controller_enemy_index_ = 0;
        this->EndControllerStatus();
    }

    auto enemy_list_module::ResolveControllerMember(void) -> const enemy_entry*
    {
        if (this->ControllerFocused())
        {
            const auto& entries = this->source_.Entries();
            const auto count = this->LayoutEntries(entries, ::GetTickCount64()).count;
            for (size_t index = 0; index < count; ++index)
                if (entries[index].id == this->controller_enemy_id_ && entries[index].index == this->controller_enemy_index_)
                    return &entries[index];
        }
        this->EndControllerBrowse();
        return nullptr;
    }

    auto enemy_list_module::ControllerMember(void) -> std::optional<enemy_entry>
    {
        const auto* entry = this->ResolveControllerMember();
        return entry != nullptr ? std::optional<enemy_entry>{*entry} : std::nullopt;
    }

    auto enemy_list_module::MoveControllerBrowse(const int direction) -> bool
    {
        const auto* current = this->ResolveControllerMember();
        if (current == nullptr || direction == 0) return false;
        const auto& entries = this->source_.Entries();
        const int count = static_cast<int>(this->LayoutEntries(entries, ::GetTickCount64()).count);
        const auto position = static_cast<int>(current - entries.data());
        const int step = direction > 0 ? 1 : -1;
        for (int next = position + step; next >= 0 && next < count; next += step)
        {
            const auto& entry = entries[static_cast<size_t>(next)];
            if (entry.id == 0 || entry.index == 0) continue;
            this->controller_enemy_id_ = entry.id;
            this->controller_enemy_index_ = entry.index;
            this->EndControllerStatus();
            return true;
        }
        return false;
    }

    auto enemy_list_module::MoveControllerBrowseHorizontal(const int direction) -> bool
    {
        const auto* current = this->ResolveControllerMember();
        if (current == nullptr || direction == 0) return false;
        const auto& entries = this->source_.Entries();
        const auto layout = this->LayoutEntries(entries, ::GetTickCount64());
        const auto& active = layout.entries[static_cast<size_t>(current - entries.data())];
        const int column = static_cast<int>(active.column) + (direction > 0 ? 1 : -1);
        const entry_layout* candidate{};
        float nearest = 1.0e9f;
        for (size_t index = 0; index < layout.count; ++index)
        {
            const auto& row = layout.entries[index];
            if (static_cast<int>(row.column) != column || entries[index].id == 0 || entries[index].index == 0) continue;
            const float distance = std::abs(row.bounds.y + row.bounds.height * 0.5f - active.bounds.y - active.bounds.height * 0.5f);
            if (distance < nearest) { nearest = distance; candidate = &row; }
        }
        if (candidate == nullptr) return false;
        this->controller_enemy_id_ = entries[candidate->index].id;
        this->controller_enemy_index_ = entries[candidate->index].index;
        this->EndControllerStatus();
        return true;
    }

    auto enemy_list_module::ControllerStatusIndex(const std::span<const combat_status> statuses) const -> std::optional<size_t>
    {
        if (!this->controller_status_browsing_ || !this->statuses_.enabled || !this->statuses_.tooltips) return std::nullopt;
        const auto count = std::min(statuses.size(), static_cast<size_t>(this->statuses_.limit));
        size_t occurrence{};
        for (size_t index = 0; index < count; ++index)
            if (statuses[index].id == this->controller_status_id_ && occurrence++ == this->controller_status_occurrence_) return index;
        return std::nullopt;
    }

    auto enemy_list_module::FocusControllerStatus(const std::span<const combat_status> statuses, const size_t index) -> bool
    {
        if (!this->statuses_.enabled || !this->statuses_.tooltips || index >= std::min(statuses.size(), static_cast<size_t>(this->statuses_.limit))) return false;
        this->controller_status_browsing_ = true;
        this->controller_status_id_ = statuses[index].id;
        this->controller_status_occurrence_ = 0;
        for (size_t previous = 0; previous < index; ++previous)
            if (statuses[previous].id == this->controller_status_id_) ++this->controller_status_occurrence_;
        return true;
    }

    auto enemy_list_module::BeginControllerStatus(void) -> bool
    {
        this->EndControllerStatus();
        const auto* current = this->ResolveControllerMember();
        return current != nullptr && this->FocusControllerStatus(this->Statuses(current->id), 0);
    }

    auto enemy_list_module::EndControllerStatus(void) -> void
    {
        this->controller_status_browsing_ = false;
        this->controller_status_id_ = 0;
        this->controller_status_occurrence_ = 0;
    }

    auto enemy_list_module::MoveControllerStatus(const int direction) -> bool
    {
        const auto* current = this->ResolveControllerMember();
        if (current == nullptr || direction == 0 || !this->statuses_.enabled || !this->statuses_.tooltips) return false;
        const auto statuses = this->Statuses(current->id);
        const auto index = this->ControllerStatusIndex(statuses);
        if (!index) { this->EndControllerStatus(); return false; }
        const auto count = std::min(statuses.size(), static_cast<size_t>(this->statuses_.limit));
        return this->FocusControllerStatus(statuses, (*index + (direction > 0 ? 1 : count - 1)) % count);
    }

    auto enemy_list_module::ControllerStatusBounds(void) const -> ui::rect
    {
        if (!this->ControllerFocused()) return {};
        const auto& entries = this->source_.Entries();
        const auto layout = this->LayoutEntries(entries, ::GetTickCount64());
        for (size_t index = 0; index < layout.count; ++index)
        {
            const auto& entry = entries[index];
            if (entry.id != this->controller_enemy_id_ || entry.index != this->controller_enemy_index_) continue;
            const auto statuses = this->Statuses(entry.id);
            const auto status = this->ControllerStatusIndex(statuses);
            if (!status) return {};
            return this->state_.transform.Map(ui::LayoutStatusStrip(statuses.size(), layout.entries[index].statuses, this->statuses_).Icon(*status));
        }
        return {};
    }

    auto enemy_list_module::ControllerBounds(uint64_t now) const -> ui::rect
    {
        if (!this->ControllerFocused()) return {};
        if (now == 0) now = ::GetTickCount64();
        const auto& entries = this->source_.Entries();
        const auto layout = this->LayoutEntries(entries, now);
        for (size_t index = 0; index < layout.count; ++index)
        {
            const auto& entry = entries[index];
            if (entry.id == this->controller_enemy_id_ && entry.index == this->controller_enemy_index_)
                return layout.entries[index].bounds;
        }
        return {};
    }

    auto enemy_list_module::DrawRow(const enemy_entry& entry, const entry_layout& layout, const uint64_t now, const module_render_context& context) -> void
    {
        auto style = this->container_;
        style.border = this->borders_;
        const auto content = context.DrawContainer(layout.bounds, style);
        if (this->borders_ && this->claim_border_)
            context.Design().BorderRect(layout.bounds, entry.name_color);
        const auto& design = context.Design();
        const auto text = [&](std::string value, const ui::rect& line, size_t profile, std::optional<D3DCOLOR> tint = std::nullopt) {
            const auto style = tint ? TintTextProfile(this->text_[profile], *tint) : this->text_[profile];
            value = xiui::ui::FitText(design, std::move(value), line.width, style.style);
            context.DrawText(value.c_str(), line, line, style);
        };
        float y = content.y;
        const float headerHeight = this->HeaderHeight(entry);
        float nameWidth = content.width;
        if (this->targets_ && !this->target_panel_ && !entry.target_name.empty())
        {
            const auto target = "-> " + entry.target_name;
            const float targetWidth = std::min(design.MeasureText(target.c_str(), this->text_[2].style), content.width * 0.45f);
            nameWidth = std::max(0.0f, content.width - targetWidth - 8.0f);
            text(target, {content.x + content.width - targetWidth, y, targetWidth, headerHeight}, 2,
                entry.attacking_player ? std::optional{this->aggro_color_} : std::nullopt);
        }
        text(entry.name, {content.x, y, nameWidth, headerHeight}, 0, entry.name_color);
        y += headerHeight + 2;
        char distance[32]{}, percent[16]{};
        if (this->distance_)
            std::snprintf(distance, sizeof(distance), "%.1f yalms", entry.distance);
        if (this->percent_)
            std::snprintf(percent, sizeof(percent), "%u%%", entry.hp_percent);
        auto labelText = this->text_[1], valueText = this->text_[1];
        labelText.style.alignment = ui::text_alignment::left;
        valueText.style.alignment = ui::text_alignment::right;
        context.DrawMeter({content.x, y, content.width, this->meter_.height + 2}, distance, percent, entry.hp_percent / 100.0f,
            this->hp_top_, this->hp_bottom_, this->meter_, this->animations_[entry.id], true, &labelText, &valueText,
            ui::meter_text_anchor::bottom, ui::meter_text_anchor::automatic, meter_fill_mode::health);
        y += this->meter_.height + 2;
        if (this->distance_ || this->percent_) y += std::max(16.0f, 18.0f * this->text_[1].style.scale) * 0.5f;
        const auto cast = this->Cast(entry);
        if (this->casts_ && cast.Active(now))
        {
            y += 2;
            const bool interrupted = cast.interrupted != 0;
            auto meter = this->meter_;
            meter.height = 6; meter.shadow_enabled = meter.sheen_enabled = false;
            const float progress = cast.Ratio(now);
            const auto castColor = interrupted ? 0xFFFFFFFF : this->cast_color_;
            context.DrawBar({content.x, y, content.width, 6}, progress, castColor, castColor, 0, meter);
            text(interrupted ? "Interrupted" : cast.name, {content.x, y + 8, content.width, LineHeight(this->text_[3])}, 3, castColor);
            y += LineHeight(this->text_[3]) + 8;
        }
        if (this->timers_.enabled)
        {
            auto attack = this->timing_ != nullptr ? this->timing_->Enemy(entry.id, now / 1000.0) : attack_prediction{};
            if (context.IsEditing()) attack = {true, true, false, 0.6f, 1.6f, 4};
            const auto timingHeight = ui::DrawTiming(attack, "Attack", "", {content.x, y + 2, content.width, 0}, this->meter_, this->timers_.text,
                this->timers_.height, this->timers_.text_only, this->timers_.drain, this->timers_.color, this->timers_.ready_color, 0.5f, context);
            if (timingHeight > 0) y += timingHeight + 2;
        }
        ui::DrawStatusStrip(this->Statuses(entry.id), layout.statuses, now, this->text_[1], context, this->statuses_);
        if (layout.target.width > 0)
        {
            auto targetStyle = this->target_container_;
            targetStyle.border = this->target_borders_;
            const auto targetContent = context.DrawContainer(layout.target, targetStyle);
            text(entry.target_name, targetContent, 2, entry.attacking_player ? std::optional{this->aggro_color_} : std::nullopt);
        }
    }

    auto enemy_list_module::RenderStatusTooltip(const module_render_context& context, IResourceManager* resources,
        const float x, const float y, const bool controller) const -> void
    {
        if (!this->ShouldRender({}) || !this->statuses_.tooltips) return;
        const auto now = ::GetTickCount64();
        const auto& entries = this->source_.Entries();
        const auto layout = this->LayoutEntries(entries, now);
        for (size_t index = 0; index < layout.count; ++index)
        {
            const auto& entry = entries[index];
            const auto statuses = this->Statuses(entry.id);
            std::optional<ui::status_tooltip> tooltip;
            if (controller)
            {
                if (!this->ControllerFocused() || entry.id != this->controller_enemy_id_ || entry.index != this->controller_enemy_index_) continue;
                const auto strip = ui::LayoutStatusStrip(statuses.size(), layout.entries[index].statuses, this->statuses_);
                if (strip.count == 0) return;
                const auto status = this->ControllerStatusIndex(statuses);
                if (!status || *status >= strip.count) return;
                tooltip = ui::status_tooltip{statuses[*status], strip.Icon(*status)};
            }
            else tooltip = ui::StatusStripHit(statuses, layout.entries[index].statuses, this->statuses_, this->state_.transform.InverseX(x), this->state_.transform.InverseY(y));
            if (!tooltip) continue;
            tooltip->entity = entry.id;
            tooltip->bounds = this->state_.transform.Map(tooltip->bounds);
            ui::DrawStatusTooltip(*tooltip, resources, this->text_[1], context);
            return;
        }
    }

    auto enemy_list_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto now = ::GetTickCount64();
        this->timing_preview_ = context.IsEditing();
        this->viewport_ = context.Viewport();
        std::vector<enemy_entry> preview;
        const auto* entries = &this->source_.Entries();
        if (context.IsEditing())
        {
            preview.resize(3);
            constexpr std::array names{"Orcish Grunt", "Wild Rabbit", "Goblin Weaver"};
            for (size_t i = 0; i < preview.size(); ++i)
            {
                auto& enemy = preview[i];
                enemy.id = static_cast<uint32_t>(i + 1); enemy.name = names[i];
                enemy.hp_percent = 82 - static_cast<uint32_t>(i) * 27;
                enemy.distance = 6.5f + static_cast<float>(i) * 3.1f;
                enemy.claimed = i != 1; enemy.selected = i == 0;
                enemy.name_color = EntityNameColor(0x10, i != 1 ? 1 : 0, i == 0, false);
                enemy.target_name = snapshot.player_name[0] != '\0' ? snapshot.player_name : "Player";
                enemy.attacking_player = i == 1;
            }
            preview[2].cast_name = "Stone"; preview[2].cast_seconds = 4;
            preview[2].cast_started = now > 1500 ? now - 1500 : 0;
            entries = &preview;
        }
        const auto layout = this->LayoutEntries(*entries, now);
        for (size_t i = 0; i < layout.count; ++i)
            this->DrawRow((*entries)[i], layout.entries[i], now, context);
        this->state_.minimum_height = layout.count > 0 ? layout.bounds.height : 50;
        this->state_.bounds.height = this->state_.minimum_height;
        this->state_.content_bounds = layout.count > 0 ? ui::rect{layout.bounds.x - this->state_.bounds.x, layout.bounds.y - this->state_.bounds.y,
            layout.bounds.width, layout.bounds.height} : ui::rect{};
        std::erase_if(this->animations_, [&](const auto& animation) {
            return std::none_of(entries->begin(), entries->end(), [&](const auto& enemy) { return enemy.id == animation.first; });
        });
    }
} // namespace xiui
