#include "localization/localization.hpp"
#include "fishing_module.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>
#include <cstdio>

namespace xiui
{
    auto fishing_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->meter_ = style;
        this->meter_.height = 18;
    }

    auto fishing_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.meter_text}, module_text_profile{theme.meter_text}};
        this->text_[0].style.alignment = ui::text_alignment::left;
        this->text_[1].style.alignment = ui::text_alignment::right;
    }

    auto fishing_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        *this = {};
        this->state_.bounds = bounds;
        this->ApplyMeterDefaults(theme.meter); this->ApplyTextDefaults(theme);
    }

    auto fishing_module::Load(const settings_store& store) -> void
    {
        this->state_.bounds = store.ReadRect("layout.fishing", this->state_.bounds);
        this->state_.enabled = store.ReadBool("fishing", "enabled", this->state_.enabled);
        this->show_name_ = store.ReadBool("fishing", "show_name", this->show_name_);
        this->show_percent_ = store.ReadBool("fishing", "show_percent", this->show_percent_);
        this->top_ = store.ReadArgb("fishing", "fill_top", this->top_);
        this->bottom_ = store.ReadArgb("fishing", "fill_bottom", this->bottom_);
        this->meter_ = LoadMeterStyle(store, "meter.fishing", this->meter_);
        this->text_[0] = LoadTextProfile(store, "text.fishing.name", this->text_[0]);
        this->text_[1] = LoadTextProfile(store, "text.fishing.percent", this->text_[1]);
        this->selected_text_ = std::clamp(store.ReadInt("fishing", "selected_text", 0), 0, 1);
        this->animation_ = {};
    }

    auto fishing_module::Save(const settings_store& store) const -> void
    {
        store.WriteRect("layout.fishing", this->state_.bounds);
        store.WriteBool("fishing", "enabled", this->state_.enabled);
        store.WriteBool("fishing", "show_name", this->show_name_);
        store.WriteBool("fishing", "show_percent", this->show_percent_);
        store.WriteArgb("fishing", "fill_top", this->top_); store.WriteArgb("fishing", "fill_bottom", this->bottom_);
        SaveMeterStyle(store, "meter.fishing", this->meter_);
        SaveTextProfile(store, "text.fishing.name", this->text_[0]); SaveTextProfile(store, "text.fishing.percent", this->text_[1]);
        store.WriteInt("fishing", "selected_text", this->selected_text_);
    }

    auto fishing_module::ConfigSections(void) const -> std::span<const module_config_section> { return BarConfigSections(); }

    auto fishing_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= base_options && index < base_options + meter_style_option_count) return topic == MeterConfigSection(index - base_options);
        if (index >= base_options + meter_style_option_count || index == selected_text) return topic == bar_config_section::text;
        if (index == width) return topic == bar_config_section::layout;
        if (index == fill_top || index == fill_bottom) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto fishing_module::ContentOption(const size_t index) -> module_option
    {
        static constexpr const char* profiles[]{"Label", "Stamina percentage"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show the hooked fish's remaining stamina. Fishing controls remain with the game.", &this->state_.enabled).Ordered(-50);
        case show_name: return module_option::Toggle("Label", "Show Fish stamina beside the meter.", &this->show_name_);
        case show_percent: return module_option::Toggle("Percentage", "Show the same remaining stamina percentage as the game's fishing gauge.", &this->show_percent_);
        case width: return ModuleWidthOption(this->state_);
        case fill_top: return module_option::Color("Fill top", "Top of the stamina gradient.", &this->top_, module_option_group::colors).Ordered(500, true);
        case fill_bottom: return module_option::Color("Fill bottom", "Bottom of the stamina gradient.", &this->bottom_, module_option_group::colors).Ordered(501);
        case selected_text: return module_option::Element("Selected text", "Independent label and percentage profiles.", &this->selected_text_, profiles, 2, module_option_group::text);
        default:
            if (index < base_options + meter_style_option_count) return MeterStyleOption(index - base_options, this->meter_);
            return TextProfileOption(index - base_options - meter_style_option_count, this->text_[this->selected_text_])
                .VisibleWhen(this->selected_text_ == 0 ? this->show_name_ : this->show_percent_);
        }
    }

    auto fishing_module::ShouldRender(const hud_snapshot& snapshot) const -> bool
    {
        return this->state_.enabled && snapshot.fishing_stamina_percent >= 0 && snapshot.fishing_stamina_percent <= 100;
    }

    auto fishing_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const int stamina = snapshot.fishing_stamina_percent >= 0 && snapshot.fishing_stamina_percent <= 100 ? snapshot.fishing_stamina_percent : 65;
        char percentage[16]{};
        if (this->show_percent_) std::snprintf(percentage, sizeof(percentage), "%d%%", stamina);
        const float textHeight = std::max({18.0f, context.Design().TextHeight(this->text_[0].style), context.Design().TextHeight(this->text_[1].style)});
        this->state_.bounds.height = this->state_.minimum_height = std::max(18.0f, this->meter_.height) + textHeight + 8;
        const auto& bounds = this->state_.bounds;
        const ui::rect row{bounds.x, bounds.y + 4, bounds.width, std::max(18.0f, this->meter_.height) + 2};
        if (::GetTickCount64() - this->animation_.last_tick > 250) this->animation_ = {};
        context.DrawMeter(row, this->show_name_ ? i18n::Text("Fish stamina") : "", percentage, stamina / 100.0f,
            this->top_, this->bottom_, this->meter_, this->animation_, true, &this->text_[0], &this->text_[1], ui::meter_text_anchor::bottom);
    }
}
