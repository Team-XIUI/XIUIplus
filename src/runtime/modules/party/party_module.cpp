#include "ui/text_fit.hpp"
#include "ui/game_icon_cache.hpp"
#include "party_module.hpp"
#include "ui/health_colors.hpp"
#include "modules/bar_config.hpp"
#include "ui/xiv1_resource_layout.hpp"
#include "ui/tp_meter.hpp"
#include "modules/treasure/treasure_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr const char* bar_names[]{"hp", "mp", "tp", "cast"};
    constexpr const char* text_names[]{"name", "jobs", "labels", "values", "details", "cast", "heading", "buffs", "debuffs", "tp", "mp", "zone"};
    constexpr const char* ids[]{"party", "alliance1", "alliance2"};
    constexpr const char* names[]{"Party", "Alliance 1", "Alliance 2"};
    constexpr size_t preview_remote_row = 5;
    constexpr uint8_t preview_jobs[]{2, 3, 5, 7, 10, 12};

    auto PartyPreset(const WORD id) -> std::string_view
    {
        const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
        const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
        const auto loaded = resource != nullptr ? ::LoadResource(module, resource) : nullptr;
        const auto* bytes = loaded != nullptr ? static_cast<const char*>(::LockResource(loaded)) : nullptr;
        return bytes != nullptr ? std::string_view{bytes, ::SizeofResource(module, resource)} : std::string_view{};
    }

    auto JobText(const xiui::party_member& member, const bool mainLevel, const bool subJob, const bool subLevel, const bool masterLevel) -> std::string
    {
        std::string text = xiui::PartyJobName(member.main_job);
        if (text.empty()) return {};
        if (mainLevel && member.main_level != 0) text += std::to_string(member.main_level);
        if (subJob && member.sub_job != 0)
        {
            text += "/"; text += xiui::PartyJobName(member.sub_job);
            if (subLevel && member.sub_level != 0) text += std::to_string(member.sub_level);
        }
        if (masterLevel && member.master_level != 0) text += " ML" + std::to_string(member.master_level);
        return text;
    }
}

namespace xiui
{
    auto party_module::JobLines(const party_member& member) const -> std::array<std::string,2>
    {
        if (!this->stack_jobs_)
            return {JobText(member,this->main_level_,this->sub_job_,this->sub_level_,this->master_level_),{}};
        std::array<std::string,2> lines{JobText(member,this->main_level_,false,false,this->master_level_),{}};
        if (!lines[0].empty() && this->sub_job_ && member.sub_job != 0)
        {
            lines[1] = PartyJobName(member.sub_job);
            if (this->sub_level_ && member.sub_level != 0) lines[1] += std::to_string(member.sub_level);
        }
        return lines;
    }

    party_module::party_module(const size_t group) : group_(std::min<size_t>(2, group))
    {
        this->state_.bounds.x = 900.0f - static_cast<float>(this->group_) * 430;
        this->container_.padding = 8;
        for (auto& row : this->split_statuses_) { row.size = 12; row.columns = 8; row.timers = false; }
        this->ApplyMeterDefaults(ui::theme::CreateDefault().meter);
        this->ApplyTextDefaults(ui::theme::CreateDefault());
    }

    auto party_module::Id(void) const -> const char* { return ids[this->group_]; }
    auto party_module::DisplayName(void) const -> const char* { return names[this->group_]; }
    auto party_module::Section(const char* prefix, const char* suffix) const -> std::string
    {
        return std::string{prefix} + this->Id() + (suffix != nullptr ? std::string{"."} + suffix : "");
    }

