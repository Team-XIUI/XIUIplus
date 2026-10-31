#include "recast_module.hpp"
#include "ui/game_icon_cache.hpp"
#include "modules/bar_config.hpp"
#include "ui/recast_icons.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    auto Blend(const D3DCOLOR from, const D3DCOLOR to, const float amount) -> D3DCOLOR
    {
        D3DCOLOR result{};
        for (unsigned shift : {0u, 8u, 16u, 24u})
        {
            const float first = static_cast<float>((from >> shift) & 255), last = static_cast<float>((to >> shift) & 255);
            result |= static_cast<D3DCOLOR>(std::lround(first + (last - first) * std::clamp(amount, 0.0f, 1.0f))) << shift;
        }
        return result;
    }

    auto FitName(const xiui::ui::design_system& design, std::string name, const float width, const xiui::ui::text_style& style) -> std::string
    {
        if (design.MeasureText(name.c_str(), style) <= width) return name;
        while (!name.empty())
        {
            size_t last = name.size() - 1;
            while (last > 0 && (static_cast<unsigned char>(name[last]) & 0xC0) == 0x80) --last;
            name.resize(last);
            if (design.MeasureText((name + "...").c_str(), style) <= width) return name + "...";
        }
        return {};
    }
}

namespace xiui
{
    auto recast_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    { this->meter_ = style; this->meter_.height = 18; }

