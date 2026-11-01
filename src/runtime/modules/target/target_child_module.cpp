#include "target_child_module.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>

namespace xiui
{
    auto target_child_module::ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void
    {
        const auto& source = static_cast<const target_child_module&>(defaults);
        this->parent_.ChildOptions(this->subtarget_) = source.parent_.ChildOptions(source.subtarget_);
        this->State() = source.State();
        if (this->subtarget_)
        {
            this->parent_.subtarget_placement_ = source.parent_.subtarget_placement_;
            this->parent_.subtarget_statuses_ = source.parent_.subtarget_statuses_;
            this->parent_.subtarget_status_text_ = source.parent_.subtarget_status_text_;
        }
    }

    auto target_child_module::State(void) -> module_state&
    {
        auto& state = this->subtarget_ ? this->parent_.subtarget_state_ : this->parent_.recipient_state_;
        state.enabled = this->parent_.ChildOptions(this->subtarget_).enabled;
        return state;
    }

    auto target_child_module::State(void) const -> const module_state&
    { return const_cast<target_child_module*>(this)->State(); }

    auto target_child_module::SetDisplayAllowed(const bool allowed) -> void
    {
        auto& value = this->subtarget_ ? this->parent_.subtarget_display_allowed_ : this->parent_.recipient_display_allowed_;
        value = allowed;
    }

    auto target_child_module::ResetDefaults(const ui::theme&) -> void
    { this->parent_.ResetChild(this->subtarget_); this->State().content_bounds = {}; }

    auto target_child_module::Load(const settings_store& store) -> void
    {
        auto& state = this->State();
        const auto& main = this->parent_.State().bounds;
        const ui::rect fallback = this->subtarget_ ? ui::rect{main.x, main.y + 90, main.width, 82} : ui::rect{main.x + main.width + 64, main.y, 160, 72};
        state.bounds = store.ReadRect(this->LayoutSection(), fallback);
        state.content_bounds = {};
    }

    auto target_child_module::Save(const settings_store& store) const -> void
    { store.WriteRect(this->LayoutSection(), this->State().bounds); }

    auto target_child_module::ContentOptionCount(void) const -> size_t
    { return this->subtarget_ ? target_module::subtarget_option_count : target_of_target_settings::option_count; }

    auto target_child_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static const auto sections = [] {
            std::array<module_config_section, 9> result{};
            std::copy(BarConfigSections().begin(), BarConfigSections().end(), result.begin());
            result[7] = {"Attack Timing", "Uses the main Target attack-timing preference.", "attack-timing"};
            result[8] = {"Status Effects", "Independent subtarget icon layout and countdown presentation.", "status-effects"};
            return result;
        }();
        return this->subtarget_ ? std::span<const module_config_section>{sections} : target_of_target_settings::ConfigSections();
    }

    auto target_module::ResetChild(const bool subtarget) -> void
    {
        auto& options = this->ChildOptions(subtarget);
        options = {};
        options.text = {this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
        options.meter = this->settings_.hp_style;
        options.fill_top = this->settings_.hp_color_top; options.fill_bottom = this->settings_.hp_color;
        if (subtarget)
        {
            options.distance = this->settings_.show_distance;
            options.percent = this->settings_.show_percent;
            options.label = this->settings_.show_label;
            options.cast = options.statuses = options.attack = options.prefix = true;
            options.follow_meter = options.follow_fill = false;
            options.follow_text = false;
            options.arrow = false; options.placement = target_of_target_settings::below; options.gap = 6;
            this->subtarget_placement_ = 0;
            this->subtarget_statuses_ = this->settings_.statuses;
            this->subtarget_status_text_ = this->settings_.status_text;
        }
    }

    auto target_module::ChildIndependent(const bool subtarget) const -> bool
    { return subtarget ? this->subtarget_placement_ == 1 : this->settings_.recipient.placement == target_of_target_settings::detached; }

    auto target_module::ChildVisible(const bool subtarget, const bool editing) const -> bool
    {
        return this->ChildOptions(subtarget).enabled && (subtarget ? this->subtarget_display_allowed_ : this->recipient_display_allowed_) &&
            (editing || this->views_[subtarget ? 1 : 2].id != 0);
    }

    auto target_module::ChildContains(const bool subtarget, const size_t section, const size_t index) const -> bool
    {
        if (subtarget && index >= target_of_target_settings::option_count)
            return index < subtarget_option_count && section == 8;
        if (subtarget && ((index >= 14 && index <= 18) || index == 27 || index == 30)) return false;
        return this->ChildOptions(subtarget).Contains(index, section);
    }

    auto target_module::ChildOption(const bool subtarget, const size_t index) -> module_option
    {
        if (subtarget && ((index >= 14 && index <= 18) || index == 27 || index == 30)) return {};
        auto& options = this->ChildOptions(subtarget);
        if (subtarget && index >= target_of_target_settings::option_count)
            return this->subtarget_statuses_.Option(index - target_of_target_settings::option_count);
        if (index == 8 && this->ChildIndependent(subtarget))
            return ModuleWidthOption(subtarget ? this->subtarget_state_ : this->recipient_state_);
        if (subtarget && index == 13)
        {
            static constexpr const char* placements[]{"Attached", "Independent"};
            return module_option::Choice("Position", "Attach below Target or move Subtarget independently.", &this->subtarget_placement_, placements, 2).Ordered(200);
        }
        if (subtarget && index == 29)
        {
            static constexpr const char* texts[]{"Name", "Distance", "HP label", "HP value", "Status timers and details"};
            return module_option::Element("Selected text", "Choose the independent subtarget text profile.", &options.selected_text, texts, 5);
        }
        if (subtarget && index >= target_of_target_settings::base_options + meter_style_option_count && options.selected_text == 4)
            return TextProfileOption(index - target_of_target_settings::base_options - meter_style_option_count, this->subtarget_status_text_);
        auto option = options.Option(index);
        if (subtarget)
        {
            if (index == 0) option.description = "Show the native spell/ability subtarget without changing game targeting.";
            if (index == 9) option.description = "Zero uses this view's meter height, or the main meter when following its style.";
            if (index == 19) option.description = "Show the native subtarget's name.";
            if (index == 20) option.description = "Add Subtarget: before its name.";
        }
        if (this->ChildIndependent(subtarget) && (index == 10 || index == 11 || index == 12 || index == 30))
            return option.VisibleWhen(false);
        return option;
    }

    auto target_module::RenderChild(const bool subtarget, const module_render_context& context) -> void
    {
        if (!this->ChildIndependent(subtarget) || !this->ChildVisible(subtarget, context.IsEditing())) return;
        const auto slot = subtarget ? 1 : 2;
        auto view = this->views_[slot];
        if (view.id == 0 && context.IsEditing())
        {
            view.id = subtarget ? 0xFFFFFFFE : 0xFFFFFFFF; view.name = "Party Member";
            view.hp = 84; view.health = true; view.distance = 8.2f; view.color = context.Design().GetTheme().colors.text;
        }
        auto& state = subtarget ? this->subtarget_state_ : this->recipient_state_;
        const auto height = this->DrawView(view, state.bounds, subtarget ? "Subtarget: " : "Target: ", this->detail_animations_[slot],
            ::GetTickCount64(), context, &this->ChildOptions(subtarget));
        state.minimum_height = std::max(30.0f, height);
        state.bounds.height = state.minimum_height;
    }
}
