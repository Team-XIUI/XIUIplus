#include "party_module.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace
{
    constexpr const char* bar_names[]{"hp", "mp", "tp", "cast"};
    constexpr const char* text_names[]{"name", "jobs", "labels", "values", "details", "cast", "heading"};
    constexpr const char* ids[]{"party", "alliance1", "alliance2"};
    constexpr const char* names[]{"Party", "Alliance 1", "Alliance 2"};
    constexpr size_t preview_remote_row = 5;
    constexpr uint8_t preview_jobs[]{2, 3, 5, 7, 10, 12};
    constexpr std::array<size_t, 6> preview_status_counts{0, 4, 0, 0, 0, 0};

    auto FitText(const xiui::ui::design_system& design, std::string text, const float width, const xiui::ui::text_style& style) -> std::string
    {
        if (design.MeasureText(text.c_str(), style) <= width) return text;
        while (!text.empty())
        {
            size_t last = text.size() - 1;
            while (last > 0 && (static_cast<unsigned char>(text[last]) & 0xC0) == 0x80) --last;
            text.resize(last);
            if (design.MeasureText((text + "...").c_str(), style) <= width) return text + "...";
        }
        return {};
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
    party_module::party_module(const size_t group) : group_(std::min<size_t>(2, group))
    {
        this->state_.bounds.x = 900.0f - static_cast<float>(this->group_) * 430;
        this->container_.padding = 8;
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
        this->text_ = {{{theme.title_text}, {theme.caption_text}, {theme.meter_text}, {theme.meter_text}, {theme.caption_text}, {theme.meter_text}, {theme.title_text}}};
        this->text_[0].style.scale = 0.88f;
        this->text_[heading_text].style.scale = 0.88f;
        for (size_t i = 1; i < heading_text; ++i) this->text_[i].style.scale = 0.75f;
        this->text_[3].style.alignment = ui::text_alignment::right;
        this->text_[4].style.alignment = ui::text_alignment::right;
    }

    auto party_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        const auto* moduleEnabled = this->module_enabled_;
        *this = party_module{this->group_};
        this->state_.bounds = bounds;
        this->source_ = source;
        this->module_enabled_ = moduleEnabled;
        this->ApplyMeterDefaults(theme.meter);
        this->ApplyTextDefaults(theme);
    }

    auto party_module::CopyPreferences(const party_module& party) -> void
    {
        const auto group = this->group_;
        const auto* source = this->source_;
        const auto state = this->state_;
        const bool hidePreview = this->hide_preview_;
        const auto* moduleEnabled = this->module_enabled_;
        *this = party;
        this->group_ = group;
        this->source_ = source;
        this->state_.enabled = state.enabled;
        this->state_.bounds.x = state.bounds.x;
        this->state_.bounds.y = state.bounds.y;
        this->state_.content_bounds = {};
        this->hide_preview_ = hidePreview;
        this->module_enabled_ = moduleEnabled;
        this->animations_ = {};
    }

    auto party_module::Load(const settings_store& store) -> void
    {
        const auto* section = this->Id();
        this->state_.enabled = store.ReadBool(section, "enabled", true);
        this->hide_preview_ = store.ReadBool(section, "hide_preview", false);
        this->state_.bounds = store.ReadRect(this->Section("layout.").c_str(), this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, this->state_.minimum_width, this->state_.maximum_width);
        const std::array<std::pair<const char*, bool*>, 20> bools{{
            std::pair{"solo", &this->solo_}, {"hide_events", &this->hide_events_}, {"preview", &this->preview_}, {"title", &this->title_},
            {"distance", &this->distance_}, {"job_icons", &this->job_icons_}, {"jobs", &this->jobs_}, {"main_level", &this->main_level_},
            {"sub_job", &this->sub_job_}, {"sub_level", &this->sub_level_}, {"master_level", &this->master_level_}, {"show_tp", &this->tp_},
            {"flash_tp", &this->flash_tp_}, {"always_mp", &this->always_mp_}, {"buffs", &this->buffs_}, {"casts", &this->casts_},
            {"selection", &this->selection_}, {"bottom", &this->bottom_}, {"expand", &this->expand_}, {"expand_alliance", &this->expand_alliance_}}};
        for (const auto& [key, value] : bools) *value = store.ReadBool(section, key, *value);
        this->tp_ = store.ReadBool(section, "show_tp", store.ReadBool(section, "tp", this->tp_));
        this->fit_width_ = store.ReadBool(section, "fit_width", true);
        this->icon_follow_name_ = store.ReadBool(section, "icon_follow_name", false);
        this->show_sync_ = store.ReadBool(section, "show_sync", true);
        this->layout_ = std::clamp(store.ReadInt(section, "layout", bars_horizontal), bars_horizontal, bars_mp_below_tp_left);
        this->cast_placement_ = std::clamp(store.ReadInt(section, "cast_placement", cast_separate), cast_separate, cast_tp);
        this->job_placement_ = std::clamp(store.ReadInt(section, "job_placement", jobs_name), jobs_name, jobs_below);
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
        this->status_placement_ = std::clamp(store.ReadInt(section, "status_placement", status_left), status_left, status_inline);
        this->status_frame_ = store.ReadBool(section, "status_container", true);
        this->status_columns_ = std::clamp(store.ReadFloat(section, "status_columns", 8), 1.0f, 16.0f);
        this->status_spacing_ = std::clamp(store.ReadFloat(section, "status_spacing", 2), 0.0f, 12.0f);
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
            const auto fallback = i == heading_text ? module_text_profile{this->text_[0].style} : this->text_[i];
            this->text_[i] = LoadTextProfile(store, this->Section("text.", text_names[i]).c_str(), fallback);
        }
        this->low_hp_ = store.ReadArgb(section, "low_hp", this->low_hp_);
        this->range_color_ = store.ReadArgb(section, "range_color", this->range_color_);
        this->ready_tp_ = store.ReadArgb(section, "ready_tp", this->ready_tp_);
        this->animations_ = {};
    }

    auto party_module::Save(const settings_store& store) const -> void
    {
        const auto* section = this->Id();
        store.WriteBool(section, "enabled", this->state_.enabled);
        store.WriteBool(section, "hide_preview", this->hide_preview_);
        store.WriteRect(this->Section("layout.").c_str(), this->state_.bounds);
        const std::array<std::pair<const char*, bool>, 20> bools{{
            std::pair{"solo", this->solo_}, {"hide_events", this->hide_events_}, {"preview", this->preview_}, {"title", this->title_},
            {"distance", this->distance_}, {"job_icons", this->job_icons_}, {"jobs", this->jobs_}, {"main_level", this->main_level_},
            {"sub_job", this->sub_job_}, {"sub_level", this->sub_level_}, {"master_level", this->master_level_}, {"show_tp", this->tp_},
            {"flash_tp", this->flash_tp_}, {"always_mp", this->always_mp_}, {"buffs", this->buffs_}, {"casts", this->casts_},
            {"selection", this->selection_}, {"bottom", this->bottom_}, {"expand", this->expand_}, {"expand_alliance", this->expand_alliance_}}};
        for (const auto& [key, value] : bools) store.WriteBool(section, key, value);
        store.WriteBool(section, "fit_width", this->fit_width_);
        store.WriteBool(section, "icon_follow_name", this->icon_follow_name_);
        store.WriteBool(section, "show_sync", this->show_sync_);
        store.WriteInt(section, "layout", this->layout_); store.WriteInt(section, "hp_display", this->hp_display_);
        store.WriteInt(section, "cast_placement", this->cast_placement_);
        store.WriteInt(section, "job_placement", this->job_placement_);
        store.WriteInt(section, "tp_display", this->tp_display_);
        store.WriteBool(section, "labels", this->labels_);
        store.WriteInt(section, "mp_display", this->mp_display_); store.WriteInt(section, "selected_bar", this->selected_bar_);
        store.WriteInt(section, "selected_text", this->selected_text_); store.WriteInt(section, "style_mode", this->style_mode_);
        store.WriteFloat(section, "spacing", this->spacing_); store.WriteFloat(section, "minimum_rows", this->minimum_rows_);
        store.WriteFloat(section, "icon_size", this->icon_size_); store.WriteFloat(section, "buff_size", this->buff_size_);
        store.WriteFloat(section, "buff_limit", this->buff_limit_); store.WriteFloat(section, "distance_highlight", this->distance_highlight_);
        store.WriteFloat(section, "opacity", this->container_.opacity); store.WriteFloat(section, "padding", this->container_.padding);
        store.WriteArgb(section, "selection_color", this->selection_style_.target_color);
        store.WriteArgb(section, "subtarget_color", this->selection_style_.subtarget_color);
        store.WriteFloat(section, "selection_background_opacity", this->selection_style_.background_opacity);
        store.WriteFloat(section, "selection_padding_x", this->selection_style_.padding_x);
        store.WriteFloat(section, "selection_padding_y", this->selection_style_.padding_y);
        store.WriteBool(section, "selection_arrow", this->selection_style_.arrow);
        store.WriteInt(section, "status_placement", this->status_placement_);
        store.WriteBool(section, "status_container", this->status_frame_);
        store.WriteFloat(section, "status_columns", this->status_columns_);
        store.WriteFloat(section, "status_spacing", this->status_spacing_);
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

    auto party_module::Option(const size_t index) -> module_option
    {
        if (index == icon_follow_option)
        {
            auto option = module_option::Toggle("Icon follows name", "Keep the job icon beside the name as its position and alignment change.", &this->icon_follow_name_);
            option.available = this->job_icons_;
            return option;
        }
        if (index == sync_option) return module_option::Toggle("Show SYNC", "Show the level-sync label beside synced party members.", &this->show_sync_);
        static constexpr const char* layouts[]{"Horizontal", "Compact vertical", "MP below HP, TP left"};
        static constexpr const char* displays[]{"Number", "Percent", "Number (percent)", "Percent (number)", "Current / max", "Hidden"};
        static constexpr const char* bars[]{"HP", "MP", "TP", "Cast"};
        static constexpr const char* styles[]{"All bars", "Per bar"};
        static constexpr const char* texts[]{"Name", "Jobs", "Bar labels", "Bar values", "Details", "Cast", "Heading"};
        static constexpr const char* placements[]{"Left", "Right", "Inside row"};
        static constexpr const char* castPlacements[]{"Separate row", "Name row", "Replace MP", "Replace TP"};
        static constexpr const char* jobPlacements[]{"Name row", "Below name"};
        static constexpr const char* tpDisplays[]{"Bar", "Number only"};
        static constexpr const char* barPositions[]{"Layout", "Left", "Center", "Right"};
        const auto statusOption = [&](module_option option, bool separate = false, bool frame = false) {
            option.available = this->group_ == 0 && this->buffs_ && (!separate || this->status_placement_ != status_inline) && (!frame || this->status_frame_);
            return option;
        };
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show this party list.", &this->state_.enabled);
        case 1: return module_option::Toggle("Show when solo", "Keep your own row while solo.", &this->solo_);
        case 2: return module_option::Toggle("Hide during events", "Hide during cutscenes.", &this->hide_events_);
        case 3: return module_option::Toggle("Full party preview", "Show sample members in edit mode.", &this->preview_);
        case 4: return module_option::Toggle("Show heading", "Show the party or alliance name and member count.", &this->title_);
        case 5: return module_option::Toggle("Distance", "Show known distance in yalms.", &this->distance_);
        case 6: return module_option::Toggle("Job icons", "Original XIVParty job images.", &this->job_icons_);
        case 7: return module_option::Toggle("Job text", "Main job and optional subjob.", &this->jobs_);
        case 8: return module_option::Toggle("Main job level", "Include the main job level.", &this->main_level_);
        case 9: return module_option::Toggle("Subjob", "Include the member's subjob.", &this->sub_job_);
        case 10: return module_option::Toggle("Subjob level", "Include the subjob level.", &this->sub_level_);
        case 11: return module_option::Toggle("Master level", "Include ML when available.", &this->master_level_);
        case 12: return module_option::Toggle("TP", "Show tactical points.", &this->tp_);
        case 13: return module_option::Toggle("Flash ready TP", "Pulse TP text at 1,000 or higher.", &this->flash_tp_);
        case 14: return module_option::Toggle("Always show MP", "Also show an empty MP bar for jobs without MP.", &this->always_mp_);
        case 15: return module_option::Toggle("Status icons", "Native status icons. The game shares these within your party.", &this->buffs_);
        case 16: return module_option::Toggle("Cast bars", "Party and alliance spell progress from action packets.", &this->casts_);
        case 17: return module_option::Toggle("Selection", "Highlight targeted members and subtargets.", &this->selection_);
        case 18: return module_option::Toggle("Align bottom", "Keep the bottom edge fixed as members join or leave.", &this->bottom_);
        case 19: return module_option::Toggle("Expand height", "Reserve all six member rows.", &this->expand_);
        case 20: return module_option::Toggle("Expand in alliance", "Reserve six rows while in an alliance.", &this->expand_alliance_);
        case 21: return module_option::Choice("Resource layout", "Arrange HP, MP and TP, including MP below HP with TP on the left.", &this->layout_, layouts, std::size(layouts), module_option_group::appearance);
        case 22: return module_option::Choice("HP display", "Other members' maximum HP is estimated (~) from percent.", &this->hp_display_, displays, 6);
        case 23: return module_option::Choice("MP display", "Other members' maximum MP is estimated (~) from percent.", &this->mp_display_, displays, 6);
        case 24: return module_option::EditScope("Bar settings", "Share the style or edit each bar independently.", &this->style_mode_, styles, 2);
        case 25: return module_option::Element("Selected bar", "Choose the resource or cast bar to edit.", &this->selected_bar_, bars, 4);
        case 26: return module_option::Element("Selected text", "Choose the text element to edit.", &this->selected_text_, texts, std::size(texts));
        case 27:
        {
            auto option = ModuleWidthOption(this->state_);
            if (this->fit_width_) { option.label = "Maximum width"; option.description = "Limit the fitted container; bar widths determine its actual size."; }
            return option;
        }
        case 28: return module_option::Slider("Entry spacing", "Space between party members.", &this->spacing_, 0, 32, 1, module_option_group::appearance);
        case 29: return module_option::Slider("Minimum rows", "Minimum reserved rows, even in a smaller party.", &this->minimum_rows_, 1, 6, 1, module_option_group::appearance);
        case 30: return module_option::Slider("Job icon size", "Size in pixels.", &this->icon_size_, 20, 64, 1, module_option_group::appearance);
        case 31: return module_option::Slider("Status icon size", "Size in pixels.", &this->buff_size_, 12, 32, 1, module_option_group::appearance);
        case 32: return module_option::Slider("Status limit", "Maximum icons. Any remainder has a +count label.", &this->buff_limit_, 1, 32, 1, module_option_group::appearance);
        case 33: return module_option::Slider("Distance highlight", "Tint nearby distance text; zero disables it.", &this->distance_highlight_, 0, 50, 0.5f, module_option_group::appearance);
        case 34: return module_option::Slider("Opacity", "Native window opacity.", &this->container_.opacity, 0, 1, 0.05f, module_option_group::appearance);
        case 35: return module_option::Slider("Padding", "Space inside the party window.", &this->container_.padding, 4, 24, 1, module_option_group::appearance);
        case 36: return module_option::Color("Ready TP text", "Value tint at 1,000 TP or higher.", &this->ready_tp_, module_option_group::colors);
        case 37: return module_option::Color("Low HP name", "Name tint below 25% health.", &this->low_hp_, module_option_group::colors);
        case 38: return module_option::Color("Nearby distance", "Distance tint inside the chosen range.", &this->range_color_, module_option_group::colors);
        case 39: return module_option::Color("Fill top", "Upper gradient for the selected bar.", &this->fill_tops_[this->selected_bar_], module_option_group::colors);
        case 40: return module_option::Color("Fill bottom", "Lower gradient for the selected bar.", &this->fills_[this->selected_bar_], module_option_group::colors);
        case 41: return statusOption(module_option::Choice("Status placement", "Status effects beside each member or inside the row.", &this->status_placement_, placements, 3));
        case 42: return statusOption(module_option::Toggle("Status container", "Give each member's effects their own themed window.", &this->status_frame_), true);
        case 43: return statusOption(module_option::Slider("Statuses per row", "Wrap effects after this many icons.", &this->status_columns_, 1, 16, 1, module_option_group::appearance));
        case 44: return statusOption(module_option::Slider("Status spacing", "Space between status icons.", &this->status_spacing_, 0, 12, 1, module_option_group::appearance));
        case 45: return statusOption(module_option::Slider("Status gap", "Space between the party window and effects.", &this->status_gap_, 0, 48, 1, module_option_group::appearance), true);
        case 46: return statusOption(module_option::Slider("Status opacity", "Status window background and border opacity.", &this->status_container_.opacity, 0, 1, 0.05f, module_option_group::appearance), true, true);
        case 47: return statusOption(module_option::Slider("Status padding", "Space inside each status container.", &this->status_container_.padding, 0, 16, 1, module_option_group::appearance), true, true);
        case 48:
        {
            auto option = module_option::Choice("Cast placement", "Use a separate row or replace the name, MP or TP while casting.", &this->cast_placement_, castPlacements, 4);
            option.available = this->casts_;
            return option;
        }
        case 49:
        {
            auto option = module_option::Choice("Job placement", "Show job/subjob beside the name or on its own row.", &this->job_placement_, jobPlacements, 2);
            option.available = this->jobs_;
            return option;
        }
        case 50: return module_option::Toggle("Bar labels", "Show HP, MP and TP above their values.", &this->labels_, module_option_group::appearance);
        case 51:
        {
            auto option = module_option::Choice("TP display", "Show TP as a bar or just its number.", &this->tp_display_, tpDisplays, 2);
            option.available = this->tp_;
            return option;
        }
        case 52: return module_option::Slider("Bar width", "Width in pixels; 0 is automatic. Limited to the party content width.",
            &this->bar_widths_[this->selected_bar_], 0, this->BarWidthLimit(), 1, module_option_group::appearance);
        case 53: return module_option::Choice("Bar position", "Keep the layout position or align within the resource row, then apply offsets.",
            &this->bar_positions_[this->selected_bar_], barPositions, std::size(barPositions), module_option_group::appearance);
        case 54: return module_option::Slider("Bar X offset", "Move this bar and its text horizontally after positioning.",
            &this->bar_offsets_x_[this->selected_bar_], -128, 128, 1, module_option_group::appearance);
        case 55: return module_option::Slider("Bar Y offset", "Move this bar and its text vertically after positioning.",
            &this->bar_offsets_y_[this->selected_bar_], -128, 128, 1, module_option_group::appearance);
        case 56: return module_option::Toggle("Fit width to bars", "Size the native container around the bars. Width becomes its maximum.", &this->fit_width_, module_option_group::appearance);
        case 57: return module_option::Color("Target color", "Border, background tint and arrow for the current target.", &this->selection_style_.target_color, module_option_group::colors);
        case 58: return module_option::Color("Subtarget color", "Border, background tint and arrow while choosing a subtarget.", &this->selection_style_.subtarget_color, module_option_group::colors);
        case 59: return module_option::Slider("Background tint", "Selection background opacity; zero hides the fill and keeps the border.", &this->selection_style_.background_opacity, 0, 1, 0.01f, module_option_group::appearance);
        case 60: return module_option::Slider("Horizontal padding", "Space between entry content and the selection frame, in pixels.", &this->selection_style_.padding_x, 0, 16, 1, module_option_group::appearance);
        case 61: return module_option::Slider("Vertical padding", "Space above and below each entry; reserved even when not selected.", &this->selection_style_.padding_y, 0, 12, 1, module_option_group::appearance);
        case 62: return module_option::Toggle("Selection arrow", "Show a matching chevron beside the selected entry.", &this->selection_style_.arrow);
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
            {"General", "Visibility, preview and distance."},
            {"Layout", "Bar width and height, positions, entries and the main party window."},
            {"Jobs", "Job icons, job names and level information."},
            {"Resources", "Choose HP, MP, TP and cast information to display."},
            {"Bar Style", "Choose a bar, then adjust its theme, shape and bookends."},
            {"Animation", "Smooth fill changes and animated sheen for the selected bar."},
            {"Bar Effects", "Shadows and glows for the selected bar."},
            {"Colors", "Resource fills, state colors and bar tints."},
            {"Status Effects", "Icons, placement and containers for your party's effects."},
            {"Text", "Choose a text element, then adjust its style and effects."},
            {"Selection", "Target and subtarget colors, background tint, padding and arrow."}}};
        return sections;
    }

    auto party_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
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
        case 0: case 1: case 2: case 3: case 5: case 33: return section == general_section;
        case 63: return this->group_ != 0 && section == general_section;
        case 17: case 57: case 58: case 59: case 60: case 61: case 62: return section == selection_section;
        case 4: return section == general_section || (section == text_section && this->selected_text_ == heading_text);
        case 18: case 19: case 20: case 21: case 27: case 28: case 29: case 34: case 35: case 56: return section == layout_section;
        case 6: case 7: case 8: case 9: case 10: case 11: case 30: case 49: return section == jobs_section;
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

    auto party_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == 6 || index == 17 || index == 27 || index == 30 || index == 35 || index == 60)
            for (auto& width : this->bar_widths_) width = std::min(width, this->BarWidthLimit());
        if (this->style_mode_ == 0 && (index == 24 || (index >= base_option_count && index < base_option_count + meter_style_option_count)))
            this->meters_.fill(this->meters_[this->selected_bar_]);
        if (this->style_mode_ == 0 && (index == 24 || index == 52))
            this->bar_widths_.fill(this->bar_widths_[this->selected_bar_]);
        if (this->style_mode_ == 0 && (index == 24 || index == 53))
            this->bar_positions_.fill(this->bar_positions_[this->selected_bar_]);
        if (this->style_mode_ == 0 && (index == 24 || index == 54))
            this->bar_offsets_x_.fill(this->bar_offsets_x_[this->selected_bar_]);
        if (this->style_mode_ == 0 && (index == 24 || index == 55))
            this->bar_offsets_y_.fill(this->bar_offsets_y_[this->selected_bar_]);
    }

    auto party_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->ModuleEnabled() && this->state_.enabled && this->source_ != nullptr && this->source_->Available() && this->Count() != 0 &&
            (this->group_ != 0 || this->solo_ || this->Count() > 1 || this->source_->InAlliance()) &&
            (!this->hide_events_ || !this->source_->InCutscene());
    }

    auto party_module::ResourceSlots(const party_member& member) const -> resource_slots
    {
        const bool mp = this->always_mp_ || member.mp_max != 0 || member.mp != 0 || PartyJobHasMP(member.main_job) || PartyJobHasMP(member.sub_job);
        const bool casting = this->casts_ && member.in_zone && member.cast.Active(::GetTickCount64());
        return {mp || (casting && this->cast_placement_ == cast_mp), this->tp_ || (casting && this->cast_placement_ == cast_tp), mp, this->tp_, casting};
    }

    auto party_module::CastHeight(void) const -> float
    {
        return std::max(this->meters_[3].height + 2, LineHeight(this->text_[5]));
    }

    auto party_module::HeaderHeight(const bool casting, const bool details) const -> float
    {
        const float height = std::max({LineHeight(this->text_[0]), details ? LineHeight(this->text_[4]) : 0,
            this->jobs_ && this->job_placement_ == jobs_name ? LineHeight(this->text_[1]) : 0});
        return casting && this->cast_placement_ == cast_name ? std::max(height, this->CastHeight()) : height;
    }

    auto party_module::HeaderPadding(const bool casting) const -> float
    {
        return casting && this->cast_placement_ == cast_name ? std::max(0.0f, this->bar_offsets_y_[3]) : 0;
    }

    auto party_module::ResourceBarHeight(const bool hasMP, const bool hasTP, const bool casting) const -> float
    {
        const bool replaceMP = casting && this->cast_placement_ == cast_mp, replaceTP = casting && this->cast_placement_ == cast_tp;
        const float values = this->hp_display_ != 5 || (hasMP && !replaceMP && this->mp_display_ != 5) || (hasTP && !replaceTP) ? LineHeight(this->text_[3]) : 0;
        const float height = std::max({this->meters_[0].height, hasMP && !replaceMP ? this->meters_[1].height : 0,
            hasTP && !replaceTP && this->tp_display_ == 0 ? this->meters_[2].height : 0, values}) + 2;
        return replaceMP || replaceTP ? std::max(height, this->CastHeight()) : height;
    }

    auto party_module::BarWidthLimit(void) const -> float
    {
        return std::max(1.0f, this->state_.bounds.width - 2 * (this->container_.edge_size + this->container_.padding + this->SelectionPadding().x) -
            (this->job_icons_ ? this->icon_size_ + 7 : 0));
    }

    auto party_module::SelectionPadding(void) const -> ui::point
    {
        return this->selection_ ? ui::point{this->selection_style_.padding_x, this->selection_style_.padding_y} : ui::point{};
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
        return this->PositionBar(3, {bounds.x, bounds.y, this->BarWidth(3, bounds.width), bounds.height}, bounds);
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
        if (this->layout_ == bars_horizontal)
        {
            const float usable = std::max(0.0f, bounds.width - ((slots.mp ? 1 : 0) + (slots.tp ? 1 : 0)) * gap);
            const auto widths = rowWidths(usable, {0.52f, slots.mp ? 0.29f : 0, slots.tp ? 0.19f : 0});
            float x = bounds.x;
            layout.bars[0] = {x, bounds.y, widths[0], height}; x += widths[0] + gap;
            if (slots.mp) { layout.bars[1] = {x, bounds.y, widths[1], height}; x += widths[1] + gap; }
            if (slots.tp) layout.bars[2] = {x, bounds.y, widths[2], height};
        }
        else if (this->layout_ == bars_mp_below_tp_left)
        {
            const float verticalGap = this->labels_ ? LineHeight(this->text_[2]) * 0.5f + 2 : 2;
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
        return layout;
    }

    auto party_module::LayoutContainer(void) const -> container_layout
    {
        const float trim = this->container_.edge_size + this->container_.padding + this->SelectionPadding().x;
        const float icon = this->job_icons_ ? this->icon_size_ + 7 : 0;
        const float maximum = this->BarWidthLimit();
        container_layout result{this->state_.bounds.width, maximum, 0};
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
        float left{}, right = icon + result.canvas_width;
        const auto include = [&](const ui::rect& bar) {
            left = std::min(left, icon + bar.x);
            right = std::max(right, icon + bar.x + bar.width);
        };
        include(resources.bars[0]); include(resources.bars[1]);
        if (tp) include(resources.bars[2]);
        if (this->casts_)
        {
            const auto cast = this->cast_placement_ == cast_mp ? resources.bars[1] : this->cast_placement_ == cast_tp ? resources.bars[2] :
                ui::rect{0, 0, result.canvas_width, this->CastHeight()};
            include(this->CastBounds(cast));
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
        std::snprintf(text, sizeof(text), "[XIUI Native] %s layout=%d; maximum=%.0f; container=%.0f; bars=%.0f; fit=%s; TP=%s; spacing=%.0f. ",
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
        const float width = container.canvas_width + (this->job_icons_ ? this->icon_size_ + 7 : 0);
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
        return LineHeight(this->text_[1]) + 2 * (style.edge_size + style.padding);
    }

    auto party_module::RowHeight(const bool inZone, const size_t statusCount, const bool hasMP, const bool hasTP) const -> float
    {
        return this->StatusHeight(this->EntryHeight(inZone, hasMP, hasTP), inZone, statusCount);
    }

    auto party_module::StatusHeight(const float entry, const bool inZone, const size_t statusCount) const -> float
    {
        if (!this->buffs_ || this->group_ != 0 || !inZone || statusCount == 0) return entry;
        const float width = this->LayoutContainer().canvas_width;
        const auto reserved = this->StatusLayout({0, 0, width, entry}, {}, statusCount);
        return this->status_placement_ == status_inline ? entry + reserved.bounds.height + 5 : std::max(entry, reserved.bounds.height);
    }

    auto party_module::BuffColumns(const float width) const -> size_t
    {
        return static_cast<size_t>(std::max(1.0f, std::min(this->status_columns_, std::floor((width + this->status_spacing_) / (this->buff_size_ + this->status_spacing_)))));
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
        const float height = static_cast<float>(rows) * (this->buff_size_ + this->status_spacing_) - this->status_spacing_;
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
            layout.content.y + static_cast<float>(index / layout.columns) * (this->buff_size_ + this->status_spacing_), this->buff_size_, this->buff_size_};
    }

    auto party_module::DrawStatuses(const party_member& member, const ui::rect& row, const ui::rect& party, const module_render_context& context) const -> void
    {
        if (!this->buffs_ || this->group_ != 0 || !member.in_zone || member.buff_count == 0) return;
        const auto layout = this->StatusLayout(row, party, member.buff_count);
        if (layout.framed) context.DrawContainer(layout.bounds, this->status_container_);
        const auto text = [&](std::string value, size_t index) {
            const auto bounds = this->StatusIconBounds(layout, index);
            value = FitText(context.Design(), std::move(value), bounds.width, this->text_[1].style);
            context.DrawText(value.c_str(), bounds, bounds, this->text_[1]);
        };
        for (size_t i = 0; i < layout.count; ++i)
        {
            auto* texture = context.Icons() != nullptr ? context.Icons()->Status(member.buffs[i]) : nullptr;
            if (texture != nullptr) context.Design().DrawTexture(texture, this->StatusIconBounds(layout, i));
            else text(std::to_string(member.buffs[i]), i);
        }
        if (layout.overflow != 0) text("+" + std::to_string(layout.overflow), layout.count);
    }

    auto party_module::DrawCast(const party_cast& cast, const ui::rect& bounds, const uint64_t now, const module_render_context& context) const -> void
    {
        if (!cast.Active(now)) return;
        const auto bar = this->CastBounds(bounds);
        const float progress = static_cast<float>((cast.interrupted != 0 ? cast.interrupted : now) - cast.started) / (std::max(0.25f, cast.seconds) * 1000);
        auto profile = this->text_[5];
        profile.style.alignment = ui::text_alignment::center;
        auto style = this->meters_[3];
        const auto label = FitText(context.Design(), cast.interrupted != 0 ? "Interrupted" : cast.name,
            context.MeterTextBounds(bar, style).width, profile.style);
        style.animation_enabled = false;
        meter_animation_state animation{};
        context.DrawMeter(bar, "", label.c_str(), std::clamp(progress, 0.0f, 1.0f), this->fill_tops_[3], this->fills_[3], style,
            animation, false, nullptr, &profile);
    }

    auto party_module::DrawRow(const party_member& member, const ui::rect& row, const ui::rect& party, const size_t rowIndex, const uint64_t now, const module_render_context& context) -> void
    {
        const auto& design = context.Design();
        const auto text = [&](std::string value, const ui::rect& bounds, const size_t profile, const std::optional<D3DCOLOR> tint = std::nullopt) {
            const auto style = tint ? TintTextProfile(this->text_[profile], *tint) : this->text_[profile];
            value = FitText(design, std::move(value), bounds.width, style.style);
            context.DrawText(value.c_str(), bounds, bounds, style);
        };
        if (this->selection_ && member.selected)
        {
            const auto padding = this->SelectionPadding();
            context.DrawSelection({row.x - padding.x, row.y - padding.y, row.width + 2 * padding.x, row.height + 2 * padding.y}, member.subtarget, this->selection_style_);
        }
        const auto slots = this->ResourceSlots(member);
        const auto detail = this->DetailText(member);
        const auto entry = this->LayoutEntry(row, member.in_zone, slots, !detail.empty());
        const float x = entry.header.x, width = entry.header.width, headerHeight = entry.header.height;
        const bool casting = slots.casting;
        const float detailWidth = detail.empty() ? 0 : std::min(width * 0.44f, design.MeasureText(detail.c_str(), this->text_[4].style) + 2);
        const auto nameTint = !member.in_zone ? std::optional<D3DCOLOR>{0xFFA8A8A8} : member.hp_percent < 25 ? std::optional{this->low_hp_} : std::nullopt;
        const bool leader = member.leader || member.alliance_leader;
        const float leaderSpace = leader ? 10.0f : 0;
        const float nameWidth = width - detailWidth - 4;
        const auto job = member.in_zone && this->jobs_ && this->job_placement_ == jobs_name ?
            JobText(member, this->main_level_, this->sub_job_, this->sub_level_, this->master_level_) : std::string{};
        const float jobWidth = job.empty() ? 0 : std::min(nameWidth * 0.5f, design.MeasureText(job.c_str(), this->text_[1].style) + 8);
        const ui::rect nameBounds{x + leaderSpace, entry.header.y, std::max(0.0f, nameWidth - jobWidth - leaderSpace), headerHeight};
        const bool nameCast = casting && this->cast_placement_ == cast_name;
        const auto fittedName = FitText(design, member.name, nameBounds.width, this->text_[0].style);
        if (nameCast) this->DrawCast(member.cast, nameBounds, now, context);
        const auto castBounds = this->CastBounds(nameBounds);
        const float jobOffset = (nameCast ? castBounds.x + castBounds.width - x : leaderSpace + design.MeasureText(fittedName.c_str(), this->text_[0].style)) + 6;
        const auto drawIdentity = [&] {
            if (!nameCast) text(fittedName, nameBounds, 0, nameTint);
            const auto visibleName = context.TextBounds(fittedName.c_str(), nameBounds, nameBounds, this->text_[0]);
            if (leader)
            {
                const std::array<ui::slot_dot, 1> dot{{{{visibleName.x - leaderSpace, visibleName.y + (visibleName.height - 6) * 0.5f, 6, 6}, 0xFFFFD84D}}};
                design.DrawSlotDots(dot);
            }
            if (this->job_icons_ && member.in_zone && member.main_job > 0 && member.main_job <= 24 && context.Icons() != nullptr && context.Icons()->JobAtlas() != nullptr)
            {
                const auto icon = member.main_job - 1;
                const auto iconBounds = this->JobIconBounds(entry, visibleName, leaderSpace);
                design.DrawTexture(context.Icons()->JobAtlas(), iconBounds, 0xFFFFFFFF,
                    {(static_cast<float>(icon % 8) * 64 + 0.5f) / 512, (static_cast<float>(icon / 8) * 64 + 0.5f) / 256, 63.0f / 512, 63.0f / 256});
            }
            if (!job.empty()) text(job, {x + jobOffset, entry.header.y, std::max(0.0f, nameWidth - jobOffset), headerHeight}, 1);
            text(detail, {x + width - detailWidth, entry.header.y, detailWidth, headerHeight}, 4,
                member.distance_known && this->distance_highlight_ > 0 && member.distance <= this->distance_highlight_ ? std::optional{this->range_color_} : std::nullopt);
        };
        if (!member.in_zone)
        {
            const auto content = context.DrawContainer(entry.zone, this->ZoneStyle());
            auto profile = this->text_[1];
            profile.style.alignment = ui::text_alignment::center;
            const auto zone = FitText(design, member.zone_name, content.width, profile.style);
            context.DrawText(zone.c_str(), content, content, profile);
            drawIdentity();
            return;
        }
        const bool jobsBelow = this->jobs_ && this->job_placement_ == jobs_below;
        auto& animation = this->animations_[rowIndex];
        if (animation.id != member.id) { animation = {}; animation.id = member.id; }
        const auto meter = [&](const size_t index, const ui::rect& bounds) {
            if (casting && ((index == 1 && this->cast_placement_ == cast_mp) || (index == 2 && this->cast_placement_ == cast_tp)))
            { this->DrawCast(member.cast, bounds, now, context); return; }
            if ((index == 1 && !slots.mp_values) || (index == 2 && !slots.tp_values)) return;
            const auto value = index == 0 ? FormatPartyResource(member.hp, member.hp_percent, member.hp_max, member.hp_max_exact, this->hp_display_) :
                index == 1 ? FormatPartyResource(member.mp, member.mp_percent, member.mp_max, member.mp_max_exact, this->mp_display_) : std::to_string(member.tp);
            const auto ratio = index == 0 ? member.hp_percent / 100.0f : index == 1 ? member.mp_percent / 100.0f : member.tp / 3000.0f;
            auto valueStyle = this->text_[3];
            if (index == 2 && member.tp >= 1000) valueStyle = TintTextProfile(valueStyle, this->ready_tp_);
            if (index == 2 && this->flash_tp_ && member.tp >= 1000)
            {
                const auto alpha = static_cast<uint32_t>(190 + 65 * (std::sin(static_cast<float>(now % 1600) * 6.2831853f / 1600) + 1) * 0.5f);
                valueStyle = TintTextProfile(valueStyle, (alpha << 24) | 0xFFFFFF);
            }
            if (index == 2 && this->tp_display_ == 1)
            {
                if (this->labels_)
                {
                    const auto layout = ui::LayoutMeterText(bounds, LineHeight(this->text_[2]), LineHeight(valueStyle), 32);
                    context.DrawText("TP", layout.label, layout.placement, this->text_[2]);
                }
                const auto fitted = FitText(design, value, bounds.width, valueStyle.style);
                context.DrawText(fitted.c_str(), bounds, bounds, valueStyle);
                return;
            }
            auto fill = this->fills_[index];
            auto fillTop = this->fill_tops_[index];
            if (member.distance_known && member.distance > 20.8f)
            {
                const auto alpha = static_cast<uint32_t>(member.distance > 50 ? 115 : 178);
                fill = (fill & 0xFFFFFF) | (((fill >> 24) * alpha / 255) << 24);
                fillTop = (fillTop & 0xFFFFFF) | (((fillTop >> 24) * alpha / 255) << 24);
            }
            const float textWidth = context.MeterTextBounds(bounds, this->meters_[index]).width;
            const auto fitted = FitText(design, value, textWidth, valueStyle.style);
            const auto label = FitText(design, !this->labels_ ? "" : index == 0 ? "HP" : index == 1 ? "MP" : "TP", textWidth, this->text_[2].style);
            context.DrawMeter(bounds, label.c_str(), fitted.c_str(), ratio,
                fillTop, fill, this->meters_[index], animation.meters[index], true, &this->text_[2], &valueStyle);
        };
        const auto& resources = entry.resources;
        meter(0, resources.bars[0]);
        if (slots.mp) meter(1, resources.bars[1]);
        if (slots.tp) meter(2, resources.bars[2]);
        if (casting && this->cast_placement_ == cast_separate) this->DrawCast(member.cast, entry.cast, now, context);
        const bool inside = this->status_placement_ == status_inline;
        this->DrawStatuses(member, inside ? ui::rect{x, row.y + entry.height + 5, width, 0} : row, party, context);
        if (jobsBelow) text(JobText(member, this->main_level_, this->sub_job_, this->sub_level_, this->master_level_), entry.jobs, 1);
        drawIdentity();
    }

    auto party_module::LayoutRows(const size_t count) const -> party_layout
    {
        party_layout layout;
        const auto container = this->LayoutContainer();
        layout.content_width = container.canvas_width + (this->job_icons_ ? this->icon_size_ + 7 : 0);
        layout.content_inset = container.inset;
        layout.heights.fill(this->RowHeight());
        size_t row{};
        if (this->source_ != nullptr)
            for (size_t i = this->group_ * 6; i < (this->group_ + 1) * 6 && row < count; ++i)
            {
                const auto& member = this->source_->Members()[i];
                if (member.active)
                {
                    layout.heights[row++] = this->MemberHeight(member);
                }
            }
        for (; row < count; ++row)
        {
            party_member sample;
            sample.main_job = preview_jobs[row]; sample.sub_job = 1; sample.mp = row % 3 == 0 ? 0 : 520;
            sample.in_zone = row != preview_remote_row; sample.distance_known = sample.in_zone; sample.sync = row == 1;
            sample.buff_count = preview_status_counts[row];
            if (row == 2) sample.cast = {"Cure IV", 4, ::GetTickCount64() - 1800, 0, 4};
            layout.heights[row] = this->MemberHeight(sample);
        }
        layout.rows = this->expand_ || (this->expand_alliance_ && this->source_ != nullptr && this->source_->InAlliance()) ? 6 :
            std::max(count, static_cast<size_t>(this->minimum_rows_));
        const float heading = this->title_ ? LineHeight(this->text_[heading_text]) + 6 : 0;
        const float padding = this->container_.edge_size + this->container_.padding;
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
        auto bounds = this->PartyBounds(editing && this->preview_ ? 6 : this->Count());
        if (this->buffs_ && this->group_ == 0 && this->status_placement_ != status_inline)
        {
            const float extension = this->StatusLayout({}, bounds, 32).bounds.width + this->status_gap_;
            if (this->status_placement_ == status_left) bounds.x -= extension;
            bounds.width += extension;
        }
        return {bounds.x - 8, bounds.y - 8, bounds.width + 16, bounds.height + 16};
    }

    auto party_module::HitTarget(const float x, const float y) const -> std::pair<uint32_t, uint32_t>
    {
        if (!this->ShouldRender({})) return {};
        const auto layout = this->LayoutRows(this->Count());
        const auto padding = this->SelectionPadding();
        float top = layout.bounds.y + this->container_.edge_size + this->container_.padding + layout.leading_space +
            (this->title_ ? LineHeight(this->text_[heading_text]) + 6 : 0);
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

    auto party_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto now = ::GetTickCount64();
        std::array<const party_member*, 6> members{};
        std::array<party_member, 6> preview{};
        size_t count{};
        if (this->source_ != nullptr)
            for (size_t i = this->group_ * 6; i < (this->group_ + 1) * 6; ++i)
                if (this->source_->Members()[i].active) members[count++] = &this->source_->Members()[i];
        if (context.IsEditing() && this->preview_)
        {
            constexpr const char* previewNames[]{"Player", "White Mage", "Red Mage", "Paladin", "Bard", "Samurai"};
            for (size_t i = count; i < members.size(); ++i)
            {
                auto& member = preview[i]; members[i] = &member; member.active = member.in_zone = member.distance_known = true;
                member.id = static_cast<uint32_t>(0xFFFF0000 + this->group_ * 6 + i); member.slot = static_cast<uint32_t>(this->group_ * 6 + i);
                member.name = i == 0 && snapshot.player_name[0] != '\0' ? snapshot.player_name : previewNames[i];
                member.main_job = preview_jobs[i]; member.main_level = 75; member.sub_job = 1; member.sub_level = 37;
                member.hp = 1270 - static_cast<uint32_t>(i) * 120; member.hp_percent = static_cast<uint8_t>(100 - i * 13);
                member.hp_max = PartyResourceMaximum(member.hp, member.hp_percent); member.mp = i % 3 == 0 ? 0 : 520;
                member.mp_percent = member.mp != 0 ? 68 : 0; member.mp_max = PartyResourceMaximum(member.mp, member.mp_percent);
                member.tp = static_cast<uint32_t>(i) * 480; member.distance = static_cast<float>(i) * 6.2f;
                member.leader = i == 0; member.selected = i == 2; member.sync = i == 1;
                member.buff_count = preview_status_counts[i];
                if (member.buff_count != 0) member.buffs = {33, 40, 43, 116};
                if (i == 2) member.cast = {"Cure IV", 4, now - 1800, 0, 4};
                if (i == preview_remote_row) { member.in_zone = false; member.zone_name = "Ru'Lude Gardens"; member.distance_known = false; }
            }
            count = 6;
        }
        if (count == 0 && !context.IsEditing()) return;
        const auto layout = this->LayoutRows(count);
        const float heading = this->title_ ? LineHeight(this->text_[heading_text]) + 6 : 0;
        const auto& bounds = layout.bounds;
        auto content = context.DrawContainer(bounds, this->container_);
        const auto selectionPadding = this->SelectionPadding();
        content.x += layout.content_inset + selectionPadding.x;
        content.width = layout.content_width;
        float y = content.y;
        if (this->title_)
        {
            const auto& profile = this->text_[heading_text];
            const auto title = std::string{this->DisplayName()} + "  " + std::to_string(count) + "/6";
            context.DrawText(title.c_str(), {content.x, y, content.width, heading}, {content.x, y, content.width, heading}, profile);
            y += heading;
        }
        y += layout.leading_space;
        for (size_t i = 0; i < count; ++i)
        {
            this->DrawRow(*members[i], {content.x, y + selectionPadding.y, content.width, layout.heights[i]}, bounds, i, now, context);
            y += layout.heights[i] + 2 * selectionPadding.y + this->spacing_;
        }
        this->state_.bounds.height = bounds.height + (bounds.y - this->state_.bounds.y);
        this->state_.minimum_height = this->state_.bounds.height;
    }
}
