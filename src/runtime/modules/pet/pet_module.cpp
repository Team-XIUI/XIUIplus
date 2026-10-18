#include "pet_module.hpp"
#include "ui/health_colors.hpp"
#include "modules/bar_config.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    constexpr const char* resource_names[]{"HP", "MP", "TP"};
    constexpr const char* text_names[]{"Name", "Values", "Details", "Status timers", "Recasts"};
    constexpr const char* text_keys[]{"name", "values", "details", "status", "recasts"};
    constexpr uint8_t pet_jobs[]{15, 9, 9, 14, 18};
}

namespace xiui
{
    auto pet_module::portrait_settings::Load(const settings_store& store, const std::string& section) -> void
    {
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = store.ReadFloat(section.c_str(), key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->scale = number("scale", this->scale, 0.1f, 4);
        this->opacity = number("opacity", this->opacity, 0, 1);
        this->offset_x = number("offset_x", this->offset_x, -600, 600);
        this->offset_y = number("offset_y", this->offset_y, -600, 600);
        this->clip = store.ReadBool(section.c_str(), "clip", this->clip);
    }
    auto pet_module::portrait_settings::Save(const settings_store& store, const std::string& section) const -> void
    {
        store.WriteFloat(section.c_str(), "scale", this->scale);
        store.WriteFloat(section.c_str(), "opacity", this->opacity);
        store.WriteFloat(section.c_str(), "offset_x", this->offset_x);
        store.WriteFloat(section.c_str(), "offset_y", this->offset_y);
        store.WriteBool(section.c_str(), "clip", this->clip);
    }
    auto pet_module::portrait_settings::Bounds(const ui::rect& anchor, const float size) const -> ui::rect
    { return {anchor.x + this->offset_x, anchor.y + this->offset_y, size * this->scale, size * this->scale}; }
    auto pet_module::PortraitStyle(const pet_kind kind, const std::string_view name) -> portrait_settings&
    {
        const auto index = ui::PetPortraitIndex(name, static_cast<int>(kind));
        if (kind == pet_kind::avatar && index >= 0) return this->avatar_portraits_[static_cast<size_t>(index)];
        return this->types_[static_cast<size_t>(kind)].portrait_style;
    }
    auto pet_module::DefaultPortrait(const size_t index) -> portrait_settings
    {
        static constexpr portrait_settings presets[]{
            {1.65f, .65f, 173, -77, true}, // Carbuncle
            {1.65f, .65f, 166, -3, true}, // Ifrit
            {1.65f, .65f, 168, 8, true}, // Shiva
            {1.65f, .65f, 172, -36, true}, // Garuda
            {1.5f, .65f, 166, 3, true}, // Titan
            {1.65f, .65f, 169, 4, true}, // Ramuh
            {1.65f, .65f, 154, -18, true}, // Leviathan
            {1.65f, .65f, 173, -46, true}, // Fenrir
            {1.65f, .65f, 173, -16, true}, // Diabolos
            {1.65f, .65f, 173, -58, true}, // Atomos
            {1.65f, .65f, 170, -40, true}, // Odin
            {1.45f, .65f, 170, -45, true}, // Alexander
            {1.65f, .65f, 173, -55, true}, // Cait Sith
            {1.65f, .65f, 173, -16, true}, // Siren
            {1.65f, .65f, 173, -55, true}, // Fire Spirit
            {1.65f, .65f, 173, -60, true}, // Ice Spirit
            {1.65f, .65f, 173, -60, true}, // Air Spirit
            {1.65f, .65f, 173, -60, true}, // Earth Spirit
            {1.65f, .65f, 173, -60, true}, // Thunder Spirit
            {1.65f, .65f, 173, -60, true}, // Water Spirit
            {1.65f, .65f, 173, -60, true}, // Light Spirit
            {1.65f, .65f, 173, -70, true}}; // Dark Spirit
        static_assert(std::size(presets) == std::size(ui::avatar_portrait_keys));
        return presets[index];
    }
    auto pet_module::ResetPortraitDefaults(void) -> void
    {
        for (size_t index = 0; index < this->avatar_portraits_.size(); ++index)
            this->avatar_portraits_[index] = DefaultPortrait(index);
        this->types_[0].portrait_size = 126;
        this->types_[0].portrait_position = 1;
    }
    pet_module::pet_module(const bool target) : target_(target)
    {
        if (target) this->state_.bounds = {24, 520, 280, 75};
        this->ResetPortraitDefaults();
        const auto theme = ui::theme::CreateDefault();
        this->ApplyMeterDefaults(theme.meter); this->ApplyTextDefaults(theme);
    }
    auto pet_module::State(void) -> module_state&
    {
        if (this->target_) this->state_.enabled = this->Main().target_settings_.enabled;
        return this->state_;
    }
    auto pet_module::State(void) const -> const module_state& { return const_cast<pet_module*>(this)->State(); }
    auto pet_module::PreviewVisible(void) const -> bool
    {
        if (this->target_) return this->HasEditableBounds() && this->Main().target_settings_.enabled && this->Main().target_allowed_;
        const auto sourceKind = this->source_ != nullptr ? this->source_->State().kind : pet_kind::count;
        const auto kind = this->view_ < pet_kind_count ? this->view_ : sourceKind < pet_kind::count ? static_cast<size_t>(sourceKind) : size_t{0};
        return this->state_.enabled && this->types_[kind].enabled;
    }
    auto pet_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        if (this->target_ && this->owner_ != nullptr) return;
        for (auto& type : this->types_) type.meters.fill(style);
        this->target_settings_.meter = style;
    }
    auto pet_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        if (this->target_ && this->owner_ != nullptr) return;
        for (auto& type : this->types_)
            type.text = {module_text_profile{theme.title_text}, module_text_profile{theme.meter_text}, module_text_profile{theme.caption_text},
                module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}};
        this->target_settings_.text = {module_text_profile{theme.title_text}, module_text_profile{theme.meter_text}, module_text_profile{theme.caption_text}};
    }
    auto pet_module::VisitTextSettings(const text_settings_visitor& visit) -> void
    {
        if (this->target_) return;
        for (auto& type : this->types_) VisitTextProfiles(visit, type.text);
        VisitTextProfiles(visit, this->target_settings_.text);
    }
    auto pet_module::ResetDefaults(const ui::theme& theme) -> void
    {
        if (this->target_)
        {
            this->state_.content_bounds = {};
            this->Main().target_animation_ = {};
            return;
        }
        this->types_ = {}; this->target_settings_ = {}; this->avatar_portraits_ = {}; this->selected_avatar_ = 0; this->state_.enabled = false;
        this->ResetPortraitDefaults();
        this->EndControllerStatus(); this->status_hits_.clear();
        this->ApplyMeterDefaults(theme.meter); this->ApplyTextDefaults(theme);
        this->animations_ = {}; this->target_animation_ = {};
    }
    auto pet_module::Load(const settings_store& store) -> void
    {
        this->state_.bounds = store.ReadRect(this->target_ ? "layout.pettarget" : "layout.pet", this->state_.bounds);
        this->state_.content_bounds = {};
        if (this->target_) return;
        this->EndControllerStatus(); this->status_hits_.clear();
        this->state_.enabled = store.ReadBool("pet", "enabled", false);
        this->view_ = static_cast<size_t>(std::clamp(store.ReadInt("pet", "selected_type", 0), 0, static_cast<int>(pet_kind_count)));
        this->selected_avatar_ = std::clamp(store.ReadInt("pet", "selected_avatar", 0), 0, static_cast<int>(this->avatar_portraits_.size()) - 1);
        const bool upgradePortraits = store.ReadInt("pet", "portrait_preset_version", 0) < 1;
        for (size_t index = 0; index < this->avatar_portraits_.size(); ++index)
        {
            auto& portrait = this->avatar_portraits_[index];
            portrait = DefaultPortrait(index);
            portrait.Load(store, std::string{"pet.portrait."} + std::string{ui::avatar_portrait_keys[index]});
            if (upgradePortraits && portrait.LegacyDefault()) portrait = DefaultPortrait(index);
        }
        for (size_t index = 0; index < pet_kind_count; ++index)
        {
            auto& type = this->types_[index]; const auto section = std::string{"pet."} + pet_kind_keys[index];
            for (auto [key, flag] : {std::pair{"enabled", &type.enabled}, {"hp", &type.hp}, {"mp", &type.mp}, {"tp", &type.tp},
                {"distance", &type.distance}, {"job_icon", &type.icon}, {"recasts", &type.recasts}, {"background", &type.background}})
                *flag = store.ReadBool(section.c_str(), key, true);
            type.without_pet = store.ReadBool(section.c_str(), "recasts_without_pet", false);
            type.elapsed = store.ReadBool(section.c_str(), "observed_elapsed", false);
            type.portrait = store.ReadBool(section.c_str(), "portrait", true);
            type.lifetime = store.ReadBool(section.c_str(), "lifetime", true);
            const auto number = [&](const char* key, float fallback, float minimum, float maximum) {
                const auto value = store.ReadFloat(section.c_str(), key, fallback);
                return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
            };
            type.spacing = number("spacing", 4, 0, 24); type.container.opacity = number("opacity", 1, 0, 1); type.container.padding = number("padding", 6, 0, 24);
            type.portrait_size = number("portrait_size", index == 0 ? 126.0f : 48.0f, 16, 512);
            type.portrait_position = std::clamp(store.ReadInt(section.c_str(), "portrait_position", index == 0 ? 1 : 0), 0, 1);
            if (upgradePortraits && index == 0 && type.portrait_size == 48 && type.portrait_position == 0)
            { type.portrait_size = 126; type.portrait_position = 1; }
            type.portrait_style = {};
            type.portrait_style.Load(store, "portrait." + section);
            type.resource = std::clamp(store.ReadInt(section.c_str(), "resource", 0), 0, 2); type.selected_text = std::clamp(store.ReadInt(section.c_str(), "selected_text", 0), 0, 4);
            type.statuses.Load(store, ("status." + section).c_str());
            for (size_t resource = 0; resource < 3; ++resource)
            {
                const auto key = std::string{resource_names[resource]};
                type.top[resource] = store.ReadArgb(section.c_str(), (key + "_top").c_str(), type.top[resource]);
                type.bottom[resource] = store.ReadArgb(section.c_str(), (key + "_bottom").c_str(), type.bottom[resource]);
                type.meters[resource] = LoadMeterStyle(store, ("meter." + section + "." + key).c_str(), type.meters[resource]);
            }
            for (size_t text = 0; text < type.text.size(); ++text)
                type.text[text] = LoadTextProfile(store, ("text." + section + "." + text_keys[text]).c_str(), type.text[text]);
        }
        auto& target = this->target_settings_;
        target.enabled = store.ReadBool("pet.target", "enabled", false); target.snapped = store.ReadBool("pet.target", "snapped", true);
        target.distance = store.ReadBool("pet.target", "distance", true); target.percent = store.ReadBool("pet.target", "percent", true); target.label = store.ReadBool("pet.target", "label", true);
        target.background = store.ReadBool("pet.target", "background", true);
        target.tint = store.ReadBool("pet.target", "tint", false);
        target.background_tint = store.ReadArgb("pet.target", "background_tint", target.background_tint);
        const auto number = [&](const char* key, float fallback, float minimum, float maximum) {
            const auto value = store.ReadFloat("pet.target", key, fallback); return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        target.gap = number("gap", 6, 0, 64); target.width = number("width", 0, 0, 1200);
        target.offset_x = number("offset_x", 0, -500, 500); target.offset_y = number("offset_y", 0, -500, 500);
        target.container.opacity = number("background_opacity", 1, 0, 1);
        target.container.padding = number("background_padding", 2, 0, 24);
        target.tint_strength = number("tint_strength", 0.2f, 0, 1);
        target.top = store.ReadArgb("pet.target", "fill_top", target.top); target.bottom = store.ReadArgb("pet.target", "fill_bottom", target.bottom);
        target.selected_text = std::clamp(store.ReadInt("pet.target", "selected_text", 0), 0, 2);
        target.meter = LoadMeterStyle(store, "meter.pet.target", target.meter);
        for (size_t index = 0; index < target.text.size(); ++index)
            target.text[index] = LoadTextProfile(store, (std::string{"text.pet.target."} + text_keys[index]).c_str(), target.text[index]);
        this->animations_ = {}; this->target_animation_ = {};
    }
    auto pet_module::Save(const settings_store& store) const -> void
    {
        store.WriteRect(this->target_ ? "layout.pettarget" : "layout.pet", this->state_.bounds);
        if (this->target_) return;
        store.WriteBool("pet", "enabled", this->state_.enabled); store.WriteInt("pet", "selected_type", static_cast<int>(this->view_));
        store.WriteInt("pet", "selected_avatar", this->selected_avatar_);
        store.WriteInt("pet", "portrait_preset_version", 1);
        for (size_t index = 0; index < this->avatar_portraits_.size(); ++index)
            this->avatar_portraits_[index].Save(store, std::string{"pet.portrait."} + std::string{ui::avatar_portrait_keys[index]});
        for (size_t index = 0; index < pet_kind_count; ++index)
        {
            const auto& type = this->types_[index]; const auto section = std::string{"pet."} + pet_kind_keys[index];
            for (auto [key, value] : {std::pair{"enabled", type.enabled}, {"hp", type.hp}, {"mp", type.mp}, {"tp", type.tp}, {"distance", type.distance},
                {"job_icon", type.icon}, {"recasts", type.recasts}, {"background", type.background}, {"recasts_without_pet", type.without_pet}, {"observed_elapsed", type.elapsed}})
                store.WriteBool(section.c_str(), key, value);
            store.WriteBool(section.c_str(), "portrait", type.portrait); store.WriteBool(section.c_str(), "lifetime", type.lifetime);
            store.WriteFloat(section.c_str(), "portrait_size", type.portrait_size);
            store.WriteInt(section.c_str(), "portrait_position", type.portrait_position);
            type.portrait_style.Save(store, "portrait." + section);
            store.WriteFloat(section.c_str(), "spacing", type.spacing); store.WriteFloat(section.c_str(), "opacity", type.container.opacity); store.WriteFloat(section.c_str(), "padding", type.container.padding);
            store.WriteInt(section.c_str(), "resource", type.resource); store.WriteInt(section.c_str(), "selected_text", type.selected_text);
            type.statuses.Save(store, ("status." + section).c_str());
            for (size_t resource = 0; resource < 3; ++resource)
            {
                const auto key = std::string{resource_names[resource]};
                store.WriteArgb(section.c_str(), (key + "_top").c_str(), type.top[resource]); store.WriteArgb(section.c_str(), (key + "_bottom").c_str(), type.bottom[resource]);
                SaveMeterStyle(store, ("meter." + section + "." + key).c_str(), type.meters[resource]);
            }
            for (size_t text = 0; text < type.text.size(); ++text) SaveTextProfile(store, ("text." + section + "." + text_keys[text]).c_str(), type.text[text]);
        }
        const auto& target = this->target_settings_;
        for (auto [key, value] : {std::pair{"enabled", target.enabled}, {"snapped", target.snapped}, {"distance", target.distance}, {"percent", target.percent}, {"label", target.label},
            {"background", target.background}, {"tint", target.tint}})
            store.WriteBool("pet.target", key, value);
        for (auto [key, value] : {std::pair{"gap", target.gap}, {"width", target.width}, {"offset_x", target.offset_x}, {"offset_y", target.offset_y}}) store.WriteFloat("pet.target", key, value);
        store.WriteFloat("pet.target", "background_opacity", target.container.opacity);
        store.WriteFloat("pet.target", "background_padding", target.container.padding);
        store.WriteFloat("pet.target", "tint_strength", target.tint_strength);
        store.WriteArgb("pet.target", "background_tint", target.background_tint);
        store.WriteArgb("pet.target", "fill_top", target.top); store.WriteArgb("pet.target", "fill_bottom", target.bottom); store.WriteInt("pet.target", "selected_text", target.selected_text);
        SaveMeterStyle(store, "meter.pet.target", target.meter);
        for (size_t index = 0; index < target.text.size(); ++index) SaveTextProfile(store, (std::string{"text.pet.target."} + text_keys[index]).c_str(), target.text[index]);
    }
    auto pet_module::ConfigViews(void) const -> std::span<const char* const>
    {
        static constexpr const char* views[]{"Avatar / spirit", "Jug pet", "Charmed pet", "Wyvern", "Automaton", "Pet Target"};
        return this->target_ ? std::span<const char* const>{} : std::span<const char* const>{views};
    }
    auto pet_module::OptionCount(void) const -> size_t
    { return this->TargetConfig() ? target_base_options + meter_style_option_count + text_profile_option_count : status_start + ui::status_strip_settings::option_count; }
    auto pet_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static const auto sections = [] { std::array<module_config_section, 9> result{}; std::copy(BarConfigSections().begin(), BarConfigSections().end(), result.begin());
            result[7] = {"Statuses", "Observed pet effects, native icons and unknown-duration presentation.", "statuses"};
            result[8] = {"Portrait", "Avatar artwork, independent image offsets, scale, opacity and clipping.", "portrait"}; return result; }();
        return this->TargetConfig() ? BarConfigSections() : std::span<const module_config_section>{sections};
    }
    auto pet_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount() || section >= this->ConfigSections().size()) return false;
        if (this->TargetConfig()) return this->TargetContains(section, index);
        if (index >= status_start) return section == 7;
        if (index >= text_start || index == 17) return section == 6;
        if (index >= meter_start) return section == static_cast<size_t>(MeterConfigSection(index - meter_start));
        if (index == 6 || index == 19 || index == 20 || index >= 22)
            return section == 8 && (index != 23 || this->view_ == 0);
        if (index == 10) return section == 1 || section == 8;
        if (index == 14) return section >= 1 && section <= 5;
        if (index == 15 || index == 16) return section == 5;
        if (index == 11 || index == 12 || index == 13 || index == 18) return section == 1;
        return section == 0;
    }
    auto pet_module::Option(const size_t index) -> module_option
    {
        if (this->TargetConfig()) return this->TargetOption(index);
        auto& type = this->types_[this->view_];
        const bool mpType = this->view_ == static_cast<size_t>(pet_kind::avatar) || this->view_ == static_cast<size_t>(pet_kind::automaton);
        const bool resourceVisible = type.resource == 0 ? type.hp : type.resource == 1 ? mpType && type.mp : type.tp;
        const bool lifetimeType = this->view_ == static_cast<size_t>(pet_kind::jug) || this->view_ == static_cast<size_t>(pet_kind::charm);
        if (index >= status_start) return type.statuses.Option(index - status_start);
        if (index >= text_start) return TextProfileOption(index - text_start, type.text[type.selected_text]).VisibleWhen(
            type.selected_text == 0 || (type.selected_text == 1 ? type.hp || (mpType && type.mp) || type.tp :
            type.selected_text == 2 ? type.distance || type.elapsed || (lifetimeType && type.lifetime) :
            type.selected_text == 3 ? type.statuses.enabled : type.recasts));
        if (index >= meter_start) return MeterStyleOption(index - meter_start, type.meters[type.resource]).VisibleWhen(resourceVisible);
        auto& portraitStyle = this->PortraitStyle(static_cast<pet_kind>(this->view_), ui::avatar_portrait_names[this->selected_avatar_]);
        static constexpr const char* portraitPositions[]{"Inline", "Overlay"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the Pet HUD; new installations keep it disabled until enabled.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Show this pet type", "Independent visibility for this pet category.", &type.enabled);
        case 2: return module_option::Toggle("HP percent", "Show native pet HP percent without inventing absolute HP.", &type.hp);
        case 3: return module_option::Toggle("MP percent", "Show native MP percent for avatars, spirits and automatons when available.", &type.mp).VisibleWhen(mpType);
        case 4: return module_option::Toggle("TP", "Show valid native pet TP (0–3000).", &type.tp);
        case 5: return module_option::Toggle("Distance", "Show the native pet distance.", &type.distance);
        case 6: return module_option::Toggle("Job icon fallback", "Use the shared job artwork when pet portrait artwork is unavailable or hidden.", &type.icon).Ordered(-40);
        case 7: return module_option::Toggle("Ability recasts", "Use existing native ability timers and Ready charges.", &type.recasts);
        case 8: return module_option::Toggle("Recasts without pet", "Keep known cooldowns visible without an active pet.", &type.without_pet).VisibleWhen(type.recasts);
        case 9: return module_option::Toggle("Observed elapsed time", "Time since XIUI observed this pet; this is not its remaining lifetime.", &type.elapsed);
        case 10: return module_option::Toggle("Show background", "Show the Pet HUD's native frame and background independently of its portrait.", &type.background).Ordered(-45);
        case 11: return module_option::Slider("Opacity", "Native container opacity.", &type.container.opacity, 0, 1, .05f).VisibleWhen(type.background);
        case 12: return module_option::Slider("Padding", "Space inside the pet window.", &type.container.padding, 0, 24, 1);
        case 13: return ModuleWidthOption(this->state_);
        case 14: return module_option::EditScope("Resource", "Choose the pet meter to style.", &type.resource, resource_names, 3);
        case 15: return module_option::Color("Fill top", "Upper resource gradient.", &type.top[type.resource]).VisibleWhen(resourceVisible);
        case 16: return module_option::Color("Fill bottom", "Lower resource gradient.", &type.bottom[type.resource]).VisibleWhen(resourceVisible);
        case 17: return module_option::Element("Selected text", "Independent text profiles for this pet type.", &type.selected_text, text_names, 5);
        case 18: return module_option::Slider("Row spacing", "Space between resources and ability timers.", &type.spacing, 0, 24, 1);
        case 19: return module_option::Toggle("Pet portrait", "Use XIUI's original pet artwork when available.", &type.portrait).Ordered(-50);
        case 20: return module_option::Slider("Portrait size", "Base image size in pixels; each avatar's scale adjusts this independently.", &type.portrait_size, 16, 512, 1).Ordered(100, true).VisibleWhen(type.portrait);
        case 21: return module_option::Toggle("Lifetime information", "Show Lifetime unavailable for Jug/Charmed pets; native memory supplies no remaining deadline.", &type.lifetime).VisibleWhen(lifetimeType);
        case 22: return module_option::Dropdown("Portrait position", "Inline reserves space beside the name. Overlay positions the image from the Pet window's top-left without moving text or bars.", &type.portrait_position, portraitPositions, 2).Ordered(150).VisibleWhen(type.portrait || type.icon);
        case 23: return module_option::Dropdown("Avatar image", "Choose an avatar or spirit to preview and customize. Each image retains its own scale, opacity, offsets and clipping; gameplay follows your summoned pet.", &this->selected_avatar_, ui::avatar_portrait_names, std::size(ui::avatar_portrait_names)).Ordered(-30).VisibleWhen(type.portrait || type.icon);
        case 24: return module_option::Slider("Image scale", "Scale the selected image while preserving its proportions.", &portraitStyle.scale, 0.1f, 4, 0.05f).Ordered(101).VisibleWhen(type.portrait || type.icon);
        case 25: return module_option::Slider("Image opacity", "Opacity of the selected image, independent of the window background.", &portraitStyle.opacity, 0, 1, 0.05f).Ordered(300).VisibleWhen(type.portrait || type.icon);
        case 26: return module_option::Slider("Image offset X", "Horizontal image offset; Overlay starts at the Pet window's left edge.", &portraitStyle.offset_x, -600, 600, 1).Ordered(200, true).VisibleWhen(type.portrait || type.icon);
        case 27: return module_option::Slider("Image offset Y", "Vertical image offset; Overlay starts at the Pet window's top edge.", &portraitStyle.offset_y, -600, 600, 1).Ordered(201).VisibleWhen(type.portrait || type.icon);
        default: return module_option::Toggle("Clip image to window", "Clip to the complete Pet window, with its native frame drawn over the image. Turn off to let artwork extend outside it.", &portraitStyle.clip).Ordered(310).VisibleWhen(type.portrait || type.icon);
        }
    }
    auto pet_module::TargetContains(const size_t section, const size_t index) const -> bool
    {
        if (index >= target_base_options + meter_style_option_count) return section == 6;
        if (index >= target_base_options) return section == static_cast<size_t>(MeterConfigSection(index - target_base_options));
        if (index >= 12) return section == 1 || (index == 16 && section == 5);
        if (index == 7 || index == 8) return section == 5;
        if (index == 9) return section == 6;
        if (index == 1 || index == 5 || index == 6 || index == 10 || index == 11) return section == 1;
        return section == 0;
    }
    auto pet_module::TargetOption(const size_t index) -> module_option
    {
        auto& main = this->Main(); auto& target = main.target_settings_;
        if (index >= target_base_options + meter_style_option_count)
            return TextProfileOption(index - target_base_options - meter_style_option_count, target.text[target.selected_text]).VisibleWhen(
                target.selected_text == 0 || (target.selected_text == 1 ? target.percent || target.label : target.distance));
        if (index >= target_base_options) return MeterStyleOption(index - target_base_options, target.meter);
        static constexpr const char* profiles[]{"Name", "HP value", "Distance"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the pet's native or last observed action target.", &target.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Snap to Pet", "Attach below the Pet HUD; turn off for independent movement.", &target.snapped);
        case 2: return module_option::Toggle("Distance", "Show the pet target's native distance.", &target.distance);
        case 3: return module_option::Toggle("HP percent", "Show the pet target's native HP percent.", &target.percent);
        case 4: return module_option::Toggle("HP label", "Show the HP label in the target meter.", &target.label);
        case 5: return module_option::Slider("Attached gap", "Space below the Pet HUD when snapped.", &target.gap, 0, 64, 1).VisibleWhen(target.snapped);
        case 6:
            if (!target.snapped && main.target_adapter_ != nullptr) return ModuleWidthOption(main.target_adapter_->state_);
            return module_option::Slider("Attached width", "Zero uses the Pet HUD width; independent width is saved separately.", &target.width, 0, 1200, 1);
        case 7: return module_option::Color("Fill top", "Pet target HP gradient top.", &target.top);
        case 8: return module_option::Color("Fill bottom", "Pet target HP gradient bottom.", &target.bottom);
        case 9: return module_option::Element("Selected text", "Independent pet-target text profiles.", &target.selected_text, profiles, 3);
        case 10: return module_option::Slider("Attached X offset", "Horizontal offset while snapped to Pet.", &target.offset_x, -500, 500, 1).Ordered(200, true).VisibleWhen(target.snapped);
        case 11: return module_option::Slider("Attached Y offset", "Vertical offset while snapped to Pet.", &target.offset_y, -500, 500, 1).Ordered(201).VisibleWhen(target.snapped);
        case 12: return module_option::Toggle("Show background", "Show Pet Target's native frame and background in attached or independent placement.", &target.background).Ordered(-40);
        case 13: return module_option::Slider("Background opacity", "Pet Target background opacity; text and the HP bar retain their own styles.", &target.container.opacity, 0, 1, 0.05f).Ordered(310).VisibleWhen(target.background);
        case 14: return module_option::Slider("Background padding", "Space inside the Pet Target window.", &target.container.padding, 0, 24, 1).Ordered(300);
        case 15: return module_option::Toggle("Tint background", "Add a color wash inside Pet Target's background.", &target.tint).Ordered(320).VisibleWhen(target.background);
        case 16: return module_option::Color("Tint color", "Color and alpha of the optional background tint.", &target.background_tint).Ordered(330).VisibleWhen(target.background && target.tint);
        default: return module_option::Slider("Tint strength", "Strength of the color wash; it follows background opacity and leaves the native frame unchanged.", &target.tint_strength, 0, 1, 0.05f).Ordered(331).VisibleWhen(target.background && target.tint);
        }
    }
    auto pet_module::Cooldowns(const pet_kind kind, const uint64_t now) const -> std::vector<const recast_entry*>
    {
        std::vector<const recast_entry*> result;
        const auto* source = this->Main().recasts_;
        if (source != nullptr && source->Available())
            for (const auto& recast : source->Entries())
                if (recast.kind == recast_kind::ability && PetRecast(recast.id, kind) && recast.Visible(now, 2)) result.push_back(&recast);
        return result;
    }
    auto pet_module::DisplayKind(void) const -> pet_kind
    {
        if (this->source_ == nullptr) return pet_kind::count;
        const auto& snapshot = this->source_->State();
        if (snapshot.pet.id != 0) return snapshot.kind;
        for (size_t step = 0; step <= pet_kind_count; ++step)
        {
            const auto kind = step == 0 ? snapshot.kind : static_cast<pet_kind>(step - 1);
            if (kind >= pet_kind::count) continue;
            const auto index = static_cast<size_t>(kind);
            const auto job = pet_jobs[index];
            const bool available = snapshot.main_job == job || ((job == 9 || job == 15) && snapshot.sub_job == job);
            const auto& settings = this->types_[index];
            if (available && settings.enabled && settings.without_pet && settings.recasts && !this->Cooldowns(kind, ::GetTickCount64()).empty()) return kind;
        }
        return snapshot.kind;
    }
    auto pet_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        const auto& main = this->Main();
        if (main.source_ == nullptr) return false;
        const auto& snapshot = main.source_->State();
        if (this->target_) return main.target_settings_.enabled && !main.target_settings_.snapped && main.target_allowed_ && snapshot.pet.id != 0 && snapshot.target.id != 0;
        const auto kind = this->DisplayKind();
        if (!this->state_.enabled || kind >= pet_kind::count || !snapshot.ready) return false;
        const auto& settings = this->types_[static_cast<size_t>(kind)];
        return settings.enabled && (snapshot.pet.id != 0 || (settings.without_pet && settings.recasts && !this->Cooldowns(kind, ::GetTickCount64()).empty()));
    }
    auto pet_module::DrawTarget(const pet_entity& target, const ui::rect& bounds, const module_render_context& context) -> float
    {
        auto& main = this->Main(); const auto& settings = main.target_settings_;
        if (main.drawn_target_id_ != target.id) { main.target_animation_ = {}; main.drawn_target_id_ = target.id; }
        const float line = std::max(18.0f, context.Design().TextHeight(settings.text[0].style) + 4);
        const float inset = settings.container.padding + (settings.background ? settings.container.edge_size : 0);
        const ui::rect at{bounds.x + inset, bounds.y + inset, std::max(1.0f, bounds.width - inset * 2), line};
        const ui::rect meter{at.x, at.y + line + 2, at.width, std::max(1.0f, settings.meter.height) + 2};
        const float height = meter.y + meter.height + std::max(16.0f, context.Design().TextHeight(settings.text[1].style)) * .5f + inset - bounds.y;
        if (settings.background)
        {
            const ui::rect panel{bounds.x, bounds.y, bounds.width, height};
            const std::array fills{ui::container_fill{panel, settings.TintColor()}};
            context.DrawContainer(panel, settings.container, std::span{fills}.first(settings.tint ? 1 : 0));
        }
        const bool valid = target.id != 0;
        const auto name = valid ? TintTextProfile(settings.text[0], target.color) : settings.text[0];
        context.DrawText(valid ? target.name.c_str() : "Pet target", at, at, name);
        char distance[32]{};
        if (valid && settings.distance && target.distance_known) std::snprintf(distance, sizeof(distance), "%.1f y", target.distance);
        if (*distance)
        {
            auto text = settings.text[2]; text.style.alignment = ui::text_alignment::right;
            context.DrawText(distance, at, at, text);
        }
        char hp[20]{};
        if (settings.percent && valid) std::snprintf(hp, sizeof(hp), "%u%%", target.hp);
        context.DrawMeter(meter, settings.label ? "HP" : "", hp, valid ? target.hp / 100.0f : 0, settings.top, settings.bottom,
            settings.meter, main.target_animation_, true, &settings.text[1], &settings.text[1], ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, meter_fill_mode::health);
        return height;
    }
    auto pet_module::LifetimeLabel(const uint64_t observedSince, const uint64_t now, const bool elapsed) -> std::string
    {
        auto text = std::string{"Lifetime unavailable"};
        if (elapsed && observedSince != 0 && now >= observedSince)
            text += "  Observed " + FormatRecastTime(static_cast<float>((now - observedSince) / 1000), false);
        return text;
    }
    auto pet_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        auto& main = this->Main();
        const pet_snapshot empty{};
        const auto& snapshot = main.source_ != nullptr ? main.source_->State() : empty;
        if (this->target_)
        {
            if (main.target_settings_.snapped || !main.target_settings_.enabled || !main.target_allowed_ || (!context.IsEditing() && snapshot.target.id == 0)) return;
            this->state_.minimum_height = this->DrawTarget(snapshot.target, this->state_.bounds, context);
            this->state_.bounds.height = this->state_.minimum_height;
            return;
        }
        this->status_hits_.clear();
        const auto previewKind = this->view_ < pet_kind_count ? static_cast<pet_kind>(this->view_) : snapshot.kind < pet_kind::count ? snapshot.kind : pet_kind::avatar;
        const auto kind = context.IsEditing() ? previewKind : this->DisplayKind();
        if (kind >= pet_kind::count) return;
        auto& settings = this->types_[static_cast<size_t>(kind)];
        if (!settings.enabled) return;
        const auto now = ::GetTickCount64();
        const bool actual = snapshot.pet.id != 0 && snapshot.kind == kind;
        if (this->drawn_pet_id_ != snapshot.pet.id || this->drawn_pet_index_ != snapshot.pet.index || this->drawn_since_ != snapshot.observed_since)
        {
            this->animations_ = {}; this->drawn_pet_id_ = snapshot.pet.id; this->drawn_pet_index_ = snapshot.pet.index;
            this->drawn_since_ = snapshot.observed_since;
            this->EndControllerStatus();
        }
        const auto cooldowns = settings.recasts ? this->Cooldowns(kind, now) : std::vector<const recast_entry*>{};
        const auto inset = settings.container.padding + (settings.background ? settings.container.edge_size : 0);
        const auto lineHeight = [&](size_t profile) { return std::max(16.0f, context.Design().TextHeight(settings.text[profile].style) + 4); };
        const std::string_view imageName = context.IsEditing() && kind == pet_kind::avatar ? ui::avatar_portrait_names[this->selected_avatar_] :
            actual ? std::string_view{snapshot.pet.name} : kind == pet_kind::avatar ? "Carbuncle" : "";
        const auto& portraitStyle = this->PortraitStyle(kind, imageName);
        ui::game_icon_view portrait{};
        if (settings.portrait && context.Icons() != nullptr)
            portrait = context.Icons()->PetPortrait(imageName, static_cast<int>(kind), settings.icon);
        const auto portraitSize = portrait.texture != nullptr ? settings.portrait_size : settings.icon ? 24.0f : 0.0f;
        const float inlineSize = settings.portrait_position == 0 ? portraitSize * portraitStyle.scale : 0;
        const float heading = std::max(lineHeight(0), inlineSize), width = std::max(1.0f, this->state_.bounds.width - inset * 2);
        const auto statuses = actual ? std::span<const combat_status>{snapshot.statuses}.first(snapshot.status_count) : std::span<const combat_status>{};
        const bool mp = actual && settings.mp && snapshot.mp_known, tp = actual && settings.tp && snapshot.tp_known;
        float height = inset * 2 + heading + settings.spacing;
        for (size_t resource = 0; resource < 3; ++resource)
            if ((resource == 0 && settings.hp) || (resource == 1 && mp) || (resource == 2 && tp))
                height += std::max(1.0f, settings.meters[resource].height) + 2 + lineHeight(1) * .5f + settings.spacing;
        const bool lifetime = settings.lifetime && (kind == pet_kind::jug || kind == pet_kind::charm) && (actual || context.IsEditing());
        if (settings.distance || (settings.elapsed && !lifetime)) height += lineHeight(2) + settings.spacing;
        if (lifetime) height += lineHeight(2) + settings.spacing;
        height += ui::StatusStripHeight(statuses.size(), width, settings.statuses);
        height += static_cast<float>(cooldowns.size()) * (lineHeight(4) + settings.spacing);
        this->state_.bounds.height = this->state_.minimum_height = std::max(40.0f, height);
        const bool frameOverPortrait = settings.background && portraitStyle.clip;
        if (settings.background)
        {
            auto container = settings.container;
            if (frameOverPortrait) container.border = false;
            context.DrawContainer(this->state_.bounds, container);
        }
        float x = this->state_.bounds.x + inset, y = this->state_.bounds.y + inset;
        const auto imageAt = portraitStyle.Bounds(settings.portrait_position == 0 ? ui::rect{x, y, width, heading} : this->state_.bounds, portraitSize);
        const auto imageTint = (static_cast<uint32_t>(std::lround(portraitStyle.opacity * 255)) << 24) | 0x00FFFFFF;
        const auto drawPortrait = [&] {
            if (portrait.texture != nullptr)
                context.Design().DrawTexture(portrait.texture, imageAt, imageTint, portrait.uv);
            else if (settings.icon && context.Icons() != nullptr && context.Icons()->JobAtlas() != nullptr)
            {
                const auto icon = pet_jobs[static_cast<size_t>(kind)] - 1;
                context.Design().DrawTexture(context.Icons()->JobAtlas(), imageAt, imageTint,
                    {(static_cast<float>(icon % 8) * 64 + .5f) / 512, (static_cast<float>(icon / 8) * 64 + .5f) / 256, 63.0f / 512, 63.0f / 256});
            }
        };
        if (portraitStyle.clip)
        {
            const auto clip = context.Design().Clip(this->state_.bounds);
            drawPortrait();
        }
        else drawPortrait();
        if (frameOverPortrait) context.DrawContainerFrame(this->state_.bounds, settings.container);
        const float iconWidth = inlineSize > 0 ? inlineSize + 6 : 0;
        const ui::rect nameAt{x + iconWidth, y, std::max(1.0f, width - iconWidth), heading};
        const auto name = actual ? TintTextProfile(settings.text[0], snapshot.pet.color) : settings.text[0];
        const auto title = actual ? snapshot.pet.name : std::string{"No active "} + pet_kind_names[static_cast<size_t>(kind)];
        context.DrawText(title.c_str(), nameAt, nameAt, name); y += heading + settings.spacing;
        for (size_t resource = 0; resource < 3; ++resource)
        {
            if ((resource == 0 && !settings.hp) || (resource == 1 && !mp) || (resource == 2 && !tp)) continue;
            const auto value = resource == 0 ? snapshot.pet.hp : resource == 1 ? snapshot.mp_percent : snapshot.tp;
            const auto maximum = resource == 2 ? 3000 : 100;
            const auto text = actual ? std::to_string(value) + (resource == 2 ? "" : "%") : "--";
            const ui::rect meter{x, y, width, std::max(1.0f, settings.meters[resource].height) + 2};
            auto label = settings.text[1], percent = settings.text[1]; label.style.alignment = ui::text_alignment::left; percent.style.alignment = ui::text_alignment::right;
            if (resource == 0 && actual) percent = TintTextProfile(percent, ui::HPValueColor(value));
            context.DrawMeter(meter, resource_names[resource], text.c_str(), actual ? static_cast<float>(value) / maximum : 0, settings.top[resource], settings.bottom[resource],
                settings.meters[resource], this->animations_[resource], true, &label, &percent, ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, resource == 0 ? meter_fill_mode::health : meter_fill_mode::single);
            y += meter.height + lineHeight(1) * .5f + settings.spacing;
        }
        if (settings.distance || (settings.elapsed && !lifetime))
        {
            std::string details = pet_kind_names[static_cast<size_t>(kind)];
            if (actual && settings.distance && snapshot.pet.distance_known)
            { char value[32]{}; std::snprintf(value, sizeof(value), "  %.1f y", snapshot.pet.distance); details += value; }
            if (actual && settings.elapsed && !lifetime && now >= snapshot.observed_since)
                details += "  Observed " + FormatRecastTime(static_cast<float>((now - snapshot.observed_since) / 1000), false);
            context.DrawText(details.c_str(), {x, y, width, lineHeight(2)}, {x, y, width, lineHeight(2)}, settings.text[2]); y += lineHeight(2) + settings.spacing;
        }
        if (lifetime)
        {
            const auto text = LifetimeLabel(actual ? snapshot.observed_since : 0, now, settings.elapsed);
            const ui::rect at{x, y, width, lineHeight(2)}; context.DrawText(text.c_str(), at, at, settings.text[2]); y += lineHeight(2) + settings.spacing;
        }
        this->status_bounds_ = {x, y, width, 0}; this->drawn_kind_ = kind;
        ui::DrawStatusStrip(statuses, this->status_bounds_, now, settings.text[3], context, settings.statuses);
        if (settings.statuses.tooltips)
        {
            const auto layout = ui::LayoutStatusStrip(statuses.size(), this->status_bounds_, settings.statuses);
            for (size_t index = 0; index < layout.count; ++index) this->status_hits_.push_back({statuses[index], layout.Icon(index), snapshot.pet.id});
        }
        y += ui::StatusStripHeight(statuses.size(), width, settings.statuses);
        for (const auto* recast : cooldowns)
        {
            auto text = recast->id == 102 && kind == pet_kind::charm ? std::string{"Sic"} : recast->name;
            if (recast->maximum_charges != 0 && kind != pet_kind::charm)
                text += " " + std::to_string(recast->charges) + "/" + std::to_string(recast->maximum_charges);
            text += recast->Remaining(now) > 0 ? "  " + FormatRecastTime(recast->Remaining(now), false) : "  Ready";
            const ui::rect at{x, y, width, lineHeight(4)}; context.DrawText(text.c_str(), at, at, settings.text[4]); y += lineHeight(4) + settings.spacing;
        }
        if (main.target_settings_.enabled && main.target_settings_.snapped && main.target_allowed_ && (context.IsEditing() || (actual && snapshot.target.id != 0)))
        {
            const auto& target = main.target_settings_;
            const ui::rect area{this->state_.bounds.x + target.offset_x, this->state_.bounds.y + this->state_.bounds.height + target.gap + target.offset_y,
                target.width > 0 ? target.width : this->state_.bounds.width, 0};
            this->DrawTarget(snapshot.target, area, context);
        }
    }
    auto pet_module::ControllerStatusIndex(void) const -> std::optional<size_t>
    {
        if (!this->ShouldRender({}) || this->source_ == nullptr || this->source_->State().pet.id != this->controller_entity_ ||
            this->source_->State().observed_since != this->controller_since_) return std::nullopt;
        return ui::StatusHitIndex(this->status_hits_, this->controller_entity_, this->controller_effect_, this->controller_occurrence_);
    }
    auto pet_module::BeginControllerStatus(void) -> bool
    {
        this->EndControllerStatus();
        if (!this->ShouldRender({}) || this->source_ == nullptr || this->status_hits_.empty()) return false;
        this->controller_entity_ = this->status_hits_.front().entity; this->controller_effect_ = this->status_hits_.front().status.id;
        this->controller_since_ = this->source_->State().observed_since;
        return this->ControllerStatusIndex().has_value();
    }
    auto pet_module::MoveControllerStatus(const int direction) -> bool
    {
        const auto current = this->ControllerStatusIndex();
        if (!current || direction == 0) return false;
        const auto next = (*current + (direction > 0 ? 1 : this->status_hits_.size() - 1)) % this->status_hits_.size();
        this->controller_effect_ = this->status_hits_[next].status.id; this->controller_occurrence_ = ui::StatusHitOccurrence(this->status_hits_, next);
        return true;
    }
    auto pet_module::ControllerStatusBounds(void) const -> ui::rect
    {
        const auto current = this->ControllerStatusIndex();
        return current ? this->status_hits_[*current].bounds : ui::rect{};
    }
    auto pet_module::RenderStatusTooltip(const module_render_context& context, IResourceManager* resources, const float x, const float y, const bool controller) const -> void
    {
        const auto& main = this->Main();
        if (this->target_ || !main.state_.enabled || main.source_ == nullptr || this->drawn_kind_ >= pet_kind::count) return;
        const auto& snapshot = main.source_->State();
        if (snapshot.kind != this->drawn_kind_ || snapshot.pet.id == 0) return;
        const auto& settings = this->types_[static_cast<size_t>(snapshot.kind)];
        const auto focused = this->ControllerStatusIndex();
        for (size_t index = 0; index < this->status_hits_.size(); ++index)
        {
            const auto& hit = this->status_hits_[index]; const auto& at = hit.bounds;
            if (hit.entity != snapshot.pet.id) continue;
            if (controller ? focused && *focused == index : x >= at.x && x < at.x + at.width && y >= at.y && y < at.y + at.height)
            { ui::DrawStatusTooltip(hit, resources != nullptr ? resources : main.source_->Resources(), settings.text[3], context); return; }
        }
    }
}