    auto recast_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.meter_text}, module_text_profile{theme.meter_text}, module_text_profile{theme.title_text}};
        this->text_[0].style.alignment = this->text_[2].style.alignment = ui::text_alignment::left;
        this->text_[1].style.alignment = ui::text_alignment::right;
        for (size_t i = 0; i < 2; ++i)
        {
            this->text_[i].style.top_color |= 0x00FFFFFF;
            this->text_[i].style.bottom_color |= 0x00FFFFFF;
        }
    }

    auto recast_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        auto* source = this->source_;
        *this = {};
        this->state_.bounds = bounds; this->source_ = source;
        this->ApplyMeterDefaults(theme.meter); this->ApplyTextDefaults(theme);
    }

    auto recast_module::Load(const settings_store& store) -> void
    {
        this->state_.bounds = store.ReadRect("layout.recast", this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, 160.0f, 1200.0f);
        for (const auto& [key, value] : {std::pair{"enabled", &this->state_.enabled}, {"abilities", &this->abilities_}, {"spells", &this->spells_},
            {"icons", &this->icons_}, {"heading", &this->heading_}, {"timer", &this->timer_}, {"charges", &this->charges_},
            {"tenths", &this->tenths_}, {"tint_text", &this->tint_text_}, {"background", &this->background_}})
            *value = store.ReadBool("recast", key, *value);
        for (const auto& [key, value] : {std::pair{"display", &this->display_}, {"anchor", &this->anchor_}, {"fill_direction", &this->fill_direction_}})
            *value = std::clamp(store.ReadInt("recast", key, *value), 0, 1);
        for (const auto& [key, value] : {std::pair{"sorting", &this->sorting_}, {"color_mode", &this->color_mode_}, {"selected_text", &this->selected_text_}})
            *value = std::clamp(store.ReadInt("recast", key, *value), 0, 2);
        this->maximum_rows_ = std::round(std::clamp(store.ReadFloat("recast", "maximum_rows", this->maximum_rows_), 1.0f, 20.0f));
        this->row_height_ = std::clamp(store.ReadFloat("recast", "row_height", this->row_height_), 16.0f, 80.0f);
        this->spacing_ = std::clamp(store.ReadFloat("recast", "spacing", this->spacing_), 0.0f, 32.0f);
        this->icon_size_ = std::clamp(store.ReadFloat("recast", "icon_size", this->icon_size_), 12.0f, 64.0f);
        this->ready_hold_ = std::clamp(store.ReadFloat("recast", "ready_hold", this->ready_hold_), 0.0f, 10.0f);
        this->soon_seconds_ = std::clamp(store.ReadFloat("recast", "soon_seconds", this->soon_seconds_), 1.0f, 300.0f);
        this->near_seconds_ = std::clamp(store.ReadFloat("recast", "near_seconds", this->near_seconds_), 0.0f, 60.0f);
        constexpr const char* colors[]{"cooldown_top", "cooldown_bottom", "soon_top", "soon_bottom", "ready_top", "ready_bottom"};
        for (size_t i = 0; i < std::size(colors); ++i)
        {
            auto& color = i < 2 ? this->cooldown_[i] : i < 4 ? this->soon_[i - 2] : this->ready_[i - 4];
            color = store.ReadArgb("recast", colors[i], color);
        }
        this->meter_ = LoadMeterStyle(store, "meter.recast", this->meter_);
        constexpr const char* profiles[]{"text.recast.name", "text.recast.timer", "text.recast.heading"};
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, profiles[i], this->text_[i]);
        if (store.ReadInt("recast", "text_color_version", 0) < 1)
        {
            this->tint_text_ = false;
            for (size_t i = 0; i < 2; ++i)
            {
                this->text_[i].style.top_color |= 0x00FFFFFF;
                this->text_[i].style.bottom_color |= 0x00FFFFFF;
            }
        }
    }

    auto recast_module::Save(const settings_store& store) const -> void
    {
        store.WriteRect("layout.recast", this->state_.bounds);
        store.WriteInt("recast", "text_color_version", 1);
        for (const auto& [key, value] : {std::pair{"enabled", this->state_.enabled}, {"abilities", this->abilities_}, {"spells", this->spells_},
            {"icons", this->icons_}, {"heading", this->heading_}, {"timer", this->timer_}, {"charges", this->charges_},
            {"tenths", this->tenths_}, {"tint_text", this->tint_text_}, {"background", this->background_}})
            store.WriteBool("recast", key, value);
        for (const auto& [key, value] : {std::pair{"display", this->display_}, {"anchor", this->anchor_}, {"fill_direction", this->fill_direction_},
            {"sorting", this->sorting_}, {"color_mode", this->color_mode_}, {"selected_text", this->selected_text_}})
            store.WriteInt("recast", key, value);
        for (const auto& [key, value] : {std::pair{"maximum_rows", this->maximum_rows_}, {"row_height", this->row_height_}, {"spacing", this->spacing_},
            {"icon_size", this->icon_size_}, {"ready_hold", this->ready_hold_}, {"soon_seconds", this->soon_seconds_}, {"near_seconds", this->near_seconds_}})
            store.WriteFloat("recast", key, value);
        constexpr const char* colors[]{"cooldown_top", "cooldown_bottom", "soon_top", "soon_bottom", "ready_top", "ready_bottom"};
        for (size_t i = 0; i < std::size(colors); ++i)
            store.WriteArgb("recast", colors[i], i < 2 ? this->cooldown_[i] : i < 4 ? this->soon_[i - 2] : this->ready_[i - 4]);
        SaveMeterStyle(store, "meter.recast", this->meter_);
        constexpr const char* profiles[]{"text.recast.name", "text.recast.timer", "text.recast.heading"};
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, profiles[i], this->text_[i]);
    }

    auto recast_module::ConfigSections(void) const -> std::span<const module_config_section> { return BarConfigSections(); }

    auto recast_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= base_options + meter_style_option_count || index == selected_text) return topic == bar_config_section::text;
        if (index >= base_options) return topic == MeterConfigSection(index - base_options);
        if (index >= color_mode && index <= ready_bottom) return topic == bar_config_section::colors;
        if (index >= width && index <= fill_direction) return topic == bar_config_section::layout;
        return topic == bar_config_section::general;
    }

    auto recast_module::ContentOption(const size_t index) -> module_option
    {
        static constexpr const char* displays[]{"Bars + icons", "Text only"};
        static constexpr const char* anchors[]{"Left", "Right"};
        static constexpr const char* fills[]{"Fill toward ready", "Drain toward ready"};
        static constexpr const char* sorts[]{"Ready soonest", "Recently used", "Name"};
        static constexpr const char* colors[]{"Smooth transition", "Thresholds", "Fixed"};
        static constexpr const char* profiles[]{"Action name", "Countdown", "Heading"};
        if (index >= base_options + meter_style_option_count)
            return TextProfileOption(index - base_options - meter_style_option_count, this->text_[this->selected_text_]).VisibleWhen(
                this->selected_text_ == 0 || (this->selected_text_ == 1 ? this->timer_ : this->heading_));
        if (index >= base_options)
        {
            const auto field = index - base_options;
            auto option = MeterStyleOption(field, this->meter_);
            return option.VisibleWhen(this->display_ == 0 && field != 16 && field != 17);
        }
        const auto barOption = [&](module_option option) { return option.VisibleWhen(this->display_ == 0); };
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show your active spell and ability cooldowns.", &this->state_.enabled).Ordered(-50);
        case display: return module_option::Choice("Display", "Choose illustrated cooldown bars or a compact text-only list.", &this->display_, displays, 2).Ordered(-30);
        case abilities: return module_option::Toggle("Job abilities", "Include shared recast groups and charge abilities.", &this->abilities_);
        case spells: return module_option::Toggle("Spells", "Include spells currently on cooldown.", &this->spells_);
        case icons: return barOption(module_option::Toggle("Action icons", "Show spell and ability artwork beside the bars.", &this->icons_));
        case heading: return module_option::Toggle("Heading", "Show Recast above the list.", &this->heading_);
        case timer: return module_option::Toggle("Countdown", "Show remaining cooldown or Ready after completion.", &this->timer_);
        case charges: return module_option::Toggle("Charge count", "Show available and maximum charges beside charge abilities.", &this->charges_).VisibleWhen(this->abilities_);
        case tenths: return module_option::Toggle("Tenths", "Show tenths of a second below one minute.", &this->tenths_).VisibleWhen(this->timer_);
        case tint_text: return module_option::Toggle("Cooldown text colors", "Tint names and timers as readiness approaches, retaining text effects.", &this->tint_text_);
        case background: return module_option::Toggle("Container background", "Draw a native game window behind the list.", &this->background_).Ordered(-10);
        case width: return ModuleWidthOption(this->state_);
        case maximum_rows: return module_option::Slider("Maximum rows", "Maximum cooldowns visible at once, using the selected sort order.", &this->maximum_rows_, 1, 20, 1).Ordered(122);
        case row_height: return module_option::Slider("Row height", "Minimum height of each row; text and icons retain enough room.", &this->row_height_, 16, 80, 1).Ordered(112);
        case spacing: return module_option::Slider("Spacing", "Gap between cooldown rows.", &this->spacing_, 0, 32, 1).Ordered(300, true);
        case icon_size: return barOption(module_option::Slider("Icon size", "Spell and ability icon size in pixels.", &this->icon_size_, 12, 64, 1).Ordered(110)).VisibleWhen(this->icons_);
        case anchor: return module_option::Choice("Anchor", "Fill from this edge, with icons and action names on the same side.", &this->anchor_, anchors, 2).Ordered(200);
        case sorting: return module_option::Choice("Sort order", "Choose which cooldowns appear first.", &this->sorting_, sorts, 3);
        case fill_direction: return barOption(module_option::Choice("Fill behavior", "Grow as readiness approaches, or drain the remaining cooldown.", &this->fill_direction_, fills, 2));
        case ready_hold: return module_option::Slider("Ready display", "Seconds to retain a completed cooldown. Zero hides it immediately.", &this->ready_hold_, 0, 10, 0.5f);
        case color_mode: return module_option::Choice("Color changes", "Blend toward ready, switch at time thresholds, or keep the cooldown colors.", &this->color_mode_, colors, 3).VisibleWhen(this->display_ == 0 || this->tint_text_);
        case soon_seconds: return module_option::Slider("Approaching ready", "Seconds remaining before the approaching colors apply; kept above Nearly ready.", &this->soon_seconds_, 1, 300, 1).VisibleWhen((this->display_ == 0 || this->tint_text_) && this->color_mode_ != 2);
        case near_seconds: return module_option::Slider("Nearly ready", "Seconds remaining for the final transition toward ready colors.", &this->near_seconds_, 0, 60, 1).VisibleWhen((this->display_ == 0 || this->tint_text_) && this->color_mode_ != 2);
        case cooldown_top: return module_option::Color("Cooldown top", "Top of the bar while readiness is far away.", &this->cooldown_[0]).Ordered(500, true).VisibleWhen(this->display_ == 0);
        case cooldown_bottom: return module_option::Color("Cooldown bottom", "Bottom of the bar and text tint while readiness is far away.", &this->cooldown_[1]).Ordered(501).VisibleWhen(this->display_ == 0 || this->tint_text_);
        case soon_top: return module_option::Color("Approaching top", "Top of the bar as readiness approaches.", &this->soon_[0]).Ordered(510, true).VisibleWhen(this->display_ == 0 && this->color_mode_ != 2);
        case soon_bottom: return module_option::Color("Approaching bottom", "Bottom of the bar and text tint as readiness approaches.", &this->soon_[1]).Ordered(511).VisibleWhen((this->display_ == 0 || this->tint_text_) && this->color_mode_ != 2);
        case ready_top: return module_option::Color("Ready top", "Top of the bar as the timer becomes ready.", &this->ready_[0]).Ordered(520, true).VisibleWhen(this->display_ == 0 && this->color_mode_ != 2);
        case ready_bottom: return module_option::Color("Ready bottom", "Bottom of the bar and text tint as the timer becomes ready.", &this->ready_[1]).Ordered(521).VisibleWhen((this->display_ == 0 || this->tint_text_) && this->color_mode_ != 2);
        case selected_text: return module_option::Element("Selected text", "Independent styled name, countdown and heading profiles.", &this->selected_text_, profiles, 3);
        default: return {};
        }
    }

    auto recast_module::Colors(const recast_entry& entry, const uint64_t now) const -> std::array<D3DCOLOR, 2>
    {
        if (this->color_mode_ == 2) return this->cooldown_;
        if (entry.ready) return this->ready_;
        const float remaining = entry.Remaining(now), nearly = this->near_seconds_, soon = std::max(nearly + 0.1f, this->soon_seconds_);
        if (this->color_mode_ == 1) return remaining <= nearly ? this->ready_ : remaining <= soon ? this->soon_ : this->cooldown_;
        const bool final = remaining <= nearly;
        const auto& first = final ? this->ready_ : this->soon_;
        const auto& last = final ? this->soon_ : this->cooldown_;
        const float ratio = final ? (nearly > 0 ? remaining / nearly : 0) : (remaining - nearly) / (soon - nearly);
        return {Blend(first[0], last[0], ratio), Blend(first[1], last[1], ratio)};
    }

    auto recast_module::Visible(const recast_entry& entry, const uint64_t now) const -> bool
    { return (entry.kind == recast_kind::spell ? this->spells_ : this->abilities_) && entry.Visible(now, this->ready_hold_); }

    auto recast_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        if (!this->state_.enabled || this->source_ == nullptr) return false;
        const auto now = ::GetTickCount64();
        const auto entries = this->source_->Entries();
        return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) { return this->Visible(entry, now); });
    }

    auto recast_module::DrawRow(const recast_entry& entry, const ui::rect& row, const uint64_t now, const module_render_context& context) const -> void
    {
        const auto& design = context.Design();
        const bool right = this->anchor_ == 1, bars = this->display_ == 0;
        const auto colors = this->Colors(entry, now);
        ui::rect bar = row;
        if (bars && this->icons_)
        {
            const float size = std::min(this->icon_size_, row.height), space = size + 6;
            const ui::rect icon{right ? row.x + row.width - size : row.x, row.y + (row.height - size) * 0.5f, size, size};
            const auto* cache = context.Icons();
            const auto uv = ui::RecastIconUV(entry.kind == recast_kind::spell, entry.id, this->source_ != nullptr && this->source_->Job() != 0 ? this->source_->Job() : 1);
            if (auto* atlas = cache != nullptr ? cache->RecastAtlas() : nullptr; atlas != nullptr && uv.width > 0)
                design.DrawTexture(atlas, icon, 0xFFFFFFFF, uv);
            else design.DrawIcon(icon, entry.kind == recast_kind::spell ? ui::icon_glyph::castbar : ui::icon_glyph::castcost, colors[1]);
            bar.width = std::max(1.0f, bar.width - space);
            if (!right) bar.x += space;
        }
        if (bars)
        {
            const float progress = entry.Progress(now), ratio = this->fill_direction_ == 0 ? progress : 1 - progress;
            const float phase = std::fmod(static_cast<float>(now % 1000000) * 0.001f * this->meter_.sheen_speed, 1.0f);
            context.DrawBar(bar, ratio, colors[0], colors[1], phase, this->meter_, right);
        }
        auto nameText = this->tint_text_ ? TintTextProfile(this->text_[0], colors[1]) : this->text_[0];
        auto timeText = this->tint_text_ ? TintTextProfile(this->text_[1], colors[1]) : this->text_[1];
        if (right && nameText.placement == 0) nameText.style.alignment = ui::text_alignment::right;
        if (right && timeText.placement == 0) timeText.style.alignment = ui::text_alignment::left;
        timeText.style.tabular_numbers = true;
        const auto content = bars ? context.MeterTextBounds(bar, this->meter_) : bar;
        const float timeWidth = this->timer_ ? std::min(content.width * 0.45f,
            std::max(design.MeasureText("0:00:00", timeText.style), design.MeasureText("Ready", timeText.style)) + 6) : 0;
        const ui::rect timeBounds{right ? content.x : content.x + content.width - timeWidth, row.y, timeWidth, row.height};
        const ui::rect nameBounds{content.x + (right && this->timer_ ? timeWidth + 4 : 0), row.y,
            std::max(0.0f, content.width - timeWidth - (this->timer_ ? 4 : 0)), row.height};
        auto name = entry.name;
        if (this->charges_ && entry.maximum_charges != 0)
            name += " (" + std::to_string(entry.charges) + "/" + std::to_string(entry.maximum_charges) + ")";
        name = FitName(design, std::move(name), nameBounds.width, nameText.style);
        context.DrawText(name.c_str(), nameBounds, nameBounds, nameText);
        if (this->timer_)
        {
            const auto timer = entry.ready ? std::string{"Ready"} : FormatRecastTime(entry.Remaining(now), this->tenths_);
            context.DrawText(timer.c_str(), timeBounds, timeBounds, timeText);
        }
    }

    auto recast_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const auto now = ::GetTickCount64();
        std::vector<const recast_entry*> rows;
        if (this->source_ != nullptr)
            for (const auto& entry : this->source_->Entries()) if (this->Visible(entry, now)) rows.push_back(&entry);
        std::array<recast_entry, 4> preview{};
        if (rows.empty() && context.IsEditing())
        {
            constexpr const char* names[]{"Berserk", "Provoke", "Cure IV", "Haste"};
            constexpr uint32_t ids[]{1, 5, 4, 57};
            constexpr float remaining[]{38, 12, 2, 0};
            for (size_t i = 0; i < preview.size(); ++i)
            {
                auto& entry = preview[i];
                entry.kind = i < 2 ? recast_kind::ability : recast_kind::spell; entry.id = ids[i]; entry.name = names[i];
                entry.duration = i == 0 ? 180.0f : 30.0f; entry.expires = now + static_cast<uint64_t>(remaining[i] * 1000);
                entry.ready = i == 3; entry.ready_at = entry.started = now;
                if (this->Visible(entry, now)) rows.push_back(&entry);
            }
        }
        std::stable_sort(rows.begin(), rows.end(), [&](const auto* first, const auto* second) {
            if (this->sorting_ == 2) return first->name < second->name;
            if (this->sorting_ == 1 && first->started != second->started) return first->started > second->started;
            const float a = first->Remaining(now), b = second->Remaining(now);
            return a != b ? a < b : first->name < second->name;
        });
        const size_t count = std::min(rows.size(), static_cast<size_t>(this->maximum_rows_));
        const bool bars = this->display_ == 0;
        const float rowHeight = std::max({this->row_height_, context.Design().TextHeight(this->text_[0].style), context.Design().TextHeight(this->text_[1].style),
            bars && this->icons_ ? this->icon_size_ : 0, bars ? this->meter_.height + 2 : 0});
        const float headingHeight = this->heading_ ? std::max(20.0f, context.Design().TextHeight(this->text_[2].style)) + 4 : 0;
        const ui::container_style container{};
        const float padding = this->background_ ? container.edge_size + container.padding : 0;
        const float height = headingHeight + (count > 0 ? count * rowHeight + (count - 1) * this->spacing_ : rowHeight) + padding * 2;
        this->state_.bounds.height = this->state_.minimum_height = height;
        const auto& bounds = this->state_.bounds;
        const auto content = this->background_ ? context.DrawContainer(bounds) : bounds;
        if (this->heading_) context.DrawText("Recast", {content.x, content.y, content.width, headingHeight - 4}, content, this->text_[2]);
        for (size_t i = 0; i < count; ++i)
            this->DrawRow(*rows[i], {content.x, content.y + headingHeight + i * (rowHeight + this->spacing_), content.width, rowHeight}, now, context);
    }
}
