#include "zone_name_module.hpp"
#include "settings/text_style_settings.hpp"
#include "ui/font_catalog.hpp"
#include "ui/text_size.hpp"

#include <cmath>

namespace
{
    // "text.zonename.name" held values an earlier build took from global typography; it is never read.
    constexpr const char* text_section = "text.zonename.title";
    constexpr float slide = 0.6f;
    constexpr const char* sample_name = "Ru'Lude Gardens";

    auto ScaleAlpha(const D3DCOLOR color, const float alpha) -> D3DCOLOR
    { return (color & 0x00FFFFFF) | (static_cast<uint32_t>(static_cast<float>(color >> 24) * alpha + 0.5f) << 24); }
}

namespace xiui
{
    auto zone_name_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {theme.title_text};
        auto& style = this->text_.style;
        style.font = ui::FontFamilyIndex(ui::highwind_family); style.font_explicit = true;
        // The XIV 1.0 theme would otherwise force its stroke and shadow over the saved toggles.
        style.theme_text = false;
        style.weight_choice = ui::text_weight_regular;
        style.scale = ui::text_max_scale;
        style.alignment = ui::text_alignment::center;
    }

    auto zone_name_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        *this = {};
        this->state_.bounds = bounds; this->source_ = source;
        this->ApplyTextDefaults(theme);
    }

    auto zone_name_module::Replay(void) -> void
    {
        if (this->name_.empty()) this->name_ = sample_name;
        this->playing_ = true; this->started_ = this->now_;
    }

    auto zone_name_module::Update(const uint64_t now, const bool configOpen) -> void
    {
        this->now_ = now;
        const auto* local = this->source_ != nullptr && this->source_->Available() ? &this->source_->Members()[0] : nullptr;
        const auto zone = local != nullptr && local->active && local->in_zone ? local->zone : uint16_t{};
        // The source clears while zoning, so the name plays once the new zone is ready.
        if (zone != 0 && zone != this->zone_ && !local->zone_name.empty())
        {
            this->zone_ = zone; this->name_ = local->zone_name;
            this->playing_ = this->state_.enabled; this->started_ = now;
        }
        if (!this->state_.enabled) this->playing_ = false;
        if (this->playing_ && !this->Frame().visible) this->playing_ = false;
        this->preview_ = configOpen;
    }

    auto zone_name_module::Frame(void) const -> zone_name_frame
    { return ZoneNameFrame(this->now_ >= this->started_ ? this->now_ - this->started_ : 0, this->fade_in_, this->hold_, this->fade_out_); }

    auto zone_name_module::Load(const settings_store& settings) -> void
    {
        this->state_.bounds = settings.ReadRect("layout.zonename", this->state_.bounds);
        this->state_.enabled = settings.ReadBool("zonename", "enabled", this->state_.enabled);
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = settings.ReadFloat("zonename", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->fade_in_ = number("fade_in", this->fade_in_, 0.1f, 3);
        this->hold_ = number("hold", this->hold_, 0.5f, 15);
        this->fade_out_ = number("fade_out", this->fade_out_, 0.1f, 5);
        // The banner keeps its own face and size instead of taking the global HUD text settings.
        const auto defaults = this->text_;
        this->text_ = LoadTextProfile(settings, text_section, defaults);
        this->text_.style = LoadTextStyle(settings, text_section, defaults.style);
        this->text_.style.top_color = settings.ReadArgb(text_section, "top_color", defaults.style.top_color);
        this->text_.style.bottom_color = settings.ReadArgb(text_section, "bottom_color", defaults.style.bottom_color);
    }

    auto zone_name_module::Save(const settings_store& settings) const -> void
    {
        settings.WriteRect("layout.zonename", this->state_.bounds);
        settings.WriteBool("zonename", "enabled", this->state_.enabled);
        settings.WriteFloat("zonename", "fade_in", this->fade_in_);
        settings.WriteFloat("zonename", "hold", this->hold_);
        settings.WriteFloat("zonename", "fade_out", this->fade_out_);
        SaveTextProfile(settings, text_section, this->text_);
    }

    auto zone_name_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Announce each zone you enter with its name.", "general"},
            {"Animation", "Set how long the name takes to slide in, stay and slide out.", "animation"},
            {"Text", "Font, size, colors and effects of the zone name.", "text"}};
        return sections;
    }

    auto zone_name_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= base_options) return section == 2;
        if (index >= fade_in && index <= fade_out) return section == 1;
        return section == 0;
    }

    auto zone_name_module::ContentOption(const size_t index) -> module_option
    {
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show the zone's name for a moment when you arrive.", &this->state_.enabled).Ordered(-50);
        case width: return ModuleWidthOption(this->state_);
        case fade_in: return module_option::Slider("Entrance", "Seconds the name takes to fade in while sliding up.", &this->fade_in_, 0.1f, 3, 0.1f);
        case hold: return module_option::Slider("Hold time", "Seconds the name stays fully visible.", &this->hold_, 0.5f, 15, 0.5f);
        case fade_out: return module_option::Slider("Exit", "Seconds the name takes to fade out while sliding down.", &this->fade_out_, 0.1f, 5, 0.1f);
        case preview: return module_option::Action("Preview zone name", "Replay the animation with the current zone.", "Preview");
        default: return TextProfileOption(index - base_options, this->text_);
        }
    }

    auto zone_name_module::OnContentOptionChanged(const size_t index) -> void
    { if (index == preview || (index >= fade_in && index <= fade_out)) this->Replay(); }

    auto zone_name_module::ShouldRender(const hud_snapshot&) const -> bool
    { return this->state_.enabled && (this->playing_ || this->preview_); }

    auto zone_name_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        // Edit mode and the open settings page hold the finished name between replays.
        const auto frame = this->playing_ ? this->Frame() : context.IsEditing() || this->preview_ ? zone_name_frame{true, 1, 0} : zone_name_frame{};
        const auto& design = context.Design();
        const auto textHeight = design.TextHeight(this->text_.style);
        this->state_.bounds.height = textHeight;
        if (!frame.visible) return;
        auto profile = this->text_;
        auto& style = profile.style;
        for (auto* color : {&style.top_color, &style.bottom_color, &style.shadow.color, &style.stroke.color, &style.glow.color, &style.sheen.color})
            *color = ScaleAlpha(*color, frame.alpha);
        auto bounds = this->state_.bounds;
        bounds.y += textHeight * slide * frame.offset;
        const auto* name = this->name_.empty() ? sample_name : this->name_.c_str();
        context.DrawText(name, bounds, bounds, profile);
    }
}