    auto party_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->meters_.fill(style);
        for (auto& meter : this->meters_) meter.height = 14;
    }

    auto party_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {{{theme.title_text}, {theme.caption_text}, {theme.meter_text}, {theme.meter_text}, {theme.caption_text}, {theme.meter_text}, {theme.title_text}, {theme.caption_text}, {theme.caption_text}, {theme.meter_text}, {theme.meter_text}, {theme.caption_text}}};
        this->text_[0].style.scale = 0.88f;
        this->text_[heading_text].style.scale = 0.88f;
        for (size_t i = 1; i < heading_text; ++i) this->text_[i].style.scale = 0.75f;
        this->text_[7].style.scale = this->text_[8].style.scale = 0.75f;
        this->text_[3].style.alignment = ui::text_alignment::right;
        this->text_[4].style.alignment = ui::text_alignment::right;
        this->text_[tp_text] = this->text_[3];
        this->text_[mp_text] = this->text_[3];
        this->text_[1].style.alignment = ui::text_alignment::left;
        this->text_[zone_text].style.scale = 0.75f;
        this->text_[zone_text].style.alignment = ui::text_alignment::center;
        this->tickers_.ApplyTextDefaults(theme);
    }

    auto party_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        const auto presetAlliances = this->preset_alliances_;
        const auto* timing = this->timing_;
        const auto* mpPreviewSource = this->mp_preview_source_;
        auto* healPrediction = this->heal_prediction_;
        const auto* moduleEnabled = this->module_enabled_;
        *this = party_module{this->group_};
        this->state_.bounds = bounds;
        this->source_ = source; this->preset_alliances_ = presetAlliances;
        this->timing_ = timing;
        this->mp_preview_source_ = mpPreviewSource; this->heal_prediction_ = healPrediction;
        this->module_enabled_ = moduleEnabled;
        this->ApplyMeterDefaults(theme.meter);
        this->ApplyTextDefaults(theme);
    }

    auto party_module::CopyPreferences(const party_module& party) -> void
    {
        const auto group = this->group_;
        const auto* source = this->source_;
        const auto presetAlliances = this->preset_alliances_;
        const auto* timing = this->timing_;
        const auto* mpPreviewSource = this->mp_preview_source_;
        auto* healPrediction = this->heal_prediction_;
        const auto state = this->state_;
        const bool hidePreview = this->hide_preview_;
        const auto* moduleEnabled = this->module_enabled_;
        *this = party;
        this->group_ = group;
        this->source_ = source; this->preset_alliances_ = presetAlliances;
        this->timing_ = timing;
        this->mp_preview_source_ = mpPreviewSource; this->heal_prediction_ = healPrediction;
        this->state_.enabled = state.enabled;
        this->state_.scale = state.scale; this->state_.scale_checkpoint = state.scale_checkpoint;
        this->state_.bounds.x = state.bounds.x;
        this->state_.bounds.y = state.bounds.y;
        this->state_.content_bounds = {};
        this->hide_preview_ = hidePreview;
        this->module_enabled_ = moduleEnabled;
        this->animations_ = {};
        this->EndControllerBrowse();
    }

    auto party_module::Load(const settings_store& store) -> void
    {
        this->EndControllerBrowse();
        const auto* section = this->Id();
        this->mp_cost_preview_ = store.ReadBool(section, "mp_cost_preview", true);
        this->heal_preview_.Load(store, section);
        this->tickers_.Load(store, section);
        this->status_order_ = std::clamp(store.ReadInt(section, "status_order", 0), 0, 3);
        this->selected_status_group_ = std::clamp(store.ReadInt(section, "selected_status_group", 0), 0, 1);
        this->split_status_gap_ = std::clamp(store.ReadFloat(section, "split_status_gap", 4), 0.0f, 48.0f);
        this->status_timers_ = store.ReadBool(section, "status_timers", false);
        this->status_tooltips_ = store.ReadBool(section, "status_tooltips", true);
        this->status_theme_ = store.ReadInt(section, "status_icon_theme", 0);
        this->status_preview_ = store.ReadBool(section, "status_preview", false);
        this->split_statuses_[0].Load(store, this->Section("status.", "buffs").c_str(), 10);
        this->split_statuses_[1].Load(store, this->Section("status.", "debuffs").c_str(), 10);
        if (this->group_ == 0 && store.ReadInt(section, "status_icon_version", 0) < 1)
            for (auto& row : this->split_statuses_) if (row.size == 10) row.size = 12;
        this->state_.enabled = store.ReadBool(section, "enabled", true);
        this->hide_preview_ = store.ReadBool(section, "hide_preview", false);
        this->state_.bounds = store.ReadRect(this->Section("layout.").c_str(), this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, this->state_.minimum_width, this->state_.maximum_width);
        const std::array<std::pair<const char*, bool*>, 19> bools{{
            std::pair{"solo", &this->solo_}, {"preview", &this->preview_}, {"title", &this->title_},
            {"distance", &this->distance_}, {"job_icons", &this->job_icons_}, {"jobs", &this->jobs_}, {"main_level", &this->main_level_},
            {"sub_job", &this->sub_job_}, {"sub_level", &this->sub_level_}, {"master_level", &this->master_level_}, {"show_tp", &this->tp_},
            {"flash_tp", &this->flash_tp_}, {"always_mp", &this->always_mp_}, {"buffs", &this->buffs_}, {"casts", &this->casts_},
            {"selection", &this->selection_}, {"bottom", &this->bottom_}, {"expand", &this->expand_}, {"expand_alliance", &this->expand_alliance_}}};
        for (const auto& [key, value] : bools) *value = store.ReadBool(section, key, *value);
        this->tp_ = store.ReadBool(section, "show_tp", store.ReadBool(section, "tp", this->tp_));
        this->fit_width_ = store.ReadBool(section, "fit_width", true);
        this->icon_follow_name_ = store.ReadBool(section, "icon_follow_name", false);
        this->show_sync_ = store.ReadBool(section, "show_sync", true);
        this->show_leader_ = store.ReadBool(section, "show_leader", true);
        this->layout_ = std::clamp(store.ReadInt(section, "layout", bars_horizontal), bars_horizontal, bars_classic);
        this->cast_placement_ = std::clamp(store.ReadInt(section, "cast_placement", cast_separate), cast_separate, cast_tp);
        this->job_placement_ = std::clamp(store.ReadInt(section, "job_placement", jobs_name), jobs_name, jobs_below);
        this->job_anchor_ = std::clamp(store.ReadInt(section, "job_anchor", job_anchor_name), job_anchor_name, job_anchor_hp);
        this->stack_jobs_ = store.ReadBool(section, "stack_jobs", false);
        this->tp_display_ = std::clamp(store.ReadInt(section, "tp_display", 0), 0, 1);
        this->labels_ = store.ReadBool(section, "labels", false);
        this->hp_display_ = std::clamp(store.ReadInt(section, "hp_display", 2), 0, 5);
        this->mp_display_ = std::clamp(store.ReadInt(section, "mp_display", 0), 0, 5);
        this->selected_bar_ = std::clamp(store.ReadInt(section, "selected_bar", 0), 0, 3);
        this->selected_text_ = std::clamp(store.ReadInt(section, "selected_text", 0), 0, static_cast<int>(this->text_.size()) - 1);
        this->style_mode_ = std::clamp(store.ReadInt(section, "style_mode", 0), 0, 1);
        this->spacing_ = std::clamp(store.ReadFloat(section, "spacing", 8), 0.0f, 32.0f);
        this->minimum_rows_ = std::clamp(store.ReadFloat(section, "minimum_rows", 1), 1.0f, 6.0f);
        this->icon_size_ = std::clamp(store.ReadFloat(section, "icon_size", 32), 20.0f, 64.0f);
        this->icon_offset_x_ = std::clamp(store.ReadFloat(section, "icon_offset_x", 0), -128.0f, 128.0f);
        this->icon_offset_y_ = std::clamp(store.ReadFloat(section, "icon_offset_y", 0), -128.0f, 128.0f);
        this->buff_size_ = std::clamp(store.ReadFloat(section, "buff_size", 18), 12.0f, 32.0f);
        this->buff_limit_ = std::clamp(store.ReadFloat(section, "buff_limit", 12), 1.0f, 32.0f);
        this->distance_highlight_ = std::clamp(store.ReadFloat(section, "distance_highlight", 20.8f), 0.0f, 50.0f);
        this->container_ = {.opacity = std::clamp(store.ReadFloat(section, "opacity", 1), 0.0f, 1.0f),
            .padding = std::clamp(store.ReadFloat(section, "padding", 8), 4.0f, 24.0f)};
        this->selection_style_.target_color = store.ReadArgb(section, "selection_color", this->selection_style_.target_color);
        this->selection_style_.subtarget_color = store.ReadArgb(section, "subtarget_color", this->selection_style_.subtarget_color);
        this->selection_style_.background_opacity = std::clamp(store.ReadFloat(section, "selection_background_opacity", 0.12f), 0.0f, 1.0f);
        this->selection_style_.padding_x = std::clamp(store.ReadFloat(section, "selection_padding_x", 6), 0.0f, 16.0f);
        this->selection_style_.padding_y = std::clamp(store.ReadFloat(section, "selection_padding_y", 4), 0.0f, 12.0f);
        this->selection_style_.arrow = store.ReadBool(section, "selection_arrow", true);
        this->selection_width_extension_ = std::clamp(store.ReadFloat(section, "selection_width_extension", 0), -128.0f, 256.0f);
        this->selection_height_extension_ = std::clamp(store.ReadFloat(section, "selection_height_extension", 0), -64.0f, 128.0f);
        this->status_placement_ = std::clamp(store.ReadInt(section, "status_placement", status_left), status_left, status_inline);
        this->status_frame_ = store.ReadBool(section, "status_container", false);
        this->status_columns_ = std::clamp(store.ReadFloat(section, "status_columns", 8), 1.0f, 16.0f);
        this->status_spacing_ = std::clamp(store.ReadFloat(section, "status_spacing", 2), 0.0f, 12.0f);
        this->status_row_spacing_ = std::clamp(store.ReadFloat(section, "status_row_spacing", this->status_spacing_), 0.0f, 32.0f);
        this->status_gap_ = std::clamp(store.ReadFloat(section, "status_gap", 8), 0.0f, 48.0f);
        this->status_container_ = {.opacity = std::clamp(store.ReadFloat(section, "status_opacity", 1), 0.0f, 1.0f),
            .padding = std::clamp(store.ReadFloat(section, "status_padding", 4), 0.0f, 16.0f)};
        for (size_t i = 0; i < this->meters_.size(); ++i)
        {
            this->meters_[i] = LoadMeterStyle(store, this->Section("meter.", bar_names[i]).c_str(), this->meters_[i]);
            this->bar_widths_[i] = std::clamp(store.ReadFloat(this->Section("meter.", bar_names[i]).c_str(), "width", 0), 0.0f, this->BarWidthLimit());
            this->bar_positions_[i] = std::clamp(store.ReadInt(this->Section("meter.", bar_names[i]).c_str(), "position", position_layout), position_layout, position_right);
            this->bar_offsets_x_[i] = std::clamp(store.ReadFloat(this->Section("meter.", bar_names[i]).c_str(), "offset_x", 0), -128.0f, 128.0f);
            this->bar_offsets_y_[i] = std::clamp(store.ReadFloat(this->Section("meter.", bar_names[i]).c_str(), "offset_y", 0), -128.0f, 128.0f);
            this->fills_[i] = store.ReadArgb(section, bar_names[i], this->fills_[i]);
            this->fill_tops_[i] = store.ReadArgb(this->Section("meter.", bar_names[i]).c_str(), "fill_top", this->fill_tops_[i]);
        }
        for (size_t i = 0; i < this->text_.size(); ++i)
        {
            if (i == tp_text && store.ReadInt(section, "tp_text_version", 0) < 1)
            { this->text_[tp_text] = this->text_[3]; continue; }
            if (i == mp_text && store.ReadInt(section, "mp_text_version", 0) < 1)
            { this->text_[mp_text] = this->text_[3]; continue; }
            auto fallback = i == heading_text ? module_text_profile{this->text_[0].style} : this->text_[i];
            if (i == zone_text)
            {
                fallback = module_text_profile{this->text_[1].style};
                fallback.style.alignment = ui::text_alignment::center;
            }
            this->text_[i] = LoadTextProfile(store, this->Section("text.", text_names[i]).c_str(), fallback);
        }
        if (store.ReadInt(section, "name_cast_anchor_version", 0) < 1 && this->layout_ == bars_horizontal &&
            this->cast_placement_ == cast_name && this->text_[0].offset_x == 4 && this->text_[0].offset_y == 8 &&
            this->bar_offsets_x_[3] == 4 && this->bar_offsets_y_[3] == 8)
            this->bar_offsets_x_[3] = this->bar_offsets_y_[3] = 0;
        if (store.ReadInt(section, "classic_layout_version", 0) < 2 && this->layout_ == bars_mp_below_tp_left && this->tp_display_ == 1 &&
            this->fit_width_ && this->icon_follow_name_ && this->icon_size_ == 20 && this->icon_offset_x_ == 9 &&
            this->text_[0].offset_x == 44 && this->text_[tp_text].offset_x == 37 &&
            this->bar_widths_[0] == 152 && this->bar_widths_[1] == 92 && this->bar_widths_[2] == 28 &&
            this->bar_positions_[0] == position_layout && this->bar_positions_[1] == position_right && this->bar_positions_[2] == position_layout &&
            this->bar_offsets_x_[0] == 0 && this->bar_offsets_x_[1] == 0 && this->bar_offsets_x_[2] == 0 &&
            this->bar_offsets_y_[0] == -8 && this->bar_offsets_y_[1] == -18 && this->bar_offsets_y_[2] == -8)
        {
            this->layout_ = bars_classic;
            this->text_[0].offset_x = 27; this->icon_offset_x_ = 0;
            this->text_[tp_text].offset_x = 0; this->bar_offsets_y_[2] = -18;
        }
        this->low_hp_ = store.ReadArgb(section, "low_hp", this->low_hp_);
        this->range_color_ = store.ReadArgb(section, "range_color", this->range_color_);
        this->ready_tp_ = store.ReadArgb(section, "ready_tp", this->ready_tp_);
        this->animations_ = {};
    }

    auto party_module::Save(const settings_store& store) const -> void
    {
        const auto* section = this->Id();
        this->tickers_.Save(store, section);
        store.WriteBool(section, "mp_cost_preview", this->mp_cost_preview_);
        this->heal_preview_.Save(store, section);
        store.WriteInt(section, "status_order", this->status_order_); store.WriteInt(section, "selected_status_group", this->selected_status_group_);
        store.WriteFloat(section, "split_status_gap", this->split_status_gap_);
        store.WriteBool(section, "status_timers", this->status_timers_); store.WriteBool(section, "status_tooltips", this->status_tooltips_);
        store.WriteInt(section, "status_icon_theme", this->status_theme_);
        store.WriteBool(section, "status_preview", this->status_preview_);
        store.WriteInt(section, "status_icon_version", 1);
        store.WriteInt(section, "tp_text_version", 1);
        store.WriteInt(section, "mp_text_version", 1);
        store.WriteInt(section, "name_cast_anchor_version", 1);
        store.WriteInt(section, "classic_layout_version", 2);
        this->split_statuses_[0].Save(store, this->Section("status.", "buffs").c_str());
        this->split_statuses_[1].Save(store, this->Section("status.", "debuffs").c_str());
        store.WriteBool(section, "enabled", this->state_.enabled);
        store.WriteBool(section, "hide_preview", this->hide_preview_);
        store.WriteRect(this->Section("layout.").c_str(), this->state_.bounds);
        const std::array<std::pair<const char*, bool>, 19> bools{{
            std::pair{"solo", this->solo_}, {"preview", this->preview_}, {"title", this->title_},
            {"distance", this->distance_}, {"job_icons", this->job_icons_}, {"jobs", this->jobs_}, {"main_level", this->main_level_},
            {"sub_job", this->sub_job_}, {"sub_level", this->sub_level_}, {"master_level", this->master_level_}, {"show_tp", this->tp_},
            {"flash_tp", this->flash_tp_}, {"always_mp", this->always_mp_}, {"buffs", this->buffs_}, {"casts", this->casts_},
            {"selection", this->selection_}, {"bottom", this->bottom_}, {"expand", this->expand_}, {"expand_alliance", this->expand_alliance_}}};
        for (const auto& [key, value] : bools) store.WriteBool(section, key, value);
        store.WriteBool(section, "fit_width", this->fit_width_);
        store.WriteBool(section, "icon_follow_name", this->icon_follow_name_);
        store.WriteBool(section, "show_sync", this->show_sync_);
        store.WriteBool(section, "show_leader", this->show_leader_);
        store.WriteInt(section, "layout", this->layout_); store.WriteInt(section, "hp_display", this->hp_display_);
        store.WriteInt(section, "cast_placement", this->cast_placement_);
        store.WriteInt(section, "job_placement", this->job_placement_);
        store.WriteInt(section, "job_anchor", this->job_anchor_);
        store.WriteBool(section, "stack_jobs", this->stack_jobs_);
        store.WriteInt(section, "tp_display", this->tp_display_);
        store.WriteBool(section, "labels", this->labels_);
        store.WriteInt(section, "mp_display", this->mp_display_); store.WriteInt(section, "selected_bar", this->selected_bar_);
        store.WriteInt(section, "selected_text", this->selected_text_); store.WriteInt(section, "style_mode", this->style_mode_);
        store.WriteFloat(section, "spacing", this->spacing_); store.WriteFloat(section, "minimum_rows", this->minimum_rows_);
        store.WriteFloat(section, "icon_size", this->icon_size_); store.WriteFloat(section, "buff_size", this->buff_size_);
        store.WriteFloat(section, "icon_offset_x", this->icon_offset_x_); store.WriteFloat(section, "icon_offset_y", this->icon_offset_y_);
        store.WriteFloat(section, "buff_limit", this->buff_limit_); store.WriteFloat(section, "distance_highlight", this->distance_highlight_);
        store.WriteFloat(section, "opacity", this->container_.opacity); store.WriteFloat(section, "padding", this->container_.padding);
        store.WriteArgb(section, "selection_color", this->selection_style_.target_color);
        store.WriteArgb(section, "subtarget_color", this->selection_style_.subtarget_color);
        store.WriteFloat(section, "selection_background_opacity", this->selection_style_.background_opacity);
        store.WriteFloat(section, "selection_padding_x", this->selection_style_.padding_x);
        store.WriteFloat(section, "selection_padding_y", this->selection_style_.padding_y);
        store.WriteBool(section, "selection_arrow", this->selection_style_.arrow);
        store.WriteFloat(section, "selection_width_extension", this->selection_width_extension_);
        store.WriteFloat(section, "selection_height_extension", this->selection_height_extension_);
        store.WriteInt(section, "status_placement", this->status_placement_);
        store.WriteBool(section, "status_container", this->status_frame_);
        store.WriteFloat(section, "status_columns", this->status_columns_);
        store.WriteFloat(section, "status_spacing", this->status_spacing_);
        store.WriteFloat(section, "status_row_spacing", this->status_row_spacing_);
        store.WriteFloat(section, "status_gap", this->status_gap_);
        store.WriteFloat(section, "status_opacity", this->status_container_.opacity);
        store.WriteFloat(section, "status_padding", this->status_container_.padding);
        for (size_t i = 0; i < this->meters_.size(); ++i)
        {
            SaveMeterStyle(store, this->Section("meter.", bar_names[i]).c_str(), this->meters_[i]);
            store.WriteFloat(this->Section("meter.", bar_names[i]).c_str(), "width", this->bar_widths_[i]);
            store.WriteInt(this->Section("meter.", bar_names[i]).c_str(), "position", this->bar_positions_[i]);
            store.WriteFloat(this->Section("meter.", bar_names[i]).c_str(), "offset_x", this->bar_offsets_x_[i]);
            store.WriteFloat(this->Section("meter.", bar_names[i]).c_str(), "offset_y", this->bar_offsets_y_[i]);
            store.WriteArgb(section, bar_names[i], this->fills_[i]);
            store.WriteArgb(this->Section("meter.", bar_names[i]).c_str(), "fill_top", this->fill_tops_[i]);
        }
        for (size_t i = 0; i < this->text_.size(); ++i)
            SaveTextProfile(store, this->Section("text.", text_names[i]).c_str(), this->text_[i]);
        store.WriteArgb(section, "low_hp", this->low_hp_); store.WriteArgb(section, "range_color", this->range_color_);
        store.WriteArgb(section, "ready_tp", this->ready_tp_);
    }

    auto party_module::ContentOption(const size_t index) -> module_option
    {
        if (index >= selection_size_options_start && index < selection_size_options_start + 2)
        {
            const bool width = index == selection_size_options_start;
            return module_option::Slider(width ? "Width adjustment" : "Height adjustment",
                width ? "Add pixels to the selection box's total width, centered on the entry. Extend it to the window edges without changing content width." :
                    "Add pixels to the selection box's total height, centered on the entry. Entry spacing stays the same.",
                width ? &this->selection_width_extension_ : &this->selection_height_extension_, width ? -128.0f : -64.0f, width ? 256.0f : 128.0f, 1,
                module_option_group::appearance).Ordered(width ? 410 : 411, width).VisibleWhen(this->selection_);
        }
        if (index == leader_option)
            return module_option::Toggle("Show party leader", "Show a yellow dot beside the party or alliance leader's name.", &this->show_leader_).Ordered(10);
        if (index >= job_text_options_start && index < job_text_options_start + 2)
        {
            static constexpr const char* anchors[]{"Name", "HP bar start"};
            auto option = index == job_text_options_start ?
                module_option::Toggle("Stack job and subjob", "Put the main job above the subjob, using the Jobs text style and offsets.", &this->stack_jobs_).Ordered(-25) :
                module_option::Choice("Anchor", "Follow the displayed name or the start of the HP fill. The text offsets below apply to either anchor.", &this->job_anchor_, anchors, 2).Ordered(-24, true);
            return option.VisibleWhen(this->jobs_);
        }
        if (index >= preset_options_start && index < preset_options_start + 2)
        {
            static constexpr const char* names[]{"Classic", "XIV"};
            static constexpr const char* descriptions[]{
                "HP above MP.\nSmall TP text.",
                "Captured horizontal bars.\nCustom job text and icon layout."};
            const auto preset = index - preset_options_start;
            auto option = module_option::Action(names[preset], descriptions[preset], "Apply");
            option.card = true; option.display_order = 2000 + static_cast<int>(preset);
            option.confirmation = "This will reset your custom Party and Alliance settings to this preset. Each list keeps its position.";
            option.available = this->group_ == 0;
            return option;
        }

        if (index == status_theme_option)
        {
            auto option = module_option::Choice("Status icon theme", "Artwork for the combined status strip. Split rows have independent theme choices. Missing effects use native artwork.",
                &this->status_theme_, ui::status_icon_overrides, std::size(ui::status_icon_overrides)).Ordered(440);
            option.available = this->group_ == 0;
            return option.VisibleWhen(this->buffs_);
        }
        if (index >= heal_options_start) return this->heal_preview_.Option(index - heal_options_start, this->heal_prediction_);
        if (index == mp_preview_option)
            return module_option::Toggle("Spell MP cost preview", "Highlight the selected spell's base MP cost on your own Party MP bar. Other members are unaffected. Works with Cast Cost hidden.", &this->mp_cost_preview_).Ordered(80);
        if (index >= icon_offset_options_start && index < icon_offset_options_start + 2)
        {
            const bool x = index == icon_offset_options_start;
            auto option = module_option::Slider(x ? "Job icon X offset" : "Job icon Y offset",
                "Move the icon from its fixed or name-following position without changing entry height.",
                x ? &this->icon_offset_x_ : &this->icon_offset_y_, -128, 128, 1).Ordered(x ? 210 : 211, x);
            return option.VisibleWhen(this->job_icons_);
        }
        if (index == status_preview_option)
        {
            auto option = module_option::Toggle("Preview status effects", "Show sample buffs and debuffs in edit mode. Enable Status timers to preview their countdowns. Works with Full party preview off.", &this->status_preview_).Ordered(-20);
            option.available = this->group_ == 0;
            return option.VisibleWhen(this->buffs_);
        }
        if (index >= status_options_start)
        {
            static constexpr const char* orders[]{"Native order", "Show debuffs first", "Show buffs first", "Split buffs and debuffs"};
            static constexpr const char* groups[]{"Buff row", "Debuff row"};
            const auto local = index - status_options_start;
            module_option option{};
            switch (local)
            {
            case 0: option = module_option::Choice("Status order", "Retain source order within each group. Unknown and neutral effects stay visible with buffs.", &this->status_order_, orders, 4).Ordered(-30); break;
            case 1: option = module_option::Element("Selected status row", "Customize the separate buff or debuff row.", &this->selected_status_group_, groups, 2); break;
            case 2: option = module_option::Slider("Split row gap", "Space between the buff and debuff rows.", &this->split_status_gap_, 0, 48, 1).Ordered(303); break;
            case 3: option = module_option::Toggle("Status timers", "Show actual local-player countdowns inside status icons. Other members have no native durations.", &this->status_timers_).Ordered(850); break;
            case 4: option = module_option::Toggle("Status tooltips", "Show native effect names and descriptions on hover or controller inspection.", &this->status_tooltips_).Ordered(870); break;
            case 5 + ui::status_strip_settings::option_count: option = module_option::Slider("Status row spacing", "Vertical gap between wrapped status rows, independent of icon spacing.", &this->status_row_spacing_, 0, 32, 1).Ordered(301); break;
            default: option = this->split_statuses_[this->selected_status_group_].Option(local - 5, 10); break;
            }
            if (local == 5) { option.label = "Show selected row"; option.description = "Show the selected buff or debuff row. Turn off Buff row to show only debuffs, or Debuff row to show only buffs."; }
            option.available = option.available && this->group_ == 0;
            return option.VisibleWhen(this->buffs_);
        }
        if (index >= ticker_options_start) return this->tickers_.Option(index - ticker_options_start);
        if (index == icon_follow_option)
        {
            auto option = module_option::Toggle("Icon follows name", "Keep the job icon beside the name as its position and alignment change.", &this->icon_follow_name_).Ordered(202);
            return option.VisibleWhen(this->job_icons_);
        }
        if (index == sync_option) return module_option::Toggle("Show SYNC", "Show the level-sync label beside synced party members.", &this->show_sync_);
        static constexpr const char* layouts[]{"Horizontal", "Compact vertical", "MP below HP, TP left", "Classic (HP above MP and TP)"};
        static constexpr const char* displays[]{"Number", "Percent", "Number (percent)", "Percent (number)", "Current / max", "Hidden"};
        static constexpr const char* bars[]{"HP", "MP", "TP", "Cast"};
        static constexpr const char* styles[]{"All bars", "Per bar"};
        static constexpr const char* texts[]{"Name", "Jobs", "Bar labels", "HP values", "Sync", "Cast", "Heading", "Buff timers and details", "Debuff timers and details", "TP values", "MP values", "Zone"};
        static constexpr const char* placements[]{"Left", "Right", "Inside row"};
        static constexpr const char* castPlacements[]{"Separate row", "Name row", "Replace MP", "Replace TP"};
        static constexpr const char* jobPlacements[]{"After name", "Below name"};
        static constexpr const char* tpDisplays[]{"Bar", "Number only"};
        static constexpr const char* barPositions[]{"Layout", "Left", "Center", "Right"};
        const auto statusOption = [&](module_option option, bool separate = false, bool frame = false) {
            option.available = option.available && this->group_ == 0;
            return option.VisibleWhen(this->buffs_ && (!separate || this->status_placement_ != status_inline) && (!frame || this->status_frame_));
        };
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show this party list.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Show when solo", "Keep your own row while solo.", &this->solo_);
        case 2: return {};
        case 3: return module_option::Toggle("Full party preview", "Show sample members and animated swing/resting ticker examples in edit mode.", &this->preview_).Ordered(-20);
        case 4: return module_option::Toggle("Show heading", "Show the party or alliance name and member count.", &this->title_);
        case 5: return module_option::Toggle("Distance", "Show known distance in yalms.", &this->distance_);
        case 6: return module_option::Toggle("Job icons", "Original XIVParty job images.", &this->job_icons_);
        case 7: return module_option::Toggle("Show job text", "Main job and optional subjob.", &this->jobs_).Ordered(-50);
        case 8: return module_option::Toggle("Main job level", "Include the main job level.", &this->main_level_).Ordered(-40, true).VisibleWhen(this->jobs_);
        case 9: return module_option::Toggle("Subjob", "Include the member's subjob.", &this->sub_job_).Ordered(-39).VisibleWhen(this->jobs_);
        case 10: return module_option::Toggle("Subjob level", "Include the subjob level.", &this->sub_level_).Ordered(-38, true).VisibleWhen(this->jobs_ && this->sub_job_);
        case 11: return module_option::Toggle("Master level", "Include ML when available.", &this->master_level_).Ordered(-37).VisibleWhen(this->jobs_);
        case 12: return module_option::Toggle("TP", "Show tactical points.", &this->tp_);
        case 13: return module_option::Toggle("Flash ready TP", "Pulse TP text at 1,000 or higher.", &this->flash_tp_).VisibleWhen(this->tp_);
        case 14: return module_option::Toggle("Always show MP", "Also show an empty MP bar for jobs without MP.", &this->always_mp_);
        case 15: return module_option::Toggle("Status icons", "Native status icons. The game shares these within your party.", &this->buffs_).Ordered(-10);
        case 16: return module_option::Toggle("Cast bars", "Party and alliance spell progress from action packets.", &this->casts_);
        case 17: return module_option::Toggle("Selection", "Highlight targeted members and subtargets.", &this->selection_).Ordered(-10);
        case 18: return module_option::Toggle("Align bottom", "Keep the bottom edge fixed as members join or leave.", &this->bottom_).Ordered(355);
        case 19: return module_option::Toggle("Expand height", "Reserve all six member rows.", &this->expand_).Ordered(352);
        case 20: return module_option::Toggle("Expand in alliance", "Reserve six rows while in an alliance.", &this->expand_alliance_).Ordered(353);
        case 21: return module_option::Choice("Resource layout", "Arrange HP, MP and TP. Classic puts HP across the top, with TP below left and MP below right.", &this->layout_, layouts, std::size(layouts), module_option_group::appearance).Ordered(-30);
        case 22: return module_option::Choice("HP display", "Other members' maximum HP is estimated (~) from percent.", &this->hp_display_, displays, 6);
        case 23: return module_option::Choice("MP display", "Other members' maximum MP is estimated (~) from percent.", &this->mp_display_, displays, 6);
        case 24: return module_option::EditScope("Bar settings", "Choose which bars to edit. Selecting All bars preserves values; edits apply only the changed setting to every bar.", &this->style_mode_, styles, 2);
        case 25: return module_option::Element("Selected bar", "Choose the resource or cast bar to edit.", &this->selected_bar_, bars, 4);
        case 26: return module_option::Element("Selected text", "Bar labels are HP/MP/TP captions; values are the numbers. Choose the element to edit.", &this->selected_text_, texts, std::size(texts));
        case 27:
        {
            auto option = ModuleWidthOption(this->state_);
            option.label = this->fit_width_ ? "Maximum window width" : "Window width";
            if (this->fit_width_) option.description = "Limit the fitted container; bar widths determine its actual size.";
            return option.Ordered(350);
        }
        case 28: return module_option::Slider("Entry spacing", "Space between party members.", &this->spacing_, 0, 32, 1, module_option_group::appearance).Ordered(300, true);
        case 29: return module_option::Slider("Minimum rows", "Minimum reserved rows, even in a smaller party.", &this->minimum_rows_, 1, 6, 1, module_option_group::appearance).Ordered(354);
        case 30: return module_option::Slider("Job icon size", "Size in pixels.", &this->icon_size_, 20, 64, 1, module_option_group::appearance).Ordered(110).VisibleWhen(this->job_icons_);
        case 31: return module_option::Slider("Status icon size", "Size in pixels.", &this->buff_size_, 12, 32, 1, module_option_group::appearance).Ordered(110).VisibleWhen(this->buffs_);
        case 32: return module_option::Slider("Status limit", "Maximum icons. Any remainder has a +count label.", &this->buff_limit_, 1, 32, 1, module_option_group::appearance).Ordered(121).VisibleWhen(this->buffs_);
        case 33: return module_option::Slider("Distance highlight", "Tint nearby distance text; zero disables it.", &this->distance_highlight_, 0, 50, 0.5f, module_option_group::appearance).VisibleWhen(this->distance_);
        case 34: return module_option::Slider("Opacity", "Native window opacity.", &this->container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450);
        case 35: return module_option::Slider("Padding", "Space inside the party window.", &this->container_.padding, 4, 24, 1, module_option_group::appearance).Ordered(402);
        case 36: return module_option::Color("Ready TP text", "Value tint at 1,000 TP or higher.", &this->ready_tp_, module_option_group::colors).VisibleWhen(this->tp_);
        case 37: return module_option::Color("Low HP name", "Name tint below 25% health.", &this->low_hp_, module_option_group::colors);
        case 38: return module_option::Color("Nearby distance", "Distance tint inside the chosen range.", &this->range_color_, module_option_group::colors).VisibleWhen(this->distance_ && this->distance_highlight_ > 0);
        case 39: return module_option::Color("Fill top", "Upper gradient for the selected bar.", &this->fill_tops_[this->selected_bar_], module_option_group::colors).Ordered(500, true);
        case 40: return module_option::Color("Fill bottom", "Lower gradient for the selected bar.", &this->fills_[this->selected_bar_], module_option_group::colors).Ordered(501);
        case 41: return statusOption(module_option::Choice("Status placement", "Status effects beside each member or inside the row.", &this->status_placement_, placements, 3).Ordered(200));
        case 42: return statusOption(module_option::Toggle("Status container", "Give each member's effects their own themed window.", &this->status_frame_).Ordered(-10), true);
        case 43: return statusOption(module_option::Slider("Statuses per row", "Wrap effects after this many icons.", &this->status_columns_, 1, 16, 1, module_option_group::appearance).Ordered(120, true));
        case 44: return statusOption(module_option::Slider("Status icon spacing", "Horizontal space between status icons.", &this->status_spacing_, 0, 12, 1, module_option_group::appearance).Ordered(300, true));
        case 45: return statusOption(module_option::Slider("Status gap", "Space between the party window and effects.", &this->status_gap_, 0, 48, 1, module_option_group::appearance).Ordered(302), true);
        case 46: return statusOption(module_option::Slider("Status container opacity", "Shared background and border opacity for the status containers.", &this->status_container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450), true, true);
        case 47: return statusOption(module_option::Slider("Status container padding", "Space inside each status container.", &this->status_container_.padding, 0, 16, 1, module_option_group::appearance).Ordered(402), true, true);
        case 48:
        {
            auto option = module_option::Choice("Cast placement", "Use a separate row or replace the name, MP or TP while casting.", &this->cast_placement_, castPlacements, 4);
            return option.VisibleWhen(this->casts_);
        }
        case 49:
        {
            auto option = module_option::Choice("Name placement", "Place job text after the name or in a row below it. Applies to the Name anchor.", &this->job_placement_, jobPlacements, 2).Ordered(-23);
            return option.VisibleWhen(this->jobs_ && this->job_anchor_ == job_anchor_name);
        }
        case 50: return module_option::Toggle("Bar labels", "Show HP, MP and TP above their values in the XI theme. XIV 1.0 includes its own resource labels.", &this->labels_, module_option_group::appearance);
        case 51:
        {
            auto option = module_option::Choice("TP display", "Show TP as a bar or just its number.", &this->tp_display_, tpDisplays, 2);
            return option.VisibleWhen(this->tp_);
        }
        case 52: return module_option::Slider("Bar width", "Width in pixels; 0 is automatic. Limited to the party content width.",
            &this->bar_widths_[this->selected_bar_], 0, this->BarWidthLimit(), 1, module_option_group::appearance).Ordered(100, true);
        case 53: return module_option::Choice("Bar position", "Keep the layout position or align within the resource row, then apply offsets.",
            &this->bar_positions_[this->selected_bar_], barPositions, std::size(barPositions), module_option_group::appearance).Ordered(200);
        case 54: return module_option::Slider("Bar X offset", "Move this bar and its text horizontally after positioning.",
            &this->bar_offsets_x_[this->selected_bar_], -128, 128, 1, module_option_group::appearance).Ordered(210, true);
        case 55: return module_option::Slider("Bar Y offset", "Move this bar and its text vertically after positioning.",
            &this->bar_offsets_y_[this->selected_bar_], -128, 128, 1, module_option_group::appearance).Ordered(211);
        case 56: return module_option::Toggle("Fit width to bars", "Size the native container around the bars. Width becomes its maximum.", &this->fit_width_, module_option_group::appearance).Ordered(351);
        case 57: return module_option::Color("Target color", "Border, background tint and arrow for the current target.", &this->selection_style_.target_color, module_option_group::colors).VisibleWhen(this->selection_);
        case 58: return module_option::Color("Subtarget color", "Border, background tint and arrow while choosing a subtarget.", &this->selection_style_.subtarget_color, module_option_group::colors).VisibleWhen(this->selection_);
        case 59: return module_option::Slider("Background tint", "Selection background opacity; zero hides the fill and keeps the border.", &this->selection_style_.background_opacity, 0, 1, 0.01f, module_option_group::appearance).Ordered(450).VisibleWhen(this->selection_);
        case 60: return module_option::Slider("Horizontal padding", "Space between entry content and the selection frame, in pixels.", &this->selection_style_.padding_x, 0, 16, 1, module_option_group::appearance).Ordered(400, true).VisibleWhen(this->selection_);
        case 61: return module_option::Slider("Vertical padding", "Space above and below each entry; reserved even when not selected.", &this->selection_style_.padding_y, 0, 12, 1, module_option_group::appearance).Ordered(401).VisibleWhen(this->selection_);
        case 62: return module_option::Toggle("Selection arrow", "Show a matching chevron beside the selected entry.", &this->selection_style_.arrow).VisibleWhen(this->selection_);
        case 63:
        {
            auto option = module_option::Toggle("Hide in preview", "Hide this alliance panel in edit mode; normal alliance visibility is unchanged.", &this->hide_preview_);
            option.available = this->group_ != 0;
            return option;
        }
        default:
            if (index < base_option_count + meter_style_option_count) return MeterStyleOption(index - base_option_count, this->meters_[this->selected_bar_]);
            return TextProfileOption(index - base_option_count - meter_style_option_count, this->text_[this->selected_text_]);
        }
    }

    auto party_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr std::array<module_config_section, section_count> sections{{
            {"General", "Visibility, preview and distance.", "general"},
            {"Layout", "Resource layout, selected bar width and height, positions and offsets.", "layout"},
            {"Job Icons", "Icon visibility, size, anchoring and offsets. Job text is under Text > Jobs.", "job-icons"},
            {"Resources", "Choose HP, MP, TP and cast information to display.", "resources"},
            {"Bar Style", "Choose a bar, then adjust its theme, shape and bookends.", "bar-style"},
            {"Animation", "Smooth fill changes and animated sheen for the selected bar.", "animation"},
            {"Bar Effects", "Shadows and glows for the selected bar.", "bar-effects"},
            {"Colors", "Resource fills, state colors and bar tints.", "colors"},
            {"Status Effects", "Choose combined effects or split rows, then edit the settings for that mode.", "status-effects"},
            {"Text", "Choose an element to edit its display, layout, font, offsets, colors and effects together.", "text"},
            {"Selection", "Target and subtarget colors, background tint, padding, independent box sizing and arrow.", "selection"},
            {"Tickers", "Swing and resting timing for you and observable party members. Other members' timing is estimated.", "tickers"},
            {"Window", "Overall list width, entry spacing, reserved rows, padding and opacity.", "window"}}};
        return sections;
    }

    auto party_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount() || index == 2) return false;
        if (index >= selection_size_options_start && index < selection_size_options_start + 2) return section == selection_section;
        if (index == leader_option) return section == general_section;
        if (index >= job_text_options_start || (index >= 7 && index <= 11) || index == 49)
            return section == text_section && this->selected_text_ == 1;
        if (index >= preset_options_start) return this->group_ == 0 && section == general_section;
        if (index == status_theme_option) return section == status_section && this->status_order_ != 3;
        if (index >= heal_options_start) return section == (heal_preview_settings::ColorOption(index - heal_options_start) ? colors_section : resources_section);
        if (index == mp_preview_option) return section == resources_section;
        if (index >= icon_offset_options_start) return section == jobs_section;
        if (index == status_preview_option) return this->group_ == 0 && section == status_section;
        if (index >= status_options_start)
        {
            const auto local = index - status_options_start;
            const bool split = this->status_order_ == 3;
            return section == status_section && (local == 0 || ((local == 3 || local == 4 || local == 5 + ui::status_strip_settings::option_count) ? !split : split));
        }
        if ((index == 31 || index == 32 || index == 43 || index == 44) && this->status_order_ == 3) return false;
        if (index >= ticker_options_start) return section == tickers_section;
        if (index == icon_follow_option) return section == jobs_section;
        if (index == sync_option) return section == general_section;
        if (index >= base_option_count + meter_style_option_count) return section == text_section;
        if (index >= base_option_count)
        {
            static constexpr config_section sections[]{general_section, layout_section, bar_style_section, animation_section,
                bar_effects_section, colors_section, text_section};
            return section == sections[static_cast<size_t>(MeterConfigSection(index - base_option_count))];
        }
        switch (index)
        {
        case 0: case 1: case 3: case 5: case 33: return section == general_section;
        case 63: return this->group_ != 0 && section == general_section;
        case 17: case 57: case 58: case 59: case 60: case 61: case 62: return section == selection_section;
        case 4: return section == general_section;
        case 18: case 19: case 20: case 27: case 28: case 29: case 34: case 35: case 56: return section == window_section;
        case 21: return section == layout_section;
        case 6: case 30: return section == jobs_section;
        case 12: case 13: case 14: case 16: case 22: case 23: return section == resources_section;
        case 24: return section == layout_section || section == bar_style_section || section == animation_section || section == bar_effects_section;
        case 25: return section == layout_section || section == bar_style_section || section == animation_section || section == bar_effects_section || section == colors_section;
        case 26: return section == text_section;
        case 36: case 37: case 38: case 39: case 40: return section == colors_section;
        case 15: case 31: case 32: case 41: case 42: case 43: case 44: case 45: case 46: case 47: return section == status_section;
        case 48: return section == resources_section || (section == layout_section && this->selected_bar_ == 3);
        case 50: return section == bar_style_section;
        case 52: case 53: case 54: case 55: return section == layout_section;
        case 51: return section == resources_section || (section == layout_section && this->selected_bar_ == 2);
        default: return false;
        }
    }

    auto party_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (index >= preset_options_start && index < preset_options_start + 2 && this->group_ == 0)
        {
            const auto preset = index == preset_options_start ? 0u : 2u;
            const auto alliances = this->preset_alliances_;
            this->ApplyPreset(preset);
            for (auto* list : alliances) if (list != nullptr) list->ApplyPreset(preset);
            return;
        }
        if (index == 6 || index == 17 || index == 27 || index == 30 || index == 35 || index == 60)
            for (auto& width : this->bar_widths_) width = std::min(width, this->BarWidthLimit());
        if (this->style_mode_ == 0 && index >= base_option_count && index < base_option_count + meter_style_option_count)
            for (auto& style : this->meters_) CopyMeterStyleOption(index - base_option_count, this->meters_[this->selected_bar_], style);
        if (this->style_mode_ == 0 && index == 52)
            this->bar_widths_.fill(this->bar_widths_[this->selected_bar_]);
        if (this->style_mode_ == 0 && index == 53)
            this->bar_positions_.fill(this->bar_positions_[this->selected_bar_]);
        if (this->style_mode_ == 0 && index == 54)
            this->bar_offsets_x_.fill(this->bar_offsets_x_[this->selected_bar_]);
        if (this->style_mode_ == 0 && index == 55)
            this->bar_offsets_y_.fill(this->bar_offsets_y_[this->selected_bar_]);
    }

    auto party_module::ApplyPreset(const size_t preset) -> void
    {
        if (preset > 2) return;
        if (preset == 0 || preset == 2)
        {
            const auto captured = PartyPreset(preset == 0 ? 306 : 307);
            if (captured.empty()) return;
            party_module saved;
            saved.Load(settings_store::FromIniText(captured));
            this->CopyPreferences(saved);
            this->state_.enabled = saved.state_.enabled;
            this->hide_preview_ = saved.hide_preview_;
            return;
        }
        this->ResetDefaults(ui::theme::CreateDefault());
        this->status_order_ = 3;
        this->split_status_gap_ = 2; this->status_gap_ = 4;
        this->buff_limit_ = 32;
        for (auto& row : this->split_statuses_) { row.columns = 32; row.placement = 2; }
        this->layout_ = bars_horizontal;
        this->state_.bounds.width = 420.0f;
        this->fit_width_ = true;
        this->bar_widths_ = {}; this->bar_positions_ = {};
        this->bar_offsets_x_ = {}; this->bar_offsets_y_ = {};
        this->spacing_ = preset == 2 ? 6.0f : 4.0f;
        this->job_icons_ = true;
        this->icon_size_ = preset == 2 ? 24.0f : 20.0f;
        this->icon_follow_name_ = true;
        this->jobs_ = false; this->title_ = false; this->distance_ = false;
        this->labels_ = false;
        this->tp_ = true; this->tp_display_ = 0;
        this->always_mp_ = preset == 2;
        this->hp_display_ = preset == 2 ? 4 : 0; this->mp_display_ = 0;
        this->job_placement_ = jobs_name; this->job_anchor_ = job_anchor_name; this->stack_jobs_ = false; this->cast_placement_ = cast_separate;
        if (preset == 1)
        {
            this->text_[0].offset_x = 4; this->text_[0].offset_y = 8;
            this->cast_placement_ = cast_name;
            this->bar_offsets_x_[3] = 0; this->bar_offsets_y_[3] = 0;
        }
        this->container_.padding = 4;
        for (auto& meter : this->meters_)
        {
            meter.appearance = ui::meter_style::window_theme;
            meter.height = 12;
            meter.bookends = true;
            meter.themed_bookends = true;
        }
        this->animations_ = {};
        this->state_.content_bounds = {};
    }

    auto party_module::ConfigurePresetSample(const size_t preset, const party_module* textSource) -> void
    {
        this->ApplyPreset(preset);
        this->PrepareSample(textSource);
    }

    auto party_module::ConfigurePresetSample(const settings_store& settings) -> void
    {
        this->ResetDefaults(ui::theme::CreateDefault());
        this->Load(settings);
        this->PrepareSample();
    }

    auto party_module::PrepareSample(const party_module* textSource) -> void
    {
        this->jobs_ = this->distance_ = this->buffs_ = this->casts_ = this->selection_ = this->flash_tp_ = false;
        this->hp_display_ = this->mp_display_ = 0;
        for (auto& ticker : this->tickers_.tickers) ticker.enabled = false;
        for (auto& meter : this->meters_) meter.animation_enabled = false;
        if (textSource != nullptr)
            for (size_t index = 0; index < this->text_.size(); ++index) this->text_[index].style = textSource->text_[index].style;
    }

    auto party_module::RenderSample(const party_member& member, const ui::rect& area, const module_render_context& context) -> ui::rect
    {
        this->xiv1_theme_ = context.Design().IsXiv1Theme();
        if (area.width <= 0 || area.height <= 0) return {};
        this->state_.bounds.width = area.width + 2 * (this->container_.edge_size + this->container_.padding + this->SelectionPadding().x);
        row_layout prepared;
        prepared.slots = this->ResourceSlots(member);
        const auto width = this->LayoutContainer().canvas_width;
        prepared.entry = this->LayoutEntry({0, 0, width, 0}, member.in_zone, prepared.slots, false);
        const ui::rect row{area.x + (area.width - width) * .5f, area.y + (area.height - prepared.entry.height) * .5f, width, prepared.entry.height};
        const auto clip = context.Design().Clip(area);
        this->DrawRow(member, prepared, row, row, 0, ::GetTickCount64(), context);
        return row;
    }

    auto party_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->ModuleEnabled() && this->state_.enabled && this->source_ != nullptr && this->source_->Available() && this->Count() != 0 &&
            (this->group_ != 0 || this->solo_ || this->Count() > 1 || this->source_->InAlliance());
    }

    auto party_module::ResourceSlots(const party_member& member) const -> resource_slots
    {
        const bool mp = this->always_mp_ || member.mp_max != 0 || member.mp != 0 || PartyJobHasMP(member.main_job) || PartyJobHasMP(member.sub_job);
        const bool casting = this->casts_ && member.in_zone && member.cast.Active(::GetTickCount64());
        const auto now = ::GetTickCount64();
        return {mp || (casting && this->cast_placement_ == cast_mp), this->tp_ || (casting && this->cast_placement_ == cast_tp), mp, this->tp_, casting,
            this->Ticker(member, 0, now).active, this->Ticker(member, 1, now).active};
    }

    auto party_module::CastHeight(void) const -> float
    {
        return std::max(this->meters_[3].height + 2, ResolvedLineHeight(this->text_[5]));
    }

    auto party_module::HeaderHeight(const bool casting, const bool details, const bool inZone) const -> float
    {
        const float height = std::max({ResolvedLineHeight(this->text_[0]), details ? ResolvedLineHeight(this->text_[4]) : 0,
            inZone && this->jobs_ && this->job_anchor_ == job_anchor_name && this->job_placement_ == jobs_name ? this->JobTextHeight() : 0});
        return casting && this->cast_placement_ == cast_name ? std::max(height, this->CastHeight()) : height;
    }

    auto party_module::HeaderPadding(const bool casting) const -> float
    {
        return casting && this->cast_placement_ == cast_name ? std::max(0.0f, this->bar_offsets_y_[3]) : 0;
    }

    auto party_module::ResourceBarHeight(const bool hasMP, const bool hasTP, const bool casting) const -> float
    {
        if (this->xiv1_theme_) return ui::xiv1_resource_height + 2;
        const bool replaceMP = casting && this->cast_placement_ == cast_mp, replaceTP = casting && this->cast_placement_ == cast_tp;
        const float values = std::max({this->hp_display_ != 5 ? ResolvedLineHeight(this->text_[3]) : 0,
            hasMP && !replaceMP && this->mp_display_ != 5 ? ResolvedLineHeight(this->text_[mp_text]) : 0,
            hasTP && !replaceTP ? ResolvedLineHeight(this->text_[tp_text]) : 0});
        const float height = std::max({this->meters_[0].height, hasMP && !replaceMP ? this->meters_[1].height : 0,
            hasTP && !replaceTP && this->tp_display_ == 0 ? this->meters_[2].height : 0, values}) + 2;
        return replaceMP || replaceTP ? std::max(height, this->CastHeight()) : height;
    }

    auto party_module::BarWidthLimit(void) const -> float
    {
        return std::max(1.0f, this->state_.bounds.width - 2 * (this->container_.edge_size + this->container_.padding + this->SelectionPadding().x));
    }

    auto party_module::SelectionPadding(void) const -> ui::point
    {
        return this->selection_ ? ui::point{this->selection_style_.padding_x, this->selection_style_.padding_y} : ui::point{};
    }

    auto party_module::SelectionBounds(const ui::rect& row) const -> ui::rect
    {
        const auto padding = this->SelectionPadding();
        const float width = std::max(1.0f, row.width + 2 * padding.x + this->selection_width_extension_);
        const float height = std::max(1.0f, row.height + 2 * padding.y + this->selection_height_extension_);
        return {row.x + (row.width - width) * 0.5f, row.y + (row.height - height) * 0.5f, width, height};
    }

    auto party_module::BarWidth(const size_t bar, const float available) const -> float
    {
        return this->bar_widths_[bar] > 0 ? std::min(this->bar_widths_[bar], std::max(0.0f, available)) : std::max(0.0f, available);
    }

    auto party_module::PositionBar(const size_t bar, const ui::rect& bounds, const ui::rect& area) const -> ui::rect
    {
        auto positioned = bounds;
        if (this->bar_positions_[bar] != position_layout)
        {
            const float anchor = this->bar_positions_[bar] == position_right ? 1.0f : this->bar_positions_[bar] == position_center ? 0.5f : 0.0f;
            positioned.x = area.x + (area.width - bounds.width) * anchor;
        }
        positioned.x += this->bar_offsets_x_[bar];
        positioned.y += this->bar_offsets_y_[bar];
        return positioned;
    }

    auto party_module::CastBounds(const ui::rect& bounds) const -> ui::rect
    {
        auto result = this->PositionBar(3, {bounds.x, bounds.y, this->BarWidth(3, bounds.width), bounds.height}, bounds);
        if (this->xiv1_theme_ && this->cast_placement_ == cast_name) result.y += 4;
        return result;
    }

    auto party_module::LayoutResources(const ui::rect& bounds, const resource_slots& slots) const -> resource_layout
    {
        resource_layout layout;
        const float height = this->ResourceBarHeight(slots.mp, slots.tp, slots.casting);
        constexpr float gap = 10;
        const auto rowWidths = [&](const float available, const std::array<float, 3>& weights) {
            std::array<float, 3> widths{};
            float fixed{}, automaticWeight{}, automaticCount{}, count{};
            for (size_t i = 0; i < widths.size(); ++i)
                if (weights[i] > 0)
                {
                    ++count;
                    if (this->bar_widths_[i] > 0) fixed += this->bar_widths_[i];
                    else { automaticWeight += weights[i]; ++automaticCount; }
                }
            const float space = std::max(0.0f, available);
            const float minimum = fixed > 0 ? std::min(24.0f, space / std::max(1.0f, count)) : 0;
            const float fixedSpace = std::max(0.0f, space - minimum * automaticCount);
            const float scale = fixed > 0 ? std::min(1.0f, fixedSpace / fixed) : 1;
            const float remainder = std::max(0.0f, space - fixed * scale - minimum * automaticCount);
            for (size_t i = 0; i < widths.size(); ++i)
                if (weights[i] > 0)
                    widths[i] = this->bar_widths_[i] > 0 ? this->bar_widths_[i] * scale : minimum + remainder * weights[i] / automaticWeight;
            return widths;
        };
        layout.height = height;
        if (this->xiv1_theme_)
        {
            const float x = bounds.x + this->bar_offsets_x_[0], y = bounds.y + std::max(0.0f, this->bar_offsets_y_[0]);
            const float half = std::max(1.0f, (bounds.width - (slots.mp ? ui::xiv1_resource_gap : 0)) / (slots.mp ? 2 : 1));
            layout.bars[0] = {x, y, half, height};
            if (slots.mp) layout.bars[1] = {x + half + ui::xiv1_resource_gap, y, half, height};
            const auto hpText = ui::Xiv1ResourceValue(this->text_[3], this->text_[3].offset_y);
            const auto mpText = ui::Xiv1ResourceValue(this->text_[mp_text], this->text_[3].offset_y);
            const float valueHeight = std::max(this->hp_display_ == 5 ? 0 : this->ResolvedLineHeight(hpText),
                slots.mp && this->mp_display_ != 5 ? this->ResolvedLineHeight(mpText) : 0);
            layout.height = y - bounds.y + height + valueHeight * .5f + 2;
            if (slots.tp && this->tp_display_ != 1)
            {
                const auto tpText = ui::Xiv1ResourceValue(this->text_[tp_text], this->text_[tp_text].offset_y);
                const float tpY = bounds.y + layout.height + 5 + std::max(0.0f, this->bar_offsets_y_[2]);
                layout.bars[2] = {bounds.x + this->bar_offsets_x_[2], tpY, bounds.width, ui::xiv1_tp_height + 2};
                layout.height = tpY - bounds.y + ui::xiv1_tp_height + this->ResolvedLineHeight(tpText) * .5f + 2;
            }
            this->LayoutTickerText(layout, slots, bounds.y);
            return layout;
        }
        else if (this->layout_ == bars_horizontal)
        {
            const float usable = std::max(0.0f, bounds.width - ((slots.mp ? 1 : 0) + (slots.tp ? 1 : 0)) * gap);
            const auto widths = rowWidths(usable, {0.52f, slots.mp ? 0.29f : 0, slots.tp ? 0.19f : 0});
            float x = bounds.x;
            layout.bars[0] = {x, bounds.y, widths[0], height}; x += widths[0] + gap;
            if (slots.mp) { layout.bars[1] = {x, bounds.y, widths[1], height}; x += widths[1] + gap; }
            if (slots.tp) layout.bars[2] = {x, bounds.y, widths[2], height};
        }
        else if (this->layout_ == bars_classic)
        {
            layout.bars[0] = {bounds.x, bounds.y, this->BarWidth(0, bounds.width), height};
            if (slots.mp || slots.tp)
            {
                const float verticalGap = this->labels_ ? ResolvedLineHeight(this->text_[2]) * 0.5f + 2 : 2;
                const auto widths = rowWidths(std::max(0.0f, bounds.width - (slots.mp && slots.tp ? gap : 0)),
                    {0, slots.mp ? 0.75f : 0, slots.tp ? 0.25f : 0});
                const float y = bounds.y + height + verticalGap;
                if (slots.tp) layout.bars[2] = {bounds.x, y, widths[2], height};
                if (slots.mp) layout.bars[1] = {bounds.x + bounds.width - widths[1], y, widths[1], height};
            }
        }
        else if (this->layout_ == bars_mp_below_tp_left)
        {
            const float verticalGap = this->labels_ ? ResolvedLineHeight(this->text_[2]) * 0.5f + 2 : 2;
            if (slots.mp) layout.height += height + verticalGap;
            float tpWidth = slots.tp ? std::min(bounds.width * 0.22f, 96.0f) : 0;
            if (slots.tp)
            {
                const float available = std::max(0.0f, bounds.width - gap);
                const float minimum = std::min(24.0f, available * 0.5f);
                const float requested = std::max(this->bar_widths_[0], slots.mp ? this->bar_widths_[1] : 0);
                tpWidth = this->bar_widths_[2] > 0 ? std::min(this->bar_widths_[2], available - minimum) :
                    std::clamp(available - requested, minimum, std::max(minimum, tpWidth));
            }
            const float inset = slots.tp ? tpWidth + gap : 0;
            const float width = std::max(0.0f, bounds.width - inset);
            layout.bars[0] = {bounds.x + inset, bounds.y, this->BarWidth(0, width), height};
            if (slots.mp) layout.bars[1] = {bounds.x + inset, bounds.y + height + verticalGap, this->BarWidth(1, width), height};
            if (slots.tp) layout.bars[2] = {bounds.x, bounds.y + (layout.height - height) * 0.5f, tpWidth, height};
        }
        else
        {
            layout.bars[0] = {bounds.x, bounds.y, this->BarWidth(0, bounds.width), height};
            if (slots.mp || slots.tp)
            {
                layout.height += height + gap;
                const auto widths = rowWidths(bounds.width - (slots.mp && slots.tp ? gap : 0), {0, slots.mp ? 1.0f : 0, slots.tp ? 1.0f : 0});
                if (slots.mp) layout.bars[1] = {bounds.x, bounds.y + height + gap, widths[1], height};
                if (slots.tp) layout.bars[2] = {bounds.x + (slots.mp ? widths[1] + gap : 0), bounds.y + height + gap, widths[2], height};
            }
        }
        layout.height = 0;
        for (size_t i = 0; i < layout.bars.size(); ++i)
        {
            if ((i == 1 && !slots.mp) || (i == 2 && !slots.tp)) continue;
            auto& bar = layout.bars[i];
            bar = this->PositionBar(i, bar, bounds);
            float bottom = bar.y + bar.height;
            if (slots.casting && ((i == 1 && this->cast_placement_ == cast_mp) || (i == 2 && this->cast_placement_ == cast_tp)))
                bottom += std::max(0.0f, this->bar_offsets_y_[3]);
            layout.height = std::max(layout.height, bottom - bounds.y);
        }
        this->LayoutTickerText(layout, slots, bounds.y);
        return layout;
    }

    auto party_module::LayoutContainer(void) const -> container_layout
    {
        const float trim = this->container_.edge_size + this->container_.padding + this->SelectionPadding().x;
        const float maximum = this->BarWidthLimit();
        container_layout result{this->state_.bounds.width, maximum, 0};
        if (this->xiv1_theme_)
        {
            const float offset = this->bar_offsets_x_[0];
            const float available = std::max(1.0f, maximum - std::abs(offset));
            const float desired = this->fit_width_ && this->bar_widths_[0] > 0 ? this->bar_widths_[0] * 2 + ui::xiv1_resource_gap : available;
            result.canvas_width = std::clamp(desired, std::min(64.0f, available), available);
            result.inset = std::max(0.0f, -offset);
            result.width = result.canvas_width + std::abs(offset) + trim * 2;
            return result;
        }
        if (!this->fit_width_ || this->bar_widths_[0] <= 0) return result;

        const bool tp = this->tp_ || (this->casts_ && this->cast_placement_ == cast_tp);
        constexpr float gap = 10;
        const float hp = this->bar_widths_[0];
        float desired{};
        if (this->layout_ == bars_horizontal)
            desired = hp + (this->bar_widths_[1] > 0 ? this->bar_widths_[1] : hp * 0.29f / 0.52f) + gap +
                (tp ? (this->bar_widths_[2] > 0 ? this->bar_widths_[2] : hp * 0.19f / 0.52f) + gap : 0);
        else if (this->layout_ == bars_mp_below_tp_left)
            desired = std::max(hp, this->bar_widths_[1]) + (tp ?
                (this->bar_widths_[2] > 0 ? this->bar_widths_[2] : std::min(64.0f, maximum * 0.22f)) + gap : 0);
        else
            desired = std::max(hp, this->bar_widths_[1] + (tp ? this->bar_widths_[2] + gap : 0) +
                (this->bar_widths_[1] <= 0 ? 24 : 0) + (tp && this->bar_widths_[2] <= 0 ? 24 : 0));
        if (this->casts_ && this->cast_placement_ == cast_separate) desired = std::max(desired, this->bar_widths_[3]);

        float leftOffset{}, rightOffset{};
        for (size_t i = 0; i < this->bar_offsets_x_.size(); ++i)
        {
            if ((i == 2 && !tp) || (i == 3 && !this->casts_)) continue;
            leftOffset = std::min(leftOffset, this->bar_offsets_x_[i]);
            rightOffset = std::max(rightOffset, this->bar_offsets_x_[i]);
        }
        const float available = std::max(1.0f, maximum + leftOffset - rightOffset);
        result.canvas_width = std::clamp(desired, std::min(64.0f, available), available);
        const auto resources = this->LayoutResources({0, 0, result.canvas_width, 0}, {true, tp});
        float left{}, right = result.canvas_width;
        const auto include = [&](const ui::rect& bar) {
            left = std::min(left, bar.x);
            right = std::max(right, bar.x + bar.width);
        };
        include(resources.bars[0]); include(resources.bars[1]);
        if (tp)
        {
            auto value = this->tp_display_ == 1 ? this->TPNumberBounds(resources.bars[2]) : resources.bars[2];
            if (this->tp_display_ == 1) value.x += this->text_[tp_text].offset_x;
            include(value);
            if (this->tp_display_ == 1 && this->labels_)
            {
                value = resources.bars[2]; value.x += this->text_[2].offset_x;
                include(value);
            }
        }
        if (this->casts_)
        {
            const auto cast = this->cast_placement_ == cast_mp ? resources.bars[1] : this->cast_placement_ == cast_tp ? resources.bars[2] :
                ui::rect{0, 0, result.canvas_width, this->CastHeight()};
            include(this->CastBounds(this->cast_placement_ == cast_name ? this->NameCastArea(cast, cast.x + this->text_[0].offset_x) : cast));
        }
        result.inset = -left;
        result.width = right - left + trim * 2;
        return result;
    }

    auto party_module::UseCompactLayout(void) -> void
    {
        this->layout_ = bars_mp_below_tp_left;
        this->fit_width_ = true;
        this->bar_positions_ = {};
        this->bar_offsets_x_ = {};
        this->bar_offsets_y_ = {};
    }

    auto party_module::DescribeLayout(void) const -> std::string
    {
        const auto container = this->LayoutContainer();
        char text[512]{};
        std::snprintf(text, sizeof(text), "[XIUI] %s layout=%d; maximum=%.0f; container=%.0f; bars=%.0f; fit=%s; TP=%s; spacing=%.0f. ",
            this->DisplayName(), this->layout_, this->state_.bounds.width, container.width, container.canvas_width,
            this->fit_width_ ? "on" : "off", this->tp_ ? "shown" : "hidden", this->spacing_);
        std::string result{text};
        for (size_t i = 0; i < this->meters_.size(); ++i)
        {
            std::snprintf(text, sizeof(text), "%s width=%.0f pos=%d xy=%.0f,%.0f; ", bar_names[i], this->bar_widths_[i],
                this->bar_positions_[i], this->bar_offsets_x_[i], this->bar_offsets_y_[i]);
            result += text;
        }
        return result;
    }

    auto party_module::EntryHeight(const bool inZone, const bool hasMP, const bool hasTP) const -> float
    {
        const auto container = this->LayoutContainer();
        const float width = container.canvas_width;
        return this->LayoutEntry({0, 0, width, 0}, inZone, {hasMP, hasTP, hasMP, hasTP}).height;
    }

    auto party_module::ZoneStyle(void) const -> ui::container_style
    {
        auto style = this->container_;
        style.padding = 4;
        return style;
    }

    auto party_module::ZoneInfoHeight(void) const -> float
    {
        const auto style = this->ZoneStyle();
        return ResolvedLineHeight(this->text_[zone_text]) + 2 * (style.edge_size + style.padding);
    }

    auto party_module::RowHeight(const bool inZone, const size_t statusCount, const bool hasMP, const bool hasTP) const -> float
    {
        return this->StatusHeight(this->EntryHeight(inZone, hasMP, hasTP), inZone, statusCount);
    }

    auto party_module::StatusHeight(const float entry, const bool inZone, const size_t statusCount, const party_member* member) const -> float
    {
        if (!this->buffs_ || this->group_ != 0 || !inZone || statusCount == 0) return entry;
        const float width = this->LayoutContainer().canvas_width;
        if (this->status_order_ == 3)
        {
            std::array<size_t, 2> counts{statusCount, statusCount};
            if (member != nullptr) { const auto groups = this->StatusGroups(*member); counts = {groups[0].count, groups[1].count}; }
            const auto layouts = this->SplitStatusLayouts({0, 0, width, entry}, {}, counts);
            const float bottom = std::max(layouts[0].bottom, layouts[1].bottom);
            return this->status_placement_ == status_inline ? entry + bottom + 5 : std::max(entry, bottom);
        }
        const auto reserved = this->StatusLayout({0, 0, width, entry}, {}, statusCount);
        return this->status_placement_ == status_inline ? entry + reserved.bounds.height + 5 : std::max(entry, reserved.bounds.height);
    }

    auto party_module::BuffColumns(const float width) const -> size_t
    {
        return static_cast<size_t>(std::max(1.0f, std::min(this->status_columns_, std::floor((width + this->status_spacing_) / (this->buff_size_ + this->status_spacing_)))));
    }

    auto party_module::StatusGroups(const party_member& member) const -> std::array<status_group, 2>
    {
        std::array<status_group, 2> result{};
        for (size_t i = 0; i < std::min(member.buff_count, member.buffs.size()); ++i)
        {
            auto& group = result[ui::IsDebuffStatus(member.buffs[i]) ? 1 : 0];
            group.effects[group.count++] = {member.buffs[i], 0, 0, member.buff_expires[i], false};
        }
        return result;
    }

    auto party_module::SplitStatusLayouts(const ui::rect& row, const ui::rect& party,
        const std::array<size_t, 2>& counts) const -> std::array<split_status_layout, 2>
    {
        std::array<split_status_layout, 2> result{};
        const bool inside = this->status_placement_ == status_inline;
        const float inset = !inside && this->status_frame_ ? this->status_container_.edge_size + this->status_container_.padding : 0;
        float top = row.y;
        for (size_t group = 0; group < result.size(); ++group)
        {
            const auto& settings = this->split_statuses_[group];
            if (!settings.enabled || counts[group] == 0) continue;
            const auto columns = settings.columns >= 1 ? settings.columns : this->status_columns_;
            const auto width = inside ? row.width : columns * (settings.size + settings.spacing) - settings.spacing;
            const auto x = inside ? row.x : this->status_placement_ == status_left ? party.x - this->status_gap_ - width - inset * 2 : party.x + party.width + this->status_gap_;
            const ui::rect origin{x + inset, top + inset, width, 0};
            auto& layout = result[group];
            layout.strip = ui::LayoutStatusStrip(counts[group], origin, settings);
            layout.frame = {layout.strip.bounds.x - inset, layout.strip.bounds.y - inset, layout.strip.bounds.width + inset * 2, layout.strip.height + inset * 2};
            layout.bottom = std::max(top, layout.frame.y + layout.frame.height);
            top = layout.bottom + this->split_status_gap_;
        }
        return result;
    }

    auto party_module::StatusLayout(const ui::rect& row, const ui::rect& party, const size_t count) const -> status_layout
    {
        status_layout layout{};
        layout.count = std::min(count, static_cast<size_t>(this->buff_limit_));
        layout.overflow = count - layout.count;
        const size_t slots = layout.count + (layout.overflow != 0 ? 1 : 0);
        if (slots == 0) return layout;
        const bool inside = this->status_placement_ == status_inline;
        layout.framed = !inside && this->status_frame_;
        layout.reverse = this->status_placement_ == status_left;
        layout.columns = std::min(slots, inside ? this->BuffColumns(row.width) : static_cast<size_t>(this->status_columns_));
        const size_t rows = (slots + layout.columns - 1) / layout.columns;
        const float width = static_cast<float>(layout.columns) * (this->buff_size_ + this->status_spacing_) - this->status_spacing_;
        const float height = static_cast<float>(rows) * (this->buff_size_ + this->status_row_spacing_) - this->status_row_spacing_;
        const float inset = layout.framed ? this->status_container_.edge_size + this->status_container_.padding : 0;
        layout.bounds = {row.x, row.y, width + inset * 2, height + inset * 2};
        if (!inside)
            layout.bounds.x = layout.reverse ? party.x - this->status_gap_ - layout.bounds.width : party.x + party.width + this->status_gap_;
        layout.content = layout.framed ? ui::container_renderer::ContentBounds(layout.bounds, this->status_container_) : layout.bounds;
        return layout;
    }

    auto party_module::StatusIconBounds(const status_layout& layout, const size_t index) const -> ui::rect
    {
        const float column = static_cast<float>(index % layout.columns) * (this->buff_size_ + this->status_spacing_);
        return {layout.reverse ? layout.content.x + layout.content.width - this->buff_size_ - column : layout.content.x + column,
            layout.content.y + static_cast<float>(index / layout.columns) * (this->buff_size_ + this->status_row_spacing_), this->buff_size_, this->buff_size_};
    }

    auto party_module::DrawStatuses(const party_member& member, const ui::rect& row, const ui::rect& party, const module_render_context& context) const -> void
    {
        if (!this->buffs_ || this->group_ != 0 || !member.in_zone || member.buff_count == 0) return;
        const auto groups = this->StatusGroups(member);
        if (this->status_order_ == 3)
        {
            const auto layouts = this->SplitStatusLayouts(row, party, {groups[0].count, groups[1].count});
            for (size_t group = 0; group < groups.size(); ++group)
            {
                const auto& settings = this->split_statuses_[group];
                const auto& layout = layouts[group];
                if (layout.strip.count == 0) continue;
                if (this->status_frame_ && this->status_placement_ != status_inline)
                { auto style = this->status_container_; style.opacity *= settings.opacity; context.DrawContainer(layout.frame, style); }
                auto origin = layout.strip.bounds;
                origin.x -= settings.offset_x; origin.y -= settings.offset_y;
                auto drawing = settings; drawing.placement = 0;
                const auto statuses = std::span<const combat_status>{groups[group].effects}.first(groups[group].count);
                ui::DrawStatusStrip(statuses, origin, ::GetTickCount64(), this->text_[7 + group], context, drawing);
                if (settings.tooltips)
                    for (size_t i = 0; i < layout.strip.count; ++i) this->status_hits_.push_back({statuses[i], context.Design().Transform().Map(layout.strip.Icon(i)), member.id});
            }
            return;
        }
        std::array<combat_status, 32> ordered{};
        if (this->status_order_ == 0)
            for (size_t i = 0; i < member.buff_count; ++i) ordered[i] = {member.buffs[i], 0, 0, member.buff_expires[i], false};
        else
        {
            const size_t first = this->status_order_ == 1 ? 1 : 0;
            std::copy_n(groups[first].effects.begin(), groups[first].count, ordered.begin());
            std::copy_n(groups[1 - first].effects.begin(), groups[1 - first].count, ordered.begin() + groups[first].count);
        }
        const auto layout = this->StatusLayout(row, party, member.buff_count);
        if (layout.framed) context.DrawContainer(layout.bounds, this->status_container_);
        const auto text = [&](std::string value, size_t index) {
            const auto bounds = this->StatusIconBounds(layout, index);
            value = xiui::ui::FitText(context.Design(), std::move(value), bounds.width, this->text_[1].style);
            context.DrawText(value.c_str(), bounds, bounds, this->text_[1]);
        };
        for (size_t i = 0; i < layout.count; ++i)
        {
            const auto& status = ordered[i];
            const auto at = this->StatusIconBounds(layout, i);
            const auto artwork = context.Icons() != nullptr ? context.Icons()->StatusIcon(status.id, this->status_theme_) : ui::game_icon_view{};
            if (artwork.texture != nullptr) context.Design().DrawTexture(artwork.texture, at, 0xFFFFFFFF, artwork.uv);
            else text(std::to_string(status.id), i);
            if (this->status_tooltips_) this->status_hits_.push_back({status, context.Design().Transform().Map(at), member.id});
            if (this->status_timers_ && status.expires > ::GetTickCount64())
            {
                const auto timer = ui::StatusTimerText(status, ::GetTickCount64(), {});
                auto profile = this->text_[ui::IsDebuffStatus(status.id) ? 8 : 7];
                profile.placement = 0; profile.style.alignment = ui::text_alignment::center;
                profile.style.scale *= 0.72f;
                const ui::rect label{at.x - 2, at.y + at.height - 12, at.width + 4, 14};
                context.DrawText(timer.c_str(), label, label, profile);
            }
        }
        if (layout.overflow != 0) text("+" + std::to_string(layout.overflow), layout.count);
    }

    auto party_module::DrawCast(const party_cast& cast, const ui::rect& bounds, const uint64_t now, const module_render_context& context) const -> void
    {
        if (!cast.Active(now)) return;
        const auto bar = this->CastBounds(bounds);
        const float progress = static_cast<float>((cast.interrupted != 0 ? cast.interrupted : now) - cast.started) / (std::max(0.25f, cast.seconds) * 1000);
        auto profile = this->text_[5];
        profile.style.alignment = this->cast_placement_ == cast_name ? ui::text_alignment::left : ui::text_alignment::center;
        auto style = this->meters_[3];
        const auto label = xiui::ui::FitText(context.Design(), cast.interrupted != 0 ? "Interrupted" : cast.name,
            context.MeterTextBounds(bar, style).width, profile.style);
        style.animation_enabled = false;
        meter_animation_state animation{};
        context.DrawMeter(bar, "", label.c_str(), std::clamp(progress, 0.0f, 1.0f), this->fill_tops_[3], this->fills_[3], style,
            animation, false, nullptr, &profile);
    }

    auto party_module::TreasureLot(const party_member& member) const -> std::optional<uint16_t>
    {
        if (this->treasure_ == nullptr || this->source_ == nullptr || member.preview || !member.active || !member.in_zone || !this->text_[4].visible ||
            this->source_->Count(0) + this->source_->Count(1) + this->source_->Count(2) < 2) return {};
        return this->treasure_->MemberLot(this->treasure_slot_, this->treasure_generation_, member.id);
    }

    auto party_module::DrawRow(const party_member& member, const row_layout& prepared, const ui::rect& row, const ui::rect& party, const size_t rowIndex, const uint64_t now, const module_render_context& context) -> void
    {
        const auto& design = context.Design();
        const auto text = [&](std::string value, const ui::rect& bounds, const size_t profile, const std::optional<D3DCOLOR> tint = std::nullopt) {
            auto style = tint ? TintTextProfile(this->text_[profile], *tint) : this->text_[profile];
            if (this->xiv1_theme_ && profile == 0)
            {
                style.style.font = ui::xiv1_italic_font;
                style.offset_x -= 4;
            }
            value = xiui::ui::FitText(design, std::move(value), bounds.width, design.ResolveHudText(style.style));
            context.DrawText(value.c_str(), bounds, bounds, style);
        };
        if (this->selection_ && member.selected && !(this->ControllerFocused() && this->controller_member_id_ == member.id))
        {
            context.DrawSelection(this->SelectionBounds(row), member.subtarget, this->selection_style_);
        }
        const auto& slots = prepared.slots;
        const auto& detail = prepared.detail;
        auto entry = prepared.entry;
        const auto position = [&](ui::rect& at) { at.x += row.x; at.y += row.y; };
        for (auto* at : {&entry.header, &entry.jobs, &entry.zone, &entry.cast, &entry.icon}) position(*at);
        for (auto& bar : entry.resources.bars) position(bar);
        position(entry.resources.ticker_text);
        for (auto& ticker : entry.tickers) position(ticker);
        const float x = entry.header.x, width = entry.header.width, headerHeight = entry.header.height;
        const bool casting = slots.casting;
        float headerRight = x + width;
        if (this->xiv1_theme_ && slots.tp && this->tp_display_ == 1)
        {
            const auto tpStyle = ui::Xiv1ResourceValue(this->text_[tp_text], 0, true).style;
            const auto tp = ui::LayoutXiv1TPNumber(entry.resources.bars[2], 0, design.MeasureText("3000", tpStyle), 0, design.TextHeight(tpStyle));
            headerRight = std::min(headerRight, (this->casts_ && this->cast_placement_ == cast_tp ? entry.resources.bars[2].x : tp.value.x) - 8);
        }
        if (const auto lot = this->TreasureLot(member))
        {
            auto profile = this->text_[4];
            profile.placement = 0; profile.offset_x = profile.offset_y = 0;
            profile.style = this->TextStyle(profile); profile.style.alignment = ui::text_alignment::right;
            const float badgeWidth = std::min(std::max(0.0f, headerRight - x),
                std::max({design.MeasureText("[---]", profile.style), design.MeasureText("[Pass]", profile.style), design.MeasureText("[999]", profile.style)}) + 2);
            const ui::rect badge{headerRight - badgeWidth, entry.header.y, badgeWidth, headerHeight};
            const auto label = *lot == 65535 ? std::string{"[Pass]"} : *lot == 0 ? std::string{"[---]"} : "[" + std::to_string(*lot) + "]";
            const D3DCOLOR tint = *lot == 65535 ? 0xFFA8A8A8 : *lot == 0 ? 0xFFE3D2A0 : 0xFFBFE07D;
            context.DrawText(label.c_str(), badge, badge, TintTextProfile(profile, tint));
            headerRight = std::max(x, badge.x - 6);
        }
        const float headerWidth = std::max(0.0f, headerRight - x);
        const float detailWidth = detail.empty() ? 0 : std::min(headerWidth * 0.44f, design.MeasureText(detail.c_str(), this->text_[4].style) + 2);
        const auto nameTint = !member.in_zone ? std::optional<D3DCOLOR>{0xFFA8A8A8} : member.hp_percent < 25 ? std::optional{this->low_hp_} : std::nullopt;
        const bool leader = this->show_leader_ && (member.leader || member.alliance_leader);
        const float leaderSpace = leader ? 10.0f : 0;
        const float nameWidth = std::max(0.0f, headerWidth - detailWidth - 4);
        const bool jobsByName = member.in_zone && this->jobs_ && this->job_anchor_ == job_anchor_name && this->job_placement_ == jobs_name;
        const auto jobs = member.in_zone && this->jobs_ ? this->JobLines(member) : std::array<std::string,2>{};
        const float jobWidth = !jobsByName || jobs[0].empty() ? 0 : std::min(nameWidth * 0.5f,
            std::max(design.MeasureText(jobs[0].c_str(), this->text_[1].style),design.MeasureText(jobs[1].c_str(), this->text_[1].style)) + 8);
        const ui::rect nameBounds{x + leaderSpace, entry.header.y, std::max(0.0f, nameWidth - jobWidth - leaderSpace), headerHeight};
        const bool nameCast = casting && this->cast_placement_ == cast_name;
        auto nameProfile = this->text_[0];
        if (this->xiv1_theme_)
        {
            nameProfile.style.font = ui::xiv1_italic_font;
            nameProfile.offset_x -= 4;
        }
        const float nameRoom = std::max(0.0f, nameBounds.width - std::max(0.0f, nameProfile.offset_x) -
            (nameProfile.placement != 0 ? 8.0f : 0));
        const auto fittedName = xiui::ui::FitText(design, member.name, nameRoom, design.ResolveHudText(nameProfile.style));
        const auto visibleName = context.TextBounds(fittedName.c_str(), nameBounds, nameBounds, nameProfile);
        const auto castArea = this->NameCastArea(nameBounds, visibleName.x);
        if (nameCast) this->DrawCast(member.cast, castArea, now, context);
        const auto castBounds = this->CastBounds(castArea);
        const auto jobBounds = this->JobTextBounds(entry.header, nameCast ? castBounds : visibleName, x + nameWidth);
        const auto drawJobs = [&](ui::rect area) {
            if (jobs[1].empty()) { text(jobs[0], area, 1); return; }
            area.y += std::max(0.0f,(area.height-this->JobTextHeight())*0.5f);
            area.height = ResolvedLineHeight(this->text_[1]);
            for (const auto& line : jobs)
            {
                if (!line.empty()) text(line, area, 1);
                area.y += area.height;
            }
        };
        const auto drawIdentity = [&] {
            if (!nameCast) text(fittedName, nameBounds, 0, nameTint);
            const auto jobAtlas = this->job_icons_ && member.in_zone && member.main_job > 0 && member.main_job <= 24 && context.Icons() != nullptr ?
                context.Icons()->JobAtlas(this->xiv1_theme_ ? ui::xiv1_job_icon_theme : -1) : nullptr;
            if (jobAtlas != nullptr)
            {
                const auto icon = member.main_job - 1;
                const auto iconBounds = this->JobIconBounds(entry, visibleName, leaderSpace);
                design.DrawTexture(jobAtlas, iconBounds, 0xFFFFFFFF,
                    {(static_cast<float>(icon % 8) * 64 + 0.5f) / 512, (static_cast<float>(icon / 8) * 64 + 0.5f) / 256, 63.0f / 512, 63.0f / 256});
            }
            if (leader)
            {
                const std::array<ui::slot_dot, 1> dot{{{{visibleName.x - 7, visibleName.y + (visibleName.height - 6) * 0.5f, 6, 6}, 0xFFFFD84D}}};
                design.DrawSlotDots(dot);
            }
            if (jobsByName) drawJobs(jobBounds);
            text(detail, {headerRight - detailWidth, entry.header.y, detailWidth, headerHeight}, 4,
                member.distance_known && this->distance_highlight_ > 0 && member.distance <= this->distance_highlight_ ? std::optional{this->range_color_} : std::nullopt);
        };
        if (!member.in_zone)
        {
            const auto content = context.DrawContainer(entry.zone, this->ZoneStyle());
            const auto& profile = this->text_[zone_text];
            const auto zone = xiui::ui::FitText(design, member.zone_name, content.width, profile.style);
            context.DrawText(zone.c_str(), content, content, profile);
            drawIdentity();
            return;
        }
        const bool jobsBelow = this->jobs_ && this->job_anchor_ == job_anchor_name && this->job_placement_ == jobs_below;
        auto& animation = this->animations_[rowIndex];
        if (animation.id != member.id) { animation = {}; animation.id = member.id; }
        animation.meters[0].Bind((uint64_t{member.hp_max_exact ? member.hp_max : 0} << 32) | member.id);
        const auto meter = [&](const size_t index, const ui::rect& bounds) {
            if (casting && ((index == 1 && this->cast_placement_ == cast_mp) || (index == 2 && this->cast_placement_ == cast_tp)))
            { this->DrawCast(member.cast, bounds, now, context); return; }
            if ((index == 1 && !slots.mp_values) || (index == 2 && !slots.tp_values)) return;
            const auto value = index == 0 ? FormatPartyResource(member.hp, member.hp_percent, member.hp_max, member.hp_max_exact, this->hp_display_) :
                index == 1 ? FormatPartyResource(member.mp, member.mp_percent, member.mp_max, member.mp_max_exact, this->mp_display_) : std::to_string(member.tp);
            const auto ratio = index == 0 ? member.hp_percent / 100.0f : index == 1 ?
                (member.mp_max_exact ? module_render_context::Ratio(member.mp, member.mp_max) : member.mp_percent / 100.0f) : member.tp / 3000.0f;
            auto valueStyle = this->text_[index == 2 ? tp_text : index == 1 ? mp_text : 3];
            if (index == 0) valueStyle = TintTextProfile(valueStyle, ui::HPValueColor(member.hp_percent));
            if (index == 2 && member.tp >= 1000) valueStyle = TintTextProfile(valueStyle, this->ready_tp_);
            if (index == 2 && this->flash_tp_ && member.tp >= 1000)
            {
                const auto alpha = static_cast<uint32_t>(190 + 65 * (std::sin(static_cast<float>(now % 1600) * 6.2831853f / 1600) + 1) * 0.5f);
                valueStyle = TintTextProfile(valueStyle, (alpha << 24) | 0xFFFFFF);
            }
            auto caption = this->text_[2];
            if (this->xiv1_theme_)
            {
                caption = ui::Xiv1ResourceLabel(caption);
                valueStyle = ui::Xiv1ResourceValue(valueStyle, this->text_[index == 2 ? tp_text : 3].offset_y, index == 2 && this->tp_display_ == 1);
            }
            const auto drawThemeText = [&](const ui::rect& track) {
                const auto text = ui::LayoutXiv1ResourceText(track, design.TextHeight(caption.style), design.TextHeight(valueStyle.style));
                if (index != 2 && this->ResourceLabels()) context.DrawText(index == 0 ? "HP" : "MP", text.label, text.label, caption);
                const auto number = xiui::ui::FitText(design, value, text.value.width, valueStyle.style);
                context.DrawText(number.c_str(), text.value, text.value, valueStyle);
            };
            if (index == 2 && this->tp_display_ == 1)
            {
                if (this->xiv1_theme_)
                {
                    valueStyle.offset_x = valueStyle.offset_y = 0;
                    const auto text = ui::LayoutXiv1TPNumber(bounds, 0, design.MeasureText("3000", valueStyle.style),
                        0, design.TextHeight(valueStyle.style));
                    context.DrawText(value.c_str(), text.value, text.value, valueStyle);
                    return;
                }
                const auto number = this->TPNumberBounds(bounds);
                context.DrawText(value.c_str(), number, number, valueStyle);
                return;
            }
            auto fill = this->fills_[index];
            auto fillTop = this->fill_tops_[index];
            if (this->xiv1_theme_ && index < 2)
            {
                fillTop = index == 0 ? 0xFFD8C1B6 : 0xFFD5DDB0;
                fill = index == 0 ? 0xFFBC7160 : 0xFF9FAE58;
            }
            if (member.distance_known && member.distance > 20.8f)
            {
                const auto alpha = static_cast<uint32_t>(member.distance > 50 ? 115 : 178);
                fill = (fill & 0xFFFFFF) | (((fill >> 24) * alpha / 255) << 24);
                fillTop = (fillTop & 0xFFFFFF) | (((fillTop >> 24) * alpha / 255) << 24);
            }
            auto meterStyle = this->meters_[index];
            if (this->xiv1_theme_) { meterStyle.height = ui::Xiv1ResourceHeight(index); meterStyle.bookends = true; }
            const float textWidth = context.MeterTextBounds(bounds, meterStyle).width;
            const auto fitted = xiui::ui::FitText(design, value, textWidth, valueStyle.style);
            const auto label = xiui::ui::FitText(design, !this->labels_ || index == 2 ? "" : index == 0 ? "HP" : "MP", textWidth, this->text_[2].style);
            std::array<ui::meter_strip, 2> strips{};
            size_t stripCount{};
            if (index == 0)
                for (size_t kind = 0; kind < this->tickers_.tickers.size(); ++kind)
                {
                    if (!(kind == 0 ? slots.swing : slots.resting)) continue;
                    const auto& ticker = this->tickers_.tickers[kind];
                    if (ticker.display == 2) continue;
                    const auto timer = this->Ticker(member, kind, now);
                    const auto color = timer.known && (timer.seconds <= 0.5f || timer.state == "Ready") ? ticker.ready_color : ticker.color;
                    strips[stripCount++] = {ticker.direction != 0 ? 1 - timer.ratio : timer.ratio, ticker.height, ticker.gap, MeterGradientTop(color), color};
                }
            std::array<ui::meter_range, 2> previews{};
            if (index == 0 && !member.preview) previews = this->heal_preview_.Ranges(this->heal_prediction_, member.id, now);
            if (index == 1 && !member.preview && member.mp_max_exact && this->mp_preview_source_ != nullptr && this->mp_cost_preview_)
                previews[0] = MPPreviewRanges(this->mp_preview_source_->MPPreview(now), member.mp, member.mp_max, member.id, now, design.GetTheme())[0];
            context.DrawMeter(bounds, this->xiv1_theme_ ? "" : label.c_str(), this->xiv1_theme_ ? "" : fitted.c_str(), ratio,
                fillTop, fill, meterStyle, animation.meters[index], true, &this->text_[2], &valueStyle,
                ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic,
                index == 2 ? meter_fill_mode::tp : index == 0 ? meter_fill_mode::health : meter_fill_mode::single, std::span{strips}.first(stripCount), previews);
            if (this->xiv1_theme_)
                drawThemeText(ui::Xiv1ResourceTrack(bounds, index));
        };
        const auto& resources = entry.resources;
        meter(0, resources.bars[0]);
        if (slots.mp) meter(1, resources.bars[1]);
        if (slots.tp) meter(2, resources.bars[2]);
        this->DrawTickers(member, entry, slots, now, context);
        if (casting && this->cast_placement_ == cast_separate) this->DrawCast(member.cast, entry.cast, now, context);
        const bool inside = this->status_placement_ == status_inline;
        this->DrawStatuses(member, inside ? ui::rect{x, row.y + entry.height + 5, width, 0} : row, party, context);
        if (jobsBelow) drawJobs(entry.jobs);
        if (this->jobs_ && this->job_anchor_ == job_anchor_hp) drawJobs(this->HPJobTextBounds(resources.bars[0]));
        drawIdentity();
    }

    auto party_module::PreviewMember(const size_t row, const party_member* live) const -> party_member
    {
        party_member member;
        if (live != nullptr) member = *live;
        else
        {
            constexpr const char* previewNames[]{"Player", "White Mage", "Red Mage", "Paladin", "Bard", "Samurai"};
            member.active = member.in_zone = member.distance_known = true;
            member.id = static_cast<uint32_t>(0xFFFF0000 + this->group_ * 6 + row); member.slot = static_cast<uint32_t>(this->group_ * 6 + row);
            member.name = previewNames[row];
            member.main_job = preview_jobs[row]; member.main_level = 75; member.sub_job = 1; member.sub_level = 37;
            member.hp = 1270 - static_cast<uint32_t>(row) * 120; member.hp_percent = static_cast<uint8_t>(100 - row * 13);
            member.hp_max = PartyResourceMaximum(member.hp, member.hp_percent); member.mp = row % 3 == 0 ? 0 : 520;
            member.mp_percent = member.mp != 0 ? 68 : 0; member.mp_max = PartyResourceMaximum(member.mp, member.mp_percent);
            member.tp = static_cast<uint32_t>(row) * 480; member.distance = static_cast<float>(row) * 6.2f;
            member.selected = row == 2; member.sync = row == 1;
            if (row == 2) member.cast = {"Cure IV", 4, ::GetTickCount64() - 1800, 0, 4};
            if (row == preview_remote_row) { member.in_zone = false; member.zone_name = "Ru'Lude Gardens"; member.distance_known = false; }
        }
        member.preview = true;
        member.leader = row == 0; member.alliance_leader = false;
        member.status = row == 1 || row == 4 ? 33 : row == 0 || row == 3 ? 1 : 0;
        return member;
    }

    auto party_module::DisplayMembers(const size_t count, const bool preview, const bool statusPreview) const -> std::array<party_member, 6>
    {
        std::array<party_member, 6> result{};
        size_t row{};
        if (this->source_ != nullptr)
            for (size_t slot = this->group_ * 6; slot < (this->group_ + 1) * 6 && row < count; ++slot)
                if (const auto& member = this->source_->Members()[slot]; member.active)
                { result[row] = preview ? this->PreviewMember(row, &member) : member; ++row; }
        for (; row < count; ++row) result[row] = this->PreviewMember(row);
        if (statusPreview)
        {
            constexpr std::array<uint16_t, 8> effects{33, 3, 40, 4, 43, 5, 116, 6};
            constexpr std::array<uint32_t, 8> seconds{125, 48, 310, 63, 92, 65, 240, 45};
            const auto now = ::GetTickCount64();
            for (size_t i = 0; i < count; ++i)
            {
                auto& member = result[i];
                if (!member.in_zone) continue;
                member.buffs = {}; member.buff_expires = {}; member.buff_count = effects.size(); member.buffs_known = true;
                std::copy(effects.begin(), effects.end(), member.buffs.begin());
                for (size_t effect = 0; effect < effects.size(); ++effect)
                    member.buff_expires[effect] = now - now % 30000 + static_cast<uint64_t>(seconds[effect]) * 1000;
            }
        }
        return result;
    }

    auto party_module::LayoutRows(const size_t count, const bool preview, const bool statusPreview) const -> party_layout
    {
        const auto members = this->DisplayMembers(count, preview, statusPreview);
        return this->LayoutRows(std::span{members}.first(count), statusPreview);
    }

    auto party_module::LayoutRows(const std::span<const party_member> members, const bool statusPreview) const -> party_layout
    {
        const auto count = members.size();
        party_layout layout;
        const auto container = this->LayoutContainer();
        layout.content_width = container.canvas_width;
        layout.content_inset = container.inset;
        layout.heights.fill(this->RowHeight());
        const float padding = this->container_.edge_size + this->container_.padding;
        const auto measure = [&](const party_member& member, const size_t index) {
            auto& prepared = layout.entries[index];
            prepared.slots = this->ResourceSlots(member);
            prepared.detail = this->DetailText(member);
            prepared.entry = this->LayoutEntry({0, 0, layout.content_width, 0}, member.in_zone, prepared.slots, !prepared.detail.empty());
            const auto& slots = prepared.slots;
            const auto& entry = prepared.entry;
            layout.heights[index] = this->StatusHeight(entry.height, member.in_zone, member.buff_count, &member);
            for (size_t kind = 0; kind < entry.tickers.size(); ++kind)
                if ((kind == 0 ? slots.swing : slots.resting) && this->tickers_.tickers[kind].display == 1)
                {
                    const auto& at = entry.tickers[kind];
                    const float right = padding + this->SelectionPadding().x + layout.content_inset + at.x + at.width +
                        std::max(0.0f, this->tickers_.tickers[kind].text.offset_x);
                    layout.right_overflow = std::max(layout.right_overflow, right - container.width);
                }
        };
        for (size_t row = 0; row < count; ++row) measure(members[row], row);
        layout.rows = this->expand_ || (this->expand_alliance_ && this->source_ != nullptr && this->source_->InAlliance()) ? 6 :
            std::max(count, static_cast<size_t>(this->minimum_rows_));
        const float heading = this->title_ || statusPreview ? ResolvedLineHeight(this->text_[heading_text]) + 6 : 0;
        const float selectionHeight = this->SelectionPadding().y * 2;
        const float height = padding * 2 + heading + selectionHeight * layout.rows + std::accumulate(layout.heights.begin(), layout.heights.begin() + layout.rows, 0.0f) +
            static_cast<float>(layout.rows - 1) * this->spacing_;
        layout.bounds = {this->state_.bounds.x, this->state_.bounds.y, container.width, height};
        if (this->bottom_)
        {
            const float castingHeight = this->LayoutEntry({0, 0, layout.content_width, 0}, true, {true, true, true, true, this->casts_}).height;
            const float maximumRow = std::max({this->RowHeight(true, 32), this->RowHeight(false), this->StatusHeight(castingHeight, true, 32)});
            const float fullHeight = padding * 2 + heading + 6 * (maximumRow + selectionHeight) + 5 * this->spacing_;
            layout.bounds.y += fullHeight - height;
            layout.leading_space = std::accumulate(layout.heights.begin() + count, layout.heights.begin() + layout.rows, 0.0f) +
                static_cast<float>(layout.rows - count) * (this->spacing_ + selectionHeight);
        }
        return layout;
    }

    auto party_module::PartyBounds(const size_t count) const -> ui::rect
    {
        return this->LayoutRows(count).bounds;
    }

    auto party_module::VisualBounds(const bool editing) const -> ui::rect
    {
        const bool preview = editing && this->preview_;
        return this->VisualBounds(this->LayoutRows(this->DisplayCount(editing), preview, this->StatusPreview(editing)));
    }

    auto party_module::VisualBounds(const party_layout& layout) const -> ui::rect
    {
        auto bounds = layout.bounds;
        if (this->buffs_ && this->group_ == 0 && this->status_placement_ != status_inline)
        {
            if (this->status_order_ == 3)
            {
                const auto rows = this->SplitStatusLayouts({}, bounds, {32, 32});
                const auto right = bounds.x + bounds.width;
                float left = bounds.x, farRight = right;
                for (const auto& row : rows)
                    if (row.frame.width > 0) { left = std::min(left, row.frame.x); farRight = std::max(farRight, row.frame.x + row.frame.width); }
                bounds.x = left; bounds.width = farRight - left;
                return {bounds.x - 8, bounds.y - 8, bounds.width + layout.right_overflow + 16, bounds.height + 16};
            }
            const float extension = this->StatusLayout({}, bounds, 32).bounds.width + this->status_gap_;
            if (this->status_placement_ == status_left) bounds.x -= extension;
            bounds.width += extension;
        }
        return {bounds.x - 8, bounds.y - 8, bounds.width + layout.right_overflow + 16, bounds.height + 16};
    }

    auto party_module::PlacementBounds(const bool editing) const -> ui::rect
    {
        const bool preview = editing && this->preview_;
        return this->LayoutRows(this->DisplayCount(editing), preview, this->StatusPreview(editing)).bounds;
    }

    auto party_module::HitTarget(const float screenX, const float screenY) const -> std::pair<uint32_t, uint32_t>
    {
        const float x = this->State().transform.InverseX(screenX), y = this->State().transform.InverseY(screenY);
        if (!this->ShouldRender({})) return {};
        const auto layout = this->LayoutRows(this->Count());
        const auto padding = this->SelectionPadding();
        float top = layout.bounds.y + this->container_.edge_size + this->container_.padding + layout.leading_space +
            (this->title_ ? ResolvedLineHeight(this->text_[heading_text]) + 6 : 0);
        size_t row{};
        for (size_t slot = this->group_ * 6; slot < (this->group_ + 1) * 6; ++slot)
        {
            const auto& member = this->source_->Members()[slot];
            if (!member.active) continue;
            const float height = layout.heights[row++] + padding.y * 2;
            if (member.in_zone && member.entity_valid && x >= layout.bounds.x && x < layout.bounds.x + layout.bounds.width && y >= top && y < top + height)
                return {member.id, member.index};
            top += height + this->spacing_;
        }
        return {};
    }

    auto party_module::BeginControllerBrowse(const uint32_t preferredId, const uint32_t preferredSlot) -> bool
    {
        this->EndControllerBrowse();
        if (!this->ShouldRender({})) return false;
        this->controller_browsing_ = true;
        this->controller_member_id_ = preferredId;
        this->controller_member_slot_ = preferredSlot;
        if (preferredId == 0 && preferredSlot == UINT32_MAX)
            for (size_t slot = this->group_ * 6; slot < (this->group_ + 1) * 6; ++slot)
                if (const auto& member = this->source_->Members()[slot]; member.active && member.selected)
                { this->controller_member_id_ = member.id; this->controller_member_slot_ = member.slot; break; }
        return this->ResolveControllerMember() != nullptr;
    }

    auto party_module::EndControllerBrowse(void) -> void
    {
        this->EndControllerStatus();
        this->controller_browsing_ = false;
        this->controller_member_id_ = 0;
        this->controller_member_slot_ = UINT32_MAX;
    }

    auto party_module::ResolveControllerMember(void) -> const party_member*
    {
        if (!this->ControllerFocused()) { this->EndControllerBrowse(); return nullptr; }
        const party_member* fallback{};
        uint32_t closest = UINT32_MAX;
        for (size_t slot = this->group_ * 6; slot < (this->group_ + 1) * 6; ++slot)
        {
            const auto& member = this->source_->Members()[slot];
            if (!member.active || member.id == 0) continue;
            if (member.id == this->controller_member_id_)
            { this->controller_member_slot_ = member.slot; return &member; }
            const auto distance = this->controller_member_slot_ == UINT32_MAX ? static_cast<uint32_t>(slot) :
                static_cast<uint32_t>(std::abs(static_cast<int>(slot % 6) - static_cast<int>(this->controller_member_slot_ % 6)));
            if (fallback == nullptr || distance < closest) { fallback = &member; closest = distance; }
        }
        if (this->controller_member_id_ != 0) { this->EndControllerBrowse(); return nullptr; }
        if (fallback != nullptr)
        {
            this->controller_member_id_ = fallback->id;
            this->controller_member_slot_ = fallback->slot;
        }
        else this->EndControllerBrowse();
        return fallback;
    }

    auto party_module::ControllerMember(void) -> std::optional<party_member>
    {
        const auto* member = this->ResolveControllerMember();
        return member != nullptr ? std::optional<party_member>{*member} : std::nullopt;
    }

    auto party_module::MoveControllerBrowse(const int direction) -> bool
    {
        const auto* current = this->ResolveControllerMember();
        if (current == nullptr || direction == 0) return false;
        std::array<const party_member*, 6> members{};
        size_t count{}, selected{};
        for (size_t slot = this->group_ * 6; slot < (this->group_ + 1) * 6; ++slot)
        {
            const auto& member = this->source_->Members()[slot];
            if (!member.active || member.id == 0) continue;
            if (member.id == current->id) selected = count;
            members[count++] = &member;
        }
        const auto next = (selected + (direction > 0 ? 1 : count - 1)) % count;
        if (next == selected) return false;
        this->controller_member_id_ = members[next]->id;
        this->controller_member_slot_ = members[next]->slot;
        return true;
    }

    auto party_module::ControllerBounds(void) const -> ui::rect
    {
        if (!this->ControllerFocused()) return {};
        const auto layout = this->LayoutRows(this->Count());
        const auto padding = this->SelectionPadding();
        float top = layout.bounds.y + this->container_.edge_size + this->container_.padding + layout.leading_space +
            (this->title_ ? ResolvedLineHeight(this->text_[heading_text]) + 6 : 0);
        size_t row{};
        for (size_t slot = this->group_ * 6; slot < (this->group_ + 1) * 6; ++slot)
        {
            const auto& member = this->source_->Members()[slot];
            if (!member.active) continue;
            const float height = layout.heights[row++] + padding.y * 2;
            if (member.id == this->controller_member_id_) return {layout.bounds.x, top, layout.bounds.width, height};
            top += height + this->spacing_;
        }
        return {};
    }

    auto party_module::BeginControllerStatus(void) -> bool
    {
        this->EndControllerStatus();
        if (!this->ControllerFocused() || (this->status_order_ != 3 && !this->status_tooltips_)) return false;
        for (const auto& hit : this->status_hits_)
            if (hit.entity == this->controller_member_id_)
            { this->controller_status_id_ = hit.status.id; this->controller_status_active_ = true; return true; }
        return false;
    }

    auto party_module::MoveControllerStatus(const int direction) -> bool
    {
        if (!this->controller_status_active_ || direction == 0) return false;
        const auto current = ui::StatusHitIndex(this->status_hits_, this->controller_member_id_, this->controller_status_id_, this->controller_status_occurrence_);
        if (!current) { this->EndControllerStatus(); return false; }
        std::array<size_t, 32> entries{};
        size_t count{}, selected{};
        for (size_t i = 0; i < this->status_hits_.size(); ++i)
            if (this->status_hits_[i].entity == this->controller_member_id_ && count < entries.size())
            {
                if (i == *current) selected = count;
                entries[count++] = i;
            }
        if (count == 0) { this->EndControllerStatus(); return false; }
        const auto index = entries[(selected + (direction > 0 ? 1 : count - 1)) % count];
        this->controller_status_id_ = this->status_hits_[index].status.id;
        this->controller_status_occurrence_ = ui::StatusHitOccurrence(this->status_hits_, index);
        return true;
    }

    auto party_module::ControllerStatusBounds(void) const -> ui::rect
    {
        if (!this->ControllerFocused() || !this->controller_status_active_ || (this->status_order_ != 3 && !this->status_tooltips_)) return {};
        const auto index = ui::StatusHitIndex(this->status_hits_, this->controller_member_id_, this->controller_status_id_, this->controller_status_occurrence_);
        return index ? this->status_hits_[*index].bounds : ui::rect{};
    }

    auto party_module::RenderStatusTooltip(const module_render_context& context, IResourceManager* resources,
        const float x, const float y, const bool controller) const -> void
    {
        if (!this->buffs_ || (this->status_order_ != 3 && !this->status_tooltips_) || this->group_ != 0) return;
        const auto focused = this->controller_status_active_ ? ui::StatusHitIndex(this->status_hits_, this->controller_member_id_, this->controller_status_id_, this->controller_status_occurrence_) : std::nullopt;
        for (const auto& hit : this->status_hits_)
        {
            const auto& at = hit.bounds;
            if (controller ? focused && &hit == &this->status_hits_[*focused] :
                x >= at.x && x < at.x + at.width && y >= at.y && y < at.y + at.height)
            { ui::DrawStatusTooltip(hit, resources, this->text_[ui::IsDebuffStatus(hit.status.id) ? 8 : 7], context); return; }
        }
    }

    auto party_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        this->xiv1_theme_ = context.Design().IsXiv1Theme();
        this->status_hits_.clear();
        const auto now = ::GetTickCount64();
        const bool preview = context.IsEditing() && this->preview_;
        const bool statusPreview = this->StatusPreview(context.IsEditing());
        const auto count = this->DisplayCount(context.IsEditing());
        auto members = this->DisplayMembers(count, preview, statusPreview);
        if ((preview || statusPreview) && this->Count() == 0 && snapshot.player_name[0] != '\0') members[0].name = snapshot.player_name;
        if (count == 0 && !context.IsEditing()) return;
        const auto layout = this->LayoutRows(std::span{members}.first(count), statusPreview);
        const float heading = this->title_ || statusPreview ? ResolvedLineHeight(this->text_[heading_text]) + 6 : 0;
        const auto& bounds = layout.bounds;
        auto content = this->xiv1_theme_ ? ui::container_renderer::ContentBounds(bounds, this->container_) : context.DrawContainer(bounds, this->container_);
        const auto selectionPadding = this->SelectionPadding();
        content.x += layout.content_inset + selectionPadding.x;
        content.width = layout.content_width;
        float y = content.y;
        if (heading > 0)
        {
            const auto& profile = this->text_[heading_text];
            const auto title = std::string{this->DisplayName()} + "  " + std::to_string(count) + "/6" + (statusPreview ? " | Status preview" : "");
            context.DrawText(title.c_str(), {content.x, y, content.width, heading}, {content.x, y, content.width, heading}, profile);
            y += heading;
        }
        y += layout.leading_space;
        for (size_t i = 0; i < count; ++i)
        {
            this->DrawRow(members[i], layout.entries[i], {content.x, y + selectionPadding.y, content.width, layout.heights[i]}, bounds, i, now, context);
            y += layout.heights[i] + 2 * selectionPadding.y + this->spacing_;
        }
        this->state_.minimum_height = bounds.height + (bounds.y - this->state_.bounds.y);
        if (this->xiv1_theme_)
            this->state_.content_bounds = {bounds.x - this->state_.bounds.x, bounds.y - this->state_.bounds.y, bounds.width, bounds.height};
        else
        {
            this->state_.bounds.height = this->state_.minimum_height;
            this->state_.content_bounds = {};
        }
    }
}
