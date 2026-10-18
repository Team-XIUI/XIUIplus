#include "status_effects_module.hpp"
#include "ui/status_classification.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    status_effects_module::status_effects_module(const bool debuffs) : debuffs_(debuffs)
    {
        if (debuffs) this->state_.bounds.y = 376;
        this->settings_.size = 32;
        this->settings_.columns = 8;
    }

    auto status_effects_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.body_text}};
        for (auto& profile : this->text_) profile.style.scale = 0.85f;
    }

    auto status_effects_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        auto* buffs = this->buffs_module_;
        *this = status_effects_module{this->debuffs_};
        this->state_.bounds = bounds;
        this->source_ = source;
        this->buffs_module_ = buffs;
        this->ApplyTextDefaults(theme);
    }

    auto status_effects_module::TextSection(const size_t index) const -> const char*
    {
        static constexpr const char* sections[2][3]{{"text.buffs.heading", "text.buffs.timers", "text.buffs.tooltip"},
            {"text.debuffs.heading", "text.debuffs.timers", "text.debuffs.tooltip"}};
        return sections[this->debuffs_ ? 1 : 0][index];
    }

    auto status_effects_module::Load(const settings_store& store) -> void
    {
        this->EndControllerBrowse();
        const auto* section = this->Section();
        this->settings_.Load(store, section);
        this->preview_ = store.ReadBool(section, "preview", false);
        this->follow_buffs_ = this->debuffs_ && store.ReadBool(section, "follow_buffs", false);
        this->state_.enabled = this->settings_.enabled;
        this->settings_.enabled = true;
        this->state_.bounds = store.ReadRect(this->LayoutSection(), this->state_.bounds);
        this->state_.content_bounds = {};
        this->heading_ = store.ReadBool(section, "heading", true);
        this->background_ = store.ReadBool(section, "background", false);
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = store.ReadFloat(section, key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->settings_.size = number("size", 32, 12, 48);
        this->settings_.columns = number("columns", 8, 0, 32);
        this->container_.opacity = number("container_opacity", 1, 0, 1);
        this->container_.padding = number("padding", 6, 0, 24);
        this->selected_text_ = std::clamp(store.ReadInt(section, "selected_text", 0), 0, 2);
        for (size_t index = 0; index < this->text_.size(); ++index)
            this->text_[index] = LoadTextProfile(store, this->TextSection(index), this->text_[index]);
    }

    auto status_effects_module::Save(const settings_store& store) const -> void
    {
        const auto* section = this->Section();
        auto settings = this->settings_;
        store.WriteBool(section, "preview", this->preview_);
        if (this->debuffs_) store.WriteBool(section, "follow_buffs", this->follow_buffs_);
        settings.enabled = this->state_.enabled;
        settings.Save(store, section);
        store.WriteRect(this->LayoutSection(), this->state_.bounds);
        store.WriteBool(section, "heading", this->heading_);
        store.WriteBool(section, "background", this->background_);
        store.WriteFloat(section, "container_opacity", this->container_.opacity);
        store.WriteFloat(section, "padding", this->container_.padding);
        store.WriteInt(section, "selected_text", this->selected_text_);
        for (size_t index = 0; index < this->text_.size(); ++index)
            SaveTextProfile(store, this->TextSection(index), this->text_[index]);
    }

    auto status_effects_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Your current native status effects and available countdowns.", "general"},
            {"Layout", "Independent group position, wrapping, icon size and spacing.", "layout"},
            {"Appearance", "Native status artwork and optional window background.", "appearance"},
            {"Text", "Heading, countdown and game effect details.", "text"}};
        return sections;
    }

    auto status_effects_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= preference_options_start) return section == 0;
        if (index >= text_options_start) return section == 3;
        if (index >= appearance_options_start)
        {
            switch (index - appearance_options_start)
            {
            case 0: return section == 0;
            case 1: case 2: return section == 2;
            case 3: case 4: return section == 1;
            default: return section == 3;
            }
        }
        if (index == 12) return section == 3;
        if (index == 15) return section == 0;
        if (index == 11) return section == 1;
        if (index == 14 || index == 17) return section == 2;
        if ((index >= 1 && index <= 6) || index == 13 || index == 16) return section == 1;
        return section == 0;
    }

    auto status_effects_module::Option(const size_t index) -> module_option
    {
        if (index == 0) return module_option::Toggle("Enabled", "Show this group of your native status effects.", &this->state_.enabled).Ordered(-50);
        if (index == preference_options_start) return module_option::Toggle("Show preview", "Show a few sample effects and countdowns in edit mode only.", &this->preview_).Ordered(-20);
        if (this->debuffs_ && index == preference_options_start + 1)
            return module_option::Toggle("Follow Buffs settings", "Match Buffs icon, timer, text, heading and container settings. Position, width, visibility and preview stay independent.", &this->follow_buffs_).Ordered(-40);
        auto& style = this->StyleModule();
        const auto inherited = [&](module_option option) { return option.VisibleWhen(&style == this); };
        if (index < appearance_options_start) return inherited(style.settings_.Option(index));
        if (index >= text_options_start) return inherited(TextProfileOption(index - text_options_start, style.text_[this->selected_text_])).VisibleWhen(
            this->selected_text_ == 0 ? style.heading_ : this->selected_text_ == 1 ? style.settings_.enabled : style.settings_.enabled && style.settings_.tooltips);
        static constexpr const char* profiles[]{"Heading", "Timers", "Tooltip"};
        switch (index - appearance_options_start)
        {
        case 0: return inherited(module_option::Toggle("Heading", "Show the group's name above its icons.", &style.heading_));
        case 1: return inherited(module_option::Toggle("Container", "Use the shared native window background.", &style.background_, module_option_group::appearance).Ordered(-10));
        case 2: return inherited(module_option::Slider("Container opacity", "Opacity of the native group container.", &style.container_.opacity, 0, 1, 0.05f, module_option_group::appearance).Ordered(450)).VisibleWhen(style.background_);
        case 3: return inherited(module_option::Slider("Padding", "Space around the heading and status icons.", &style.container_.padding, 0, 24, 1).Ordered(402));
        case 4: return ModuleWidthOption(this->state_);
        default: return inherited(module_option::Element("Selected text", "Edit heading, timers and tooltip styles separately.", &this->selected_text_, profiles, 3, module_option_group::text));
        }
    }

    auto status_effects_module::Effects(const bool editing) const -> effect_list
    {
        effect_list result;
        if (editing && this->preview_)
        {
            constexpr std::array<uint16_t, 6> buffs{33, 40, 41, 42, 43, 116}, debuffs{2, 3, 4, 5, 6, 7};
            const auto& ids = this->debuffs_ ? debuffs : buffs;
            for (size_t index = 0; index < ids.size(); ++index)
                result.entries[result.count++] = {ids[index], 0, 0, ::GetTickCount64() + (index + 1) * 15000, false};
            return result;
        }
        if (this->source_ == nullptr) return result;
        const auto& player = this->source_->Members()[0];
        if (!player.active || !player.buffs_known) return result;
        result.player = player.id;
        for (size_t index = 0; index < std::min(player.buff_count, player.buffs.size()); ++index)
        {
            const auto effect = player.buffs[index];
            if (effect == 255 || effect > 1023 || ui::IsDebuffStatus(effect) != this->debuffs_) continue;
            result.entries[result.count++] = {effect, 0, 0, player.buff_expires[index], false};
        }
        return result;
    }

    auto status_effects_module::ShouldRender(const hud_snapshot&) const -> bool
    { return this->state_.enabled && this->Effects().count != 0; }

    auto status_effects_module::HeadingHeight(void) const -> float
    { const auto& style = this->StyleModule(); return style.heading_ ? std::max(16.0f, style.text_[0].style.scale * 18) + 4 : 0; }

    auto status_effects_module::StripBounds(void) const -> ui::rect
    {
        const auto inset = this->OuterInset();
        return {this->state_.bounds.x + inset, this->state_.bounds.y + inset + this->HeadingHeight(),
            std::max(1.0f, this->state_.bounds.width - inset * 2), 0};
    }

    auto status_effects_module::Layout(const effect_list& effects) const -> ui::status_strip_layout
    { return ui::LayoutStatusStrip(effects.count, this->StripBounds(), this->StyleModule().settings_); }

    auto status_effects_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const auto& style = this->StyleModule();
        const auto effects = this->Effects(context.IsEditing());
        const auto strip = this->Layout(effects);
        const auto inset = this->OuterInset();
        const auto emptyHeight = effects.count == 0 && context.IsEditing() ? std::max(16.0f, style.text_[1].style.scale * 18) : 0;
        this->state_.minimum_width = style.settings_.size + inset * 2;
        this->state_.minimum_height = inset * 2 + this->HeadingHeight() + std::max(emptyHeight,
            ui::StatusStripHeight(effects.count, this->StripBounds().width, style.settings_));
        this->state_.bounds.height = this->state_.minimum_height;
        if (style.background_) context.DrawContainer(this->state_.bounds, style.container_);
        if (style.heading_)
        {
            const ui::rect heading{this->state_.bounds.x + inset, this->state_.bounds.y + inset,
                this->state_.bounds.width - inset * 2, this->HeadingHeight() - 4};
            const auto title = std::string{this->DisplayName()} + (context.IsEditing() && this->preview_ ? " (Preview)" : "");
            context.DrawText(title.c_str(), heading, heading, style.text_[0]);
        }
        ui::DrawStatusStrip(effects.Effects(), this->StripBounds(), ::GetTickCount64(), style.text_[1], context, style.settings_);
        if (emptyHeight > 0)
        {
            const auto at = this->StripBounds();
            const ui::rect empty{at.x, at.y, at.width, emptyHeight};
            context.DrawText(this->debuffs_ ? "No active debuffs" : "No active buffs", empty, empty, style.text_[1]);
        }
        auto measured = this->state_.bounds;
        if (strip.count > 0)
        {
            const auto x = std::min(measured.x, strip.bounds.x), y = std::min(measured.y, strip.bounds.y);
            measured = {x, y, std::max(measured.x + measured.width, strip.bounds.x + strip.bounds.width) - x,
                std::max(measured.y + measured.height, strip.bounds.y + strip.bounds.height) - y};
        }
        this->state_.content_bounds = {measured.x - this->state_.bounds.x, measured.y - this->state_.bounds.y, measured.width, measured.height};
        if (this->controller_browsing_ && !this->ControllerIndex(effects)) this->EndControllerBrowse();
    }

    auto status_effects_module::ControllerIndex(const effect_list& effects) const -> std::optional<size_t>
    {
        if (!this->controller_browsing_ || !this->state_.enabled || effects.player != this->controller_player_) return std::nullopt;
        size_t occurrence{};
        const auto count = this->Layout(effects).count;
        for (size_t index = 0; index < count; ++index)
            if (effects.entries[index].id == this->controller_effect_ && occurrence++ == this->controller_occurrence_) return index;
        return std::nullopt;
    }

    auto status_effects_module::FocusEffect(const effect_list& effects, const size_t index) -> bool
    {
        if (index >= this->Layout(effects).count) return false;
        this->controller_browsing_ = true;
        this->controller_player_ = effects.player;
        this->controller_effect_ = effects.entries[index].id;
        this->controller_occurrence_ = 0;
        for (size_t previous = 0; previous < index; ++previous)
            if (effects.entries[previous].id == this->controller_effect_) ++this->controller_occurrence_;
        return true;
    }

    auto status_effects_module::BeginControllerBrowse(void) -> bool
    {
        this->EndControllerBrowse();
        return this->state_.enabled && this->FocusEffect(this->Effects(), 0);
    }

    auto status_effects_module::MoveControllerBrowse(const int direction) -> bool
    {
        const auto effects = this->Effects();
        const auto index = this->ControllerIndex(effects);
        if (!index || direction == 0) return false;
        const auto layout = this->Layout(effects);
        const int next = static_cast<int>(*index) + (direction > 0 ? 1 : -1) * static_cast<int>(layout.columns);
        return next >= 0 && this->FocusEffect(effects, static_cast<size_t>(next));
    }

    auto status_effects_module::MoveControllerBrowseHorizontal(const int direction) -> bool
    {
        const auto effects = this->Effects();
        const auto index = this->ControllerIndex(effects);
        if (!index || direction == 0) return false;
        const auto columns = this->Layout(effects).columns;
        const int next = static_cast<int>(*index) + (direction > 0 ? 1 : -1);
        if (next < 0 || static_cast<size_t>(next) / columns != *index / columns) return false;
        return this->FocusEffect(effects, static_cast<size_t>(next));
    }

    auto status_effects_module::EndControllerBrowse(void) -> void
    {
        this->controller_browsing_ = false;
        this->controller_player_ = 0;
        this->controller_effect_ = 0;
        this->controller_occurrence_ = 0;
    }

    auto status_effects_module::ControllerFocused(void) const -> bool
    { return this->ControllerIndex(this->Effects()).has_value(); }

    auto status_effects_module::ControllerBounds(void) const -> ui::rect
    {
        const auto effects = this->Effects();
        const auto index = this->ControllerIndex(effects);
        return index ? this->Layout(effects).Icon(*index) : ui::rect{};
    }

    auto status_effects_module::RenderStatusTooltip(const module_render_context& context, IResourceManager* resources,
        const float x, const float y, const bool controller) const -> void
    {
        const auto& style = this->StyleModule();
        if (!this->state_.enabled || !style.settings_.tooltips) return;
        const auto effects = this->Effects(context.IsEditing());
        std::optional<ui::status_tooltip> tooltip;
        if (controller)
        {
            const auto index = this->ControllerIndex(effects);
            if (!index) return;
            tooltip = ui::status_tooltip{effects.entries[*index], this->Layout(effects).Icon(*index), effects.player};
        }
        else tooltip = ui::StatusStripHit(effects.Effects(), this->StripBounds(), style.settings_, x, y);
        if (!tooltip) return;
        if (resources == nullptr && this->source_ != nullptr) resources = this->source_->Resources();
        ui::DrawStatusTooltip(*tooltip, resources, style.text_[2], context);
    }
}
