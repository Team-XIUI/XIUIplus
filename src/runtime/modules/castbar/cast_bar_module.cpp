#include "cast_bar_module.hpp"
#include "modules/bar_config.hpp"
#include "modules/target/xiv1_cast_chrome.hpp"

#include <algorithm>
#include <cstdio>

namespace xiui
{
    auto cast_bar_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->meter_ = style;
        this->meter_.height = 18;
    }

    auto cast_bar_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.meter_text}, module_text_profile{theme.meter_text}};
        this->text_[0].style.alignment = ui::text_alignment::left;
        this->text_[1].style.alignment = ui::text_alignment::right;
    }

    auto cast_bar_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        auto* source = this->source_;
        *this = {};
        this->state_.bounds = bounds; this->source_ = source;
        this->ApplyMeterDefaults(theme.meter); this->ApplyTextDefaults(theme);
    }

    auto cast_bar_module::Load(const settings_store& store) -> void
    {
        this->fastcast_.Load(store);
        this->state_.bounds = store.ReadRect("layout.castbar", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"enabled", &this->state_.enabled}, {"show_name", &this->show_name_},
            {"show_percent", &this->show_percent_}, {"show_items", &this->show_items_}, {"show_interrupted", &this->show_interrupted_}})
            *value = store.ReadBool("castbar", key, *value);
        this->top_ = store.ReadArgb("castbar", "fill_top", this->top_);
        this->bottom_ = store.ReadArgb("castbar", "fill_bottom", this->bottom_);
        this->interrupted_ = store.ReadArgb("castbar", "interrupted_color", this->interrupted_);
        this->meter_ = LoadMeterStyle(store, "meter.castbar", this->meter_);
        this->text_[0] = LoadTextProfile(store, "text.castbar.name", this->text_[0]);
        this->text_[1] = LoadTextProfile(store, "text.castbar.percent", this->text_[1]);
        this->selected_text_ = std::clamp(store.ReadInt("castbar", "selected_text", 0), 0, 1);
        this->animation_ = {}; this->cast_started_ = 0;
    }

    auto cast_bar_module::Save(const settings_store& store) const -> void
    {
        this->fastcast_.Save(store);
        store.WriteRect("layout.castbar", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"enabled", this->state_.enabled}, {"show_name", this->show_name_},
            {"show_percent", this->show_percent_}, {"show_items", this->show_items_}, {"show_interrupted", this->show_interrupted_}})
            store.WriteBool("castbar", key, value);
        store.WriteArgb("castbar", "fill_top", this->top_); store.WriteArgb("castbar", "fill_bottom", this->bottom_);
        store.WriteArgb("castbar", "interrupted_color", this->interrupted_);
        SaveMeterStyle(store, "meter.castbar", this->meter_);
        SaveTextProfile(store, "text.castbar.name", this->text_[0]); SaveTextProfile(store, "text.castbar.percent", this->text_[1]);
        store.WriteInt("castbar", "selected_text", this->selected_text_);
    }

    auto cast_bar_module::ConfigSections(void) const -> std::span<const module_config_section> { return BarConfigSections(); }

    auto cast_bar_module::ContentConfigSectionContains(size_t section, size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= base_options && index < base_options + meter_style_option_count) return topic == MeterConfigSection(index - base_options);
        if (index >= base_options + meter_style_option_count || index == selected_text) return topic == bar_config_section::text;
        if (index == width) return topic == bar_config_section::layout;
        if (index >= fill_top && index <= interrupted_color) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto cast_bar_module::ContentOption(size_t index) -> module_option
    {
        static constexpr const char* profiles[]{"Spell / item name", "Progress"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show your own cast progress independently of the Party module.", &this->state_.enabled).Ordered(-50);
        case show_name: return module_option::Toggle("Action name", "Show the spell or item being used.", &this->show_name_);
        case show_percent: return module_option::Toggle("Percentage", "Show the client's current cast percentage.", &this->show_percent_);
        case show_items: return module_option::Toggle("Item use", "Include item-use progress as well as spell casts.", &this->show_items_);
        case show_interrupted: return module_option::Toggle("Interruptions", "Briefly retain an interrupted cast with its interruption color.", &this->show_interrupted_);
        case fastcast_enabled: return module_option::Toggle("Manual Fast Cast / true progress", "Adjust displayed spell progress using your configured reductions. Does not change actual casting speed.", &this->fastcast_.enabled);
        case fastcast_job: return module_option::Element("Fast Cast job", "Choose the main job whose total Fast Cast reduction to edit.", &this->selected_job_, cast_job_names, 22).VisibleWhen(this->fastcast_.enabled);
        case fastcast_amount: return module_option::Slider("Fast Cast percent", "Total Fast Cast for this main job, including equipment and traits.", &this->fastcast_.jobs[this->selected_job_], 0, 80, 1).VisibleWhen(this->fastcast_.enabled);
        case fastcast_rdm_sub: return module_option::Slider("RDM subjob percent", "Additional Fast Cast when your subjob is RDM; combined reduction is capped at 80%.", &this->fastcast_.rdm_sub, 0, 80, 1).VisibleWhen(this->fastcast_.enabled);
        case fastcast_cure: return module_option::Slider("WHM Cure speed percent", "Cure/Curaga cast-time reduction, multiplied with WHM Fast Cast.", &this->fastcast_.cure, 0, 80, 1).VisibleWhen(this->fastcast_.enabled);
        case fastcast_singing: return module_option::Slider("BRD singing speed percent", "Singing cast-time reduction, multiplied with BRD Fast Cast.", &this->fastcast_.singing, 0, 80, 1).VisibleWhen(this->fastcast_.enabled);
        case fastcast_summoning: return module_option::Slider("SMN summoning speed percent", "Summoning cast-time reduction, multiplied with SMN Fast Cast.", &this->fastcast_.summoning, 0, 80, 1).VisibleWhen(this->fastcast_.enabled);
        case width: return ModuleWidthOption(this->state_);
        case fill_top: return module_option::Color("Fill top", "Top of the cast progress gradient.", &this->top_, module_option_group::colors).Ordered(500, true);
        case fill_bottom: return module_option::Color("Fill bottom", "Bottom of the cast progress gradient.", &this->bottom_, module_option_group::colors).Ordered(501);
        case interrupted_color: return module_option::Color("Interrupted", "Tint for interrupted progress and text, preserving text effects.", &this->interrupted_, module_option_group::colors).VisibleWhen(this->show_interrupted_);
        case selected_text: return module_option::Element("Selected text", "Independent name and percentage profiles.", &this->selected_text_, profiles, 2, module_option_group::text);
        default:
            if (index < base_options + meter_style_option_count) return MeterStyleOption(index - base_options, this->meter_);
            return TextProfileOption(index - base_options - meter_style_option_count, this->text_[this->selected_text_]).VisibleWhen(
                this->selected_text_ == 0 ? this->show_name_ || this->show_interrupted_ : this->show_percent_);
        }
    }

    auto cast_bar_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        if (!this->state_.enabled || this->source_ == nullptr || !this->source_->State().Active()) return false;
        const auto& cast = this->source_->State();
        return (!cast.item || this->show_items_) && (cast.interrupted == 0 || this->show_interrupted_);
    }

    auto cast_bar_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const auto* cast = this->source_ != nullptr && this->source_->State().Active() ? &this->source_->State() : nullptr;
        const auto started = cast != nullptr ? cast->started : 0;
        if (started != this->cast_started_) { this->animation_ = {}; this->cast_started_ = started; }
        const bool interrupted = cast != nullptr && cast->interrupted != 0;
        const float progress = cast != nullptr ? (cast->item ? cast->progress : this->fastcast_.Progress(cast->progress, this->source_->MainJob(), this->source_->SubJob(), cast->id, cast->skill)) : 0.55f;
        const char* name = interrupted ? "Interrupted" : cast != nullptr ? cast->name.c_str() : "Cure III";
        char percentage[16]{};
        if (this->show_percent_) std::snprintf(percentage, sizeof(percentage), "%.0f%%", progress * 100);
        const float textHeight = std::max({18.0f, context.Design().TextHeight(this->text_[0].style), context.Design().TextHeight(this->text_[1].style)});
        this->state_.bounds.height = this->state_.minimum_height = std::max(18.0f, this->meter_.height) + textHeight + 8;
        const auto& bounds = this->state_.bounds;
        const ui::rect row{bounds.x, bounds.y + 4, bounds.width, std::max(18.0f, this->meter_.height) + 2};
        auto nameText = interrupted ? TintTextProfile(this->text_[0], this->interrupted_) : this->text_[0];
        auto progressText = interrupted ? TintTextProfile(this->text_[1], this->interrupted_) : this->text_[1];
        if (context.Design().IsXiv1Theme())
        {
            ui::game_icon_view icon;
            if (context.Icons() != nullptr)
                icon = context.Icons()->ActionIcon(cast != nullptr ? (cast->item ? "item:" : "spell:") + std::to_string(cast->id) : "spell:3");
            this->state_.bounds.height = this->state_.minimum_height = 56;
            const ui::rect frame{bounds.x, bounds.y + 2, bounds.width, 52};
            DrawXiv1Cast(context, frame, this->show_name_ || interrupted ? name : "", progress,
                interrupted ? this->interrupted_ : 0xFFEDCAB5, interrupted ? this->interrupted_ : 0xFFB98370, this->meter_, nameText, icon);
            if (this->show_percent_)
            {
                progressText.style.scale *= .8f;
                const ui::rect value{frame.x + frame.width - 70, frame.y + 45, 65, context.Design().TextHeight(progressText.style)};
                context.DrawText(percentage, value, value, progressText);
            }
            return;
        }
        context.DrawMeter(row, this->show_name_ || interrupted ? name : "", percentage, progress,
            interrupted ? this->interrupted_ : this->top_, interrupted ? this->interrupted_ : this->bottom_, this->meter_, this->animation_, true,
            &nameText, &progressText, ui::meter_text_anchor::bottom);
    }
}
