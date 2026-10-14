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

    auto FitText(const xiui::ui::design_system& design, std::string text, float width, const xiui::ui::text_style& style) -> std::string
    {
        if (design.MeasureText(text.c_str(), style) <= width) return text;
        while (!text.empty())
        {
            text.pop_back();
            if (design.MeasureText((text + "...").c_str(), style) <= width) return text + "...";
        }
        return {};
    }

    auto LineHeight(const xiui::module_text_profile& profile) -> float { return std::max(14.0f, 18.0f * profile.style.scale); }

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
        this->timers_.Load(store, "timing.enemylist");
        this->state_.enabled = store.ReadBool("enemylist", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.enemylist", this->state_.bounds);
        this->distance_ = store.ReadBool("enemylist", "distance", true);
        this->percent_ = store.ReadBool("enemylist", "hp_percent", true);
        this->targets_ = store.ReadBool("enemylist", "enemy_targets", true);
        this->casts_ = store.ReadBool("enemylist", "casts", true);
        this->include_selected_ = store.ReadBool("enemylist", "include_selected", false);
        this->max_rows_ = std::clamp(store.ReadFloat("enemylist", "max_rows", 8), 1.0f, 12.0f);
        this->gap_ = std::clamp(store.ReadFloat("enemylist", "spacing", 6), 0.0f, 24.0f);
        this->selected_text_ = std::clamp(store.ReadInt("enemylist", "selected_text", 0), 0, 4);
        this->container_ = {.opacity = std::clamp(store.ReadFloat("enemylist", "opacity", 1), 0.1f, 1.0f),
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
    }

    auto enemy_list_module::Save(const settings_store& store) const -> void
    {
        this->timers_.Save(store, "timing.enemylist");
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
        store.WriteFloat("enemylist", "spacing", this->gap_);
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

    auto enemy_list_module::ConfigSections(void) const -> std::span<const module_config_section> { return ui::TimingSections(); }

    auto enemy_list_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= timing_options_start) return section == 7;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= 16 && index < 16 + meter_style_option_count) return topic == MeterConfigSection(index - 16);
        if (index >= 16 + meter_style_option_count || index == 6) return topic == bar_config_section::text;
        if (index >= 12) return topic == bar_config_section::colors;
        if (index >= 7) return topic == bar_config_section::layout;
        return topic == bar_config_section::general;
    }

    auto enemy_list_module::Option(const size_t index) -> module_option
    {
        if (index >= timing_options_start) return this->timers_.Option(index - timing_options_start);
        static constexpr const char* textNames[]{"Enemy name", "Details", "Enemy target", "Cast", "Attack timing"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show enemies fighting your group.", &this->state_.enabled);
        case 1: return module_option::Toggle("Distance", "Show distance in yalms.", &this->distance_);
        case 2: return module_option::Toggle("HP percent", "Show enemy health percent.", &this->percent_);
        case 3: return module_option::Toggle("Enemy targets", "Show the target beside the enemy name.", &this->targets_);
        case 4: return module_option::Toggle("Casts", "Show enemy spells and abilities.", &this->casts_);
        case 5: return module_option::Toggle("Include selected", "Also list an unclaimed selected enemy.", &this->include_selected_);
        case 6: return module_option::Element("Selected text", "Text element configured in Text.", &this->selected_text_, textNames, 5);
        case 7: return ModuleWidthOption(this->state_);
        case 8: return module_option::Slider("Visible enemies", "Maximum number of enemy cards.", &this->max_rows_, 1, 12, 1, module_option_group::appearance);
        case 9: return module_option::Slider("Spacing", "Gap between enemy containers.", &this->gap_, 0, 24, 1, module_option_group::appearance);
        case 10: return module_option::Slider("Opacity", "Fade the native window skin.", &this->container_.opacity, 0.1f, 1, 0.05f, module_option_group::appearance);
        case 11: return module_option::Slider("Padding", "Space inside each enemy container.", &this->container_.padding, 4, 24, 1, module_option_group::appearance);
        case 12: return module_option::Color("HP fill top", "Upper health gradient.", &this->hp_top_, module_option_group::colors);
        case 13: return module_option::Color("HP fill bottom", "Lower health gradient.", &this->hp_bottom_, module_option_group::colors);
        case 14: return module_option::Color("Attacking you", "Target text when an enemy attacks you.", &this->aggro_color_, module_option_group::colors);
        case 15: return module_option::Color("Cast", "Enemy cast progress and name.", &this->cast_color_, module_option_group::colors);
        default:
            if (index < 16 + meter_style_option_count) return MeterStyleOption(index - 16, this->meter_);
            const auto textIndex = index - 16 - meter_style_option_count;
            auto option = TextProfileOption(textIndex, this->selected_text_ == 4 ? this->timers_.text : this->text_[this->selected_text_]);
            if (this->selected_text_ == 0 && (textIndex == 5 || textIndex == 6))
            {
                option.available = false;
                option.description = "Name color follows the game state.";
            }
            return option;
        }
    }

    auto enemy_list_module::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        if (this->state_.enabled) this->source_.Update(core, now, this->include_selected_);
        else this->source_.Clear();
        if (this->source_.Entries().empty()) this->animations_.clear();
    }

    auto enemy_list_module::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (this->state_.enabled) this->source_.HandlePacket(core, id, data, now);
    }

    auto enemy_list_module::HeaderHeight(const enemy_entry& entry) const -> float
    {
        return this->targets_ && !entry.target_name.empty()
            ? std::max(LineHeight(this->text_[0]), LineHeight(this->text_[2])) : LineHeight(this->text_[0]);
    }

    auto enemy_list_module::RowHeight(const enemy_entry& entry, const uint64_t now) const -> float
    {
        float height = 2 * (this->container_.edge_size + this->container_.padding) + this->HeaderHeight(entry) + 2 + this->meter_.height + 2;
        if (this->distance_ || this->percent_) height += std::max(16.0f, 18.0f * this->text_[1].style.scale) * 0.5f;
        if (this->casts_ && this->Cast(entry).Active(now)) height += LineHeight(this->text_[3]) + 10;
        if (this->timers_.enabled && (this->timing_preview_ || (this->timing_ != nullptr && this->timing_->Enemy(entry.id, now / 1000.0).active)))
            height += 2 + ui::TimingHeight(this->timers_.text, this->timers_.height, this->timers_.text_only);
        height += ui::StatusStripHeight(this->Statuses(entry.id).size(), this->state_.bounds.width - 2 * (this->container_.edge_size + this->container_.padding));
        return std::ceil(height);
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

    auto enemy_list_module::HitTarget(const float x, const float y, const uint64_t now) const -> std::pair<uint32_t, uint32_t>
    {
        if (!this->state_.enabled) return {};
        float top = this->state_.bounds.y;
        const auto& entries = this->source_.Entries();
        for (size_t i = 0; i < std::min(entries.size(), static_cast<size_t>(this->max_rows_)); ++i)
        {
            const auto height = this->RowHeight(entries[i], now);
            if (x >= this->state_.bounds.x && x < this->state_.bounds.x + this->state_.bounds.width && y >= top && y < top + height)
                return {entries[i].id, entries[i].index};
            top += height + this->gap_;
        }
        return {};
    }

    auto enemy_list_module::DrawRow(const enemy_entry& entry, const ui::rect& bounds, const uint64_t now, const module_render_context& context) -> void
    {
        const auto content = context.DrawContainer(bounds, this->container_);
        const auto& design = context.Design();
        const auto text = [&](std::string value, const ui::rect& line, size_t profile, std::optional<D3DCOLOR> tint = std::nullopt) {
            const auto style = tint ? TintTextProfile(this->text_[profile], *tint) : this->text_[profile];
            value = FitText(design, std::move(value), line.width, style.style);
            context.DrawText(value.c_str(), line, line, style);
        };
        float y = content.y;
        const float headerHeight = this->HeaderHeight(entry);
        float nameWidth = content.width;
        if (this->targets_ && !entry.target_name.empty())
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
            ui::meter_text_anchor::bottom);
        y += this->meter_.height + 2;
        if (this->distance_ || this->percent_) y += std::max(16.0f, 18.0f * this->text_[1].style.scale) * 0.5f;
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
        ui::DrawStatusStrip(this->Statuses(entry.id), {content.x, y + 2, content.width, bounds.height}, now, this->text_[1], context);
    }

    auto enemy_list_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto now = ::GetTickCount64();
        this->timing_preview_ = context.IsEditing();
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
        const size_t count = std::min(entries->size(), static_cast<size_t>(this->max_rows_));
        float y = this->state_.bounds.y;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& entry = (*entries)[i];
            const float height = this->RowHeight(entry, now);
            this->DrawRow(entry, {this->state_.bounds.x, y, this->state_.bounds.width, height}, now, context);
            y += height + this->gap_;
        }
        this->state_.minimum_height = count > 0 ? y - this->state_.bounds.y - this->gap_ : 50;
        this->state_.bounds.height = this->state_.minimum_height;
        std::erase_if(this->animations_, [&](const auto& animation) {
            return std::none_of(entries->begin(), entries->end(), [&](const auto& enemy) { return enemy.id == animation.first; });
        });
    }
} // namespace xiui
