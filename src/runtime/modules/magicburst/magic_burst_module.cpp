#include "magic_burst_module.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    auto magic_burst_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.meter_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}};
        this->text_[0].style.scale = 1.6f;
        this->text_[1].style.scale = this->text_[2].style.scale = 0.8f;
        for (auto& profile : this->text_) profile.style.alignment = ui::text_alignment::center;
    }
    auto magic_burst_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds; const auto* source = this->source_;
        *this = {}; this->state_.bounds = bounds; this->source_ = source; this->ApplyTextDefaults(theme);
    }
    auto magic_burst_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("magicburst", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.magicburst", this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, 64.0f, 384.0f);
        this->opacity_ = std::clamp(store.ReadFloat("magicburst", "opacity", 1), 0.0f, 1.0f);
        this->artwork_ = store.ReadBool("magicburst", "artwork", true);
        this->pulse_ = store.ReadBool("magicburst", "pulse", true);
        this->fade_in_ = store.ReadBool("magicburst", "fade_in", true);
        this->timer_ = store.ReadBool("magicburst", "timer", true);
        this->chain_name_ = store.ReadBool("magicburst", "chain_name", true);
        this->elements_ = store.ReadBool("magicburst", "elements", false);
        this->element_tint_ = store.ReadBool("magicburst", "element_tint", true);
        this->selected_text_ = std::clamp(store.ReadInt("magicburst", "selected_text", 0), 0, 2);
        constexpr const char* sections[]{"text.magicburst.timer", "text.magicburst.chain", "text.magicburst.elements"};
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, sections[i], this->text_[i]);
    }
    auto magic_burst_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("magicburst", "enabled", this->state_.enabled); store.WriteRect("layout.magicburst", this->state_.bounds);
        store.WriteFloat("magicburst", "opacity", this->opacity_); store.WriteBool("magicburst", "artwork", this->artwork_);
        store.WriteBool("magicburst", "pulse", this->pulse_); store.WriteBool("magicburst", "fade_in", this->fade_in_);
        store.WriteBool("magicburst", "timer", this->timer_); store.WriteBool("magicburst", "chain_name", this->chain_name_);
        store.WriteBool("magicburst", "elements", this->elements_); store.WriteBool("magicburst", "element_tint", this->element_tint_);
        store.WriteInt("magicburst", "selected_text", this->selected_text_);
        constexpr const char* sections[]{"text.magicburst.timer", "text.magicburst.chain", "text.magicburst.elements"};
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, sections[i], this->text_[i]);
    }
    auto magic_burst_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Show the selected enemy's observed burst window. Prediction controls and server rules are in Action Bars / Skillchains.", "general"},
            {"Appearance", "Elemental burst artwork, size, opacity and urgency animation.", "appearance"},
            {"Text", "Independent countdown, chain-name and element-list text profiles.", "text"}};
        return sections;
    }
    auto magic_burst_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        if (index == enabled) return section == 0;
        if (index >= timer) return section == 2;
        return section == 1;
    }
    auto magic_burst_module::Option(const size_t index) -> module_option
    {
        static constexpr const char* profiles[]{"Countdown", "Chain name", "Elements"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Magic Burst HUD", "Show a burst window for your selected enemy after an observed skillchain.", &this->state_.enabled).Ordered(-50);
        case size: return module_option::Slider("Size", "Burst artwork size; preview and normal movement share the same anchor.", &this->state_.bounds.width, 64, 384, 1);
        case opacity: return module_option::Slider("Opacity", "Opacity of the artwork and styled text.", &this->opacity_, 0, 1, 0.05f);
        case artwork: return module_option::Toggle("Element artwork", "Show elemental orbs, splitting two-element chains down the center.", &this->artwork_);
        case pulse: return module_option::Toggle("Urgency pulse", "Pulse and gently grow the artwork during the last three seconds.", &this->pulse_);
        case fade_in: return module_option::Toggle("Fade in", "Fade in over the first 0.2 seconds of the observed burst window.", &this->fade_in_);
        case timer: return module_option::Toggle("Countdown", "Show estimated seconds remaining in the observed burst window.", &this->timer_);
        case chain_name: return module_option::Toggle("Chain name", "Show the observed skillchain name beneath the artwork.", &this->chain_name_);
        case elements: return module_option::Toggle("Burst elements", "List the spell elements that can burst, including all four for Light/Darkness.", &this->elements_);
        case element_tint: return module_option::Toggle("Element text tint", "Tint the chain name while preserving the profile's gradient and effects.", &this->element_tint_);
        case selected_text: return module_option::Element("Selected text", "Choose which text element to configure.", &this->selected_text_, profiles, 3);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]);
        }
    }
    auto magic_burst_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        const auto now = ::GetTickCount64(); const auto* window = this->source_ != nullptr ? this->source_->Current(now) : nullptr;
        return this->state_.enabled && window != nullptr && window->Burst(now);
    }
    auto magic_burst_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        if (!this->state_.enabled) return;
        const auto now = ::GetTickCount64();
        const auto* window = this->source_ != nullptr ? this->source_->Current(now) : nullptr;
        auto chain = window != nullptr && window->Burst(now) ? window->chain : skillchain_property::none;
        float remaining = window != nullptr && window->Burst(now) ? static_cast<float>(window->closes - now) / 1000 : 0;
        float elapsed = window != nullptr && window->Burst(now) ? static_cast<float>(now - window->burst_started) / 1000 : 0;
        if (context.IsEditing()) { chain = skillchain_property::distortion; remaining = 9 - static_cast<float>(now % 9000) / 1000; elapsed = 9 - remaining; }
        if (chain == skillchain_property::none || remaining <= 0) return;
        const auto& design = context.Design();
        const float side = this->state_.bounds.width;
        const float nameHeight = this->chain_name_ ? std::max(18.0f, design.TextHeight(this->text_[1].style) + 4) : 0;
        const float elementLineHeight = std::max(18.0f, design.TextHeight(this->text_[2].style) + 4);
        std::vector<std::string> elementLines;
        if (this->elements_)
        {
            static constexpr const char* names[]{"Fire", "Ice", "Wind", "Earth", "Lightning", "Water", "Light", "Dark"};
            std::string line;
            for (uint8_t i = 0; i < 8; ++i) if (SkillchainElements(chain) & (1u << i))
            {
                auto candidate = line.empty() ? std::string{names[i]} : line + " / " + names[i];
                if (!line.empty() && design.MeasureText(candidate.c_str(), this->text_[2].style) > side)
                { elementLines.push_back(std::move(line)); line = names[i]; }
                else line = std::move(candidate);
            }
            if (!line.empty()) elementLines.push_back(std::move(line));
        }
        const float elementHeight = elementLineHeight * static_cast<float>(elementLines.size());
        this->state_.bounds.height = side + nameHeight + elementHeight;
        float alpha = this->opacity_ * (this->fade_in_ ? std::clamp(elapsed / 0.2f, 0.0f, 1.0f) : 1);
        float growth = 1;
        if (this->pulse_ && remaining < 3)
        {
            const float wave = (std::sin(static_cast<float>(now % 100000) / 1000 * 3.5f * 6.2831853f) + 1) / 2;
            const float intensity = (1 - remaining / 3) * wave;
            alpha *= 1 - 0.55f * intensity; growth += 0.1f * intensity;
        }
        const auto tint = static_cast<uint32_t>(std::clamp(alpha, 0.0f, 1.0f) * 255) << 24 | 0xFFFFFF;
        const ui::rect square{this->state_.bounds.x, this->state_.bounds.y, side, side};
        const float extra = side * (growth - 1) / 2;
        const ui::rect art{square.x - extra, square.y - extra, side + extra * 2, side + extra * 2};
        uint8_t mask = SkillchainElements(chain);
        if (chain == skillchain_property::light || chain == skillchain_property::light2 || chain == skillchain_property::radiance) mask = 1u << 6;
        if (chain == skillchain_property::darkness || chain == skillchain_property::darkness2 || chain == skillchain_property::umbra) mask = 1u << 7;
        std::array<uint8_t, 2> orb{}; size_t count{};
        for (uint8_t i = 0; i < 8 && count < 2; ++i) if (mask & (1u << i)) orb[count++] = i;
        if (this->artwork_ && context.Icons() != nullptr)
            for (size_t i = 0; i < count; ++i)
            {
                const auto icon = context.Icons()->MagicBurstElement(orb[i]);
                const auto clip = design.Clip(count == 2 ? ui::rect{art.x + static_cast<float>(i) * art.width / 2, art.y, art.width / 2, art.height} : art);
                if (icon.texture != nullptr) design.DrawTexture(icon.texture, art, tint, icon.uv);
            }
        if (this->artwork_ && count == 2)
        {
            const auto seamAlpha = static_cast<uint32_t>(alpha * 96) << 24;
            design.FillRect({art.x + art.width / 2 - 2, art.y + 4, 4, art.height - 8}, seamAlpha);
        }
        if (this->timer_)
        {
            char seconds[24]{}; std::snprintf(seconds, sizeof(seconds), "%.1f", remaining);
            auto profile = TintTextProfile(this->text_[0], tint); profile.style.tabular_numbers = true;
            context.DrawText(seconds, square, square, profile);
        }
        if (this->chain_name_)
        {
            auto profile = this->element_tint_ ? TintTextProfile(this->text_[1], SkillchainColor(chain)) : this->text_[1];
            profile = TintTextProfile(profile, tint);
            const ui::rect line{square.x, square.y + side, side, nameHeight};
            context.DrawText(SkillchainName(chain), line, line, profile);
        }
        for (size_t i = 0; i < elementLines.size(); ++i)
        {
            const ui::rect line{square.x, square.y + side + nameHeight + static_cast<float>(i) * elementLineHeight, side, elementLineHeight};
            context.DrawText(elementLines[i].c_str(), line, line, TintTextProfile(this->text_[2], tint));
        }
    }
}
