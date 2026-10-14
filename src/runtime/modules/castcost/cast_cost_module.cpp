#include "cast_cost_module.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* profiles[]{"name", "cost", "timing", "recast_label", "recast_value"};
}

namespace xiui
{
    auto cast_cost_module::ApplyMeterDefaults(const ui::meter_style& style) -> void { this->meter_ = style; this->meter_.height = 10; }

    auto cast_cost_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.meter_text}, module_text_profile{theme.caption_text},
            module_text_profile{theme.meter_text}, module_text_profile{theme.meter_text}};
        constexpr float scale[]{0.9f, 0.85f, 0.78f, 0.8f, 0.8f};
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i].style.scale = scale[i];
        this->text_[4].style.alignment = ui::text_alignment::right;
    }

    auto cast_cost_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        auto* source = this->source_;
        *this = {};
        this->state_.bounds = bounds; this->source_ = source;
        this->ApplyMeterDefaults(theme.meter); this->ApplyTextDefaults(theme);
    }

    auto cast_cost_module::Load(const settings_store& store) -> void
    {
        this->state_.bounds = store.ReadRect("layout.castcost", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"enabled", &this->state_.enabled}, {"show_name", &this->show_name_},
            {"show_cost", &this->show_cost_}, {"show_timing", &this->show_timing_}, {"show_cooldown", &this->show_cooldown_}, {"container", &this->container_}})
            *value = store.ReadBool("castcost", key, *value);
        const auto number = [&](const char* key, float fallback, float maximum) {
            const auto value = store.ReadFloat("castcost", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, 0.0f, maximum);
        };
        this->line_spacing_ = number("line_spacing", 3, 20);
        this->container_style_.opacity = number("opacity", 0.95f, 1);
        this->container_style_.padding = number("padding", 10, 24);
        for (const auto& [key, value] : {std::pair{"fill_top", &this->top_}, {"fill_bottom", &this->bottom_}, {"ready_color", &this->ready_}, {"unavailable_color", &this->unavailable_}})
            *value = store.ReadArgb("castcost", key, *value);
        this->meter_ = LoadMeterStyle(store, "meter.castcost", this->meter_);
        for (size_t i = 0; i < this->text_.size(); ++i)
            this->text_[i] = LoadTextProfile(store, (std::string{"text.castcost."} + profiles[i]).c_str(), this->text_[i]);
        this->selected_text_ = std::clamp(store.ReadInt("castcost", "selected_text", 0), 0, 4);
        this->animation_ = {}; this->last_id_ = UINT32_MAX;
    }

    auto cast_cost_module::Save(const settings_store& store) const -> void
    {
        store.WriteRect("layout.castcost", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"enabled", this->state_.enabled}, {"show_name", this->show_name_},
            {"show_cost", this->show_cost_}, {"show_timing", this->show_timing_}, {"show_cooldown", this->show_cooldown_}, {"container", this->container_}})
            store.WriteBool("castcost", key, value);
        store.WriteFloat("castcost", "line_spacing", this->line_spacing_);
        store.WriteFloat("castcost", "opacity", this->container_style_.opacity); store.WriteFloat("castcost", "padding", this->container_style_.padding);
        for (const auto& [key, value] : {std::pair{"fill_top", this->top_}, {"fill_bottom", this->bottom_}, {"ready_color", this->ready_}, {"unavailable_color", this->unavailable_}})
            store.WriteArgb("castcost", key, value);
        SaveMeterStyle(store, "meter.castcost", this->meter_);
        for (size_t i = 0; i < this->text_.size(); ++i)
            SaveTextProfile(store, (std::string{"text.castcost."} + profiles[i]).c_str(), this->text_[i]);
        store.WriteInt("castcost", "selected_text", this->selected_text_);
    }

    auto cast_cost_module::ConfigSections(void) const -> std::span<const module_config_section> { return BarConfigSections(); }

    auto cast_cost_module::ConfigSectionContains(size_t section, size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= base_options && index < base_options + meter_style_option_count) return topic == MeterConfigSection(index - base_options);
        if (index >= base_options + meter_style_option_count || index == selected_text) return topic == bar_config_section::text;
        if (index >= width && index <= padding) return topic == bar_config_section::layout;
        if (index >= fill_top && index <= unavailable_color) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto cast_cost_module::Option(size_t index) -> module_option
    {
        static constexpr const char* names[]{"Action name", "MP / TP / charges", "Base cast / recast", "Recast label", "Recast value"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show the action highlighted in native magic, ability and mount menus.", &this->state_.enabled);
        case show_name: return module_option::Toggle("Action name", "Show the highlighted action's resource name.", &this->show_name_);
        case show_cost: return module_option::Toggle("Cost", "Show resource MP/TP costs or charge requirements; red when insufficient.", &this->show_cost_);
        case show_timing: return module_option::Toggle("Base cast / recast", "Show base resource times. Equipment and buff adjustments are not included.", &this->show_timing_);
        case show_cooldown: return module_option::Toggle("Live cooldown", "Show the current native recast timer and readiness.", &this->show_cooldown_);
        case width: return ModuleWidthOption(this->state_);
        case line_spacing: return module_option::Slider("Line spacing", "Space between information rows.", &this->line_spacing_, 0, 20, 1);
        case container: return module_option::Toggle("Container", "Use the shared native Window Type container.", &this->container_);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the native frame and backing.", &this->container_style_.opacity, 0, 1, 0.05f);
        case padding: return module_option::Slider("Container padding", "Space inside the native frame.", &this->container_style_.padding, 0, 24, 1);
        case fill_top: return module_option::Color("Fill top", "Top of the cooldown progress gradient.", &this->top_, module_option_group::colors);
        case fill_bottom: return module_option::Color("Fill bottom", "Bottom of the cooldown progress gradient.", &this->bottom_, module_option_group::colors);
        case ready_color: return module_option::Color("Ready", "Ready-state text tint, retaining gradients and effects.", &this->ready_, module_option_group::colors);
        case unavailable_color: return module_option::Color("Insufficient resources", "Tint for insufficient MP, TP or charges.", &this->unavailable_, module_option_group::colors);
        case selected_text: return module_option::Element("Selected text", "Style the name, cost, timing and cooldown text independently.", &this->selected_text_, names, 5, module_option_group::text);
        default:
            if (index < base_options + meter_style_option_count) return MeterStyleOption(index - base_options, this->meter_);
            return TextProfileOption(index - base_options - meter_style_option_count, this->text_[this->selected_text_]);
        }
    }

    auto cast_cost_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->state_.enabled && this->source_ != nullptr && this->source_->State().Valid();
    }

    auto cast_cost_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        static const action_cost_state sample{action_cost_kind::spell, 2, 24, 0, 180, 1250, 0, 0, 0, 1.5f, 5, 2, 5, true, "Cure II"};
        const auto& action = this->source_ != nullptr && this->source_->State().Valid() ? this->source_->State() : sample;
        if (this->last_id_ != action.id || this->last_kind_ != action.kind)
        { this->animation_ = {}; this->last_id_ = action.id; this->last_kind_ = action.kind; }
        const auto lineHeight = [&](size_t index) { return std::max(18.0f, context.Design().TextHeight(this->text_[index].style) + 4); };
        const float inset = this->container_ ? this->container_style_.edge_size + this->container_style_.padding : 0;
        const float meterRow = std::max(12.0f, this->meter_.height) + std::max(lineHeight(3), lineHeight(4)) + 2;
        float height = inset * 2;
        if (this->show_name_) height += lineHeight(0) + this->line_spacing_;
        if (this->show_cost_) height += lineHeight(1) + this->line_spacing_;
        if (this->show_timing_) height += lineHeight(2) + this->line_spacing_;
        if (this->show_cooldown_) height += meterRow;
        this->state_.bounds.height = std::max(24.0f, height);
        const auto& bounds = this->state_.bounds;
        const auto content = this->container_ ? context.DrawContainer(bounds, this->container_style_) : bounds;
        float y = content.y;
        const auto line = [&](const std::string& value, size_t profile, std::optional<D3DCOLOR> tint = std::nullopt) {
            const ui::rect row{content.x, y, content.width, lineHeight(profile)};
            context.DrawText(value.c_str(), row, row, tint ? TintTextProfile(this->text_[profile], *tint) : this->text_[profile]);
            y += row.height + this->line_spacing_;
        };
        if (this->show_name_) line(action.name, 0);
        if (this->show_cost_)
        {
            std::string cost;
            if (action.mp_cost > 0 || action.kind == action_cost_kind::spell) cost = "MP: " + std::to_string(action.mp_cost);
            if (action.tp_cost > 0)
            {
                if (!cost.empty()) cost += "   ";
                cost += "TP: " + std::to_string(action.tp_cost);
                if (action.kind == action_cost_kind::weapon_skill) cost += "+ (current " + std::to_string(action.player_tp) + ")";
            }
            if (action.max_charges > 0) cost = "Charges: " + std::to_string(action.charge_cost) + " (available " + std::to_string(action.charges) + "/" + std::to_string(action.max_charges) + ")";
            if (cost.empty()) cost = "No MP / TP cost";
            line(cost, 1, action.EnoughResources() ? std::nullopt : std::optional{this->unavailable_});
        }
        if (this->show_timing_)
        {
            std::string timing;
            if (action.kind == action_cost_kind::spell) timing = "Base cast: " + FormatCastTime(action.cast_seconds);
            if (action.base_recast > 0)
            {
                if (!timing.empty()) timing += "   ";
                timing += std::string{action.kind == action_cost_kind::spell ? "Recast: " : "Recast base: "} + FormatCastTime(action.base_recast);
            }
            if (timing.empty()) timing = action.kind == action_cost_kind::weapon_skill ? "Requires at least 1000 TP" : "Instant action";
            line(timing, 2);
        }
        if (this->show_cooldown_)
        {
            const auto status = !action.recast_known ? std::string{"Recast unavailable"} : !action.EnoughResources() ? std::string{"Insufficient resources"} :
                action.Ready() ? std::string{"Ready"} + (action.max_charges > 0 && action.remaining > 0 ? " / " + FormatCastTime(action.remaining) : "") : FormatCastTime(action.remaining);
            auto valueText = action.Ready() ? TintTextProfile(this->text_[4], this->ready_) :
                !action.EnoughResources() ? TintTextProfile(this->text_[4], this->unavailable_) : this->text_[4];
            const float ratio = !action.recast_known ? 0 : action.remaining <= 0 ? 1 : action.total > 0 ? std::clamp(1 - action.remaining / action.total, 0.0f, 1.0f) : 0;
            const ui::rect row{content.x, y + 2, content.width, std::max(12.0f, this->meter_.height) + 2};
            context.DrawMeter(row, action.max_charges > 0 ? "Next charge" : "Recast", status.c_str(), ratio, this->top_, this->bottom_, this->meter_, this->animation_, true,
                &this->text_[3], &valueText, ui::meter_text_anchor::bottom);
        }
    }
}
