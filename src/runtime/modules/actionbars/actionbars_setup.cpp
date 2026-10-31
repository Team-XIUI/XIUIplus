#include "actionbars_module.hpp"
#include "config/config_controls.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    namespace
    {
        constexpr const char* tab_names[]{"General", "Mappings", "Timing"};
        constexpr const char* preset_names[]{"FFXI bindings", "Xbox", "PlayStation", "Nintendo", "Custom"};
        auto Contains(const ui::rect& bounds, const float x, const float y) -> bool
        { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }

        struct setup_layout
        {
            ui::rect bounds;
            static constexpr float row_height = 28, row_pitch = 36;
            auto Left(void) const -> float { return this->bounds.x + 16; }
            auto Width(void) const -> float { return std::max(0.0f, this->bounds.width - 32); }
            auto Top(void) const -> float { return this->bounds.y + 88; }
            auto Row(const float offset) const -> ui::rect { return {this->Left(), this->Top() + offset, this->Width(), row_height}; }
            auto Column(const int column, const float offset) const -> ui::rect
            {
                const float width = (this->Width() - 24) * 0.5f;
                return {this->Left() + static_cast<float>(column) * (width + 24), this->Top() + offset, width, row_height};
            }
            auto Value(const ui::rect& row, const bool compact = false) const -> ui::rect
            {
                const float labelWidth = row.width < this->Width() ? 80.0f : 180.0f;
                const float width = compact ? 76.0f : std::min(248.0f, row.width - labelWidth);
                return {row.x + labelWidth, row.y, width, row.height};
            }
            auto Done(void) const -> ui::rect { return {this->Left() + this->Width() - 96, this->bounds.y + this->bounds.height - 44, 96, row_height}; }
        };

        auto StandardMapping(const int preset) -> controller_crossbar_mapping
        {
            if (preset == 1) return {9, 12, -2, {6, 4, 7, 5, 2, 0, 3, 1}};
            if (preset == 2) return {6, 7, -2, {41, 40, 41, 40, 3, 2, 1, 0}};
            if (preset == 3) return {6, 7, -2, {41, 40, 41, 40, 3, 1, 0, 2}};
            return {};
        }

        auto PresetFor(controller_crossbar_mapping mapping, const int backend) -> int
        {
            if (mapping == controller_crossbar_mapping{}) return 0;
            mapping.palette_modifier = -2;
            for (int preset = 1; preset <= 3; ++preset)
                if ((backend == 0) == (preset == 1) && mapping == StandardMapping(preset)) return preset;
            return 4;
        }

        auto ShoulderSide(const controller_crossbar_mapping& mapping, const int backend, const bool preferLeft) -> int
        {
            if (mapping.palette_modifier == -2) return preferLeft ? 0 : 1;
            if (mapping.palette_modifier == (backend == 0 ? 8 : 4)) return 0;
            if (mapping.palette_modifier == (backend == 0 ? 11 : 5)) return 1;
            return 2;
        }

        auto MappingName(const int value, const int backend, const int preset) -> std::string
        {
            if (value == -2) return "Auto";
            if (value == -1) return "Unbound";
            if (backend == 0)
            {
                constexpr const char* names[]{"B", "X", "Y", "A", "D-pad R", "D-pad L", "D-pad U", "D-pad D", "LB", "LT", "L3", "RB", "RT", "R3", "Start", "Back"};
                if (value >= 0 && value < static_cast<int>(std::size(names))) return names[value];
            }
            else
            {
                if (value == 40 || value == -40) return value < 0 ? "POV -X" : "POV +X";
                if (value == 41 || value == -41) return value < 0 ? "POV -Y" : "POV +Y";
                if (value >= 0 && value < 8 && (preset == 2 || preset == 3))
                {
                    constexpr const char* playstation[]{"Square", "Cross", "Circle", "Triangle", "L1", "R1", "L2", "R2"};
                    constexpr const char* nintendo[]{"B", "A", "Y", "X", "L", "R", "ZL", "ZR"};
                    return preset == 2 ? playstation[value] : nintendo[value];
                }
                if (value >= 0) return "Button " + std::to_string(value + 1);
            }
            return "Binding " + std::to_string(value);
        }

    }

    auto actionbars_module::KeybindMappingLabel(const size_t slot) const -> std::string
    {
        const auto& mapping = this->Mapping();
        const int preset = this->setup_preset_ < 0 ? PresetFor(mapping, this->setup_backend_) : this->setup_preset_;
        const auto value = mapping.slots[slot];
        const auto backend = this->setup_backend_ == 0 ? controller_backend::xinput : controller_backend::direct_input;
        if (value == -2 && this->controller_status_.crossbar_mapping_known && this->controller_status_.backend == backend)
            return "Auto: " + MappingName(this->controller_status_.crossbar_mapping.slots[slot], this->setup_backend_, preset);
        return MappingName(value, this->setup_backend_, preset);
    }

    auto actionbars_module::SetupControls(const ui::rect& bounds) const -> std::vector<control>
    {
        std::vector<control> controls;
        const setup_layout layout{bounds};
        const int tab = std::clamp(this->setup_step_, 0, 2);
        const float tabWidth = (layout.Width() - 16) / 3;
        for (int index = 0; index < 3; ++index)
            controls.push_back({tab_first + index, {layout.Left() + static_cast<float>(index) * (tabWidth + 8), bounds.y + 44, tabWidth, 28}, tab_names[index], {}, control_kind::button, true, index == tab});
        int labelId = label_first;
        const auto label = [&](const ui::rect& at, const char* caption) {
            controls.push_back({labelId++, at, caption, {}, control_kind::label, false});
        };
        const auto heading = [&](ui::rect at, const char* caption) {
            at.height = 20;
            label(at, caption);
        };
        const auto choice = [&](const int id, const ui::rect& row, const char* caption, const std::string& value, const bool enabled = true) {
            const auto at = layout.Value(row);
            label({row.x, row.y, at.x - row.x - 10, row.height}, caption);
            controls.push_back({id, at, value, {}, control_kind::dropdown, enabled});
        };
        const auto toggle = [&](const int id, const ui::rect& row, const char* caption, const bool on) {
            const auto at = layout.Value(row, true);
            label({row.x, row.y, at.x - row.x - 10, row.height}, caption);
            controls.push_back({id, at, on ? "On" : "Off", {}, control_kind::toggle, true, on});
        };
        const auto slider = [&](const int id, ui::rect row, const char* caption, const float value, const float minimum, const float maximum, const float step, const bool enabled = true) {
            row.width = std::min(row.width, 460.0f);
            control item{id, row, caption, {}, control_kind::slider, enabled};
            item.value = value; item.minimum = minimum; item.maximum = maximum; item.step = step;
            item.label_width = row.width < 400 ? 80.0f : 180.0f;
            controls.push_back(std::move(item));
        };
        const auto& mapping = this->Mapping();
        const int preset = this->setup_preset_ < 0 ? PresetFor(mapping, this->setup_backend_) : std::clamp(this->setup_preset_, 0, 4);
        if (tab == 0)
        {
            toggle(use_triggers, layout.Row(0), "Use XIUI triggers", this->controller_settings_.enabled && this->controller_settings_.replace_native);
            choice(mapping_backend, layout.Row(setup_layout::row_pitch), "Input type", this->setup_backend_ == 0 ? "XInput" : "DirectInput");
            choice(mapping_preset, layout.Row(2 * setup_layout::row_pitch), "Controller preset", preset_names[preset]);
            toggle(ordered_banks, layout.Row(3 * setup_layout::row_pitch), "Ordered trigger banks", this->controller_settings_.expanded);
            toggle(double_banks, layout.Row(4 * setup_layout::row_pitch), "Double-tap banks", this->controller_settings_.double_tap);
        }
        else if (tab == 1)
        {
            heading(layout.Row(0), "Triggers");
            choice(mapping_first, layout.Column(0, 24), "Left", MappingName(mapping.left_trigger, this->setup_backend_, preset));
            choice(mapping_first + 1, layout.Column(1, 24), "Right", MappingName(mapping.right_trigger, this->setup_backend_, preset));
            choice(mapping_first + 2, layout.Row(66), "Palette shoulder", MappingName(mapping.palette_modifier, this->setup_backend_, preset));
            heading(layout.Column(0, 110), "D-pad");
            heading(layout.Column(1, 110), "Face buttons");
            constexpr const char* directions[]{"Up", "Right", "Down", "Left"};
            constexpr const char* faces[]{"Top", "Right", "Bottom", "Left"};
            for (int index = 0; index < 4; ++index)
            {
                const float offset = 142 + static_cast<float>(index) * 34;
                choice(mapping_first + 3 + index, layout.Column(0, offset), directions[index], MappingName(mapping.slots[index], this->setup_backend_, preset));
                choice(mapping_first + 7 + index, layout.Column(1, offset), faces[index], MappingName(mapping.slots[index + 4], this->setup_backend_, preset));
            }
        }
        else
        {
            float offset{};
            if (this->controller_settings_.double_tap)
            {
                slider(setup_double_window, layout.Row(offset), "Double-tap window (ms)", this->controller_settings_.double_tap_ms, 100, 1000, 25);
                slider(minimum_hold, layout.Row(offset + setup_layout::row_pitch), "First-tap hold (ms)", this->controller_settings_.min_hold_ms, 0, 500, 10);
                offset += 84;
            }
            heading(layout.Row(offset), "Palette switching");
            toggle(setup_palette_cycle, layout.Row(offset + 24), "Shoulder + D-pad", this->controller_settings_.palette_cycle);
            offset += 60;
            if (this->controller_settings_.palette_cycle)
            {
                const int shoulder = ShoulderSide(mapping, this->setup_backend_, this->controller_settings_.palette_left_shoulder);
                const auto shoulderName = shoulder == 0 ? std::string{"Left shoulder"} : shoulder == 1 ? std::string{"Right shoulder"} : "Custom: " + MappingName(mapping.palette_modifier, this->setup_backend_, preset);
                choice(palette_shoulder, layout.Row(offset), "Palette shoulder", shoulderName);
                offset += setup_layout::row_pitch;
            }
            if (this->setup_backend_ == 0)
            {
                offset += 8;
                heading(layout.Row(offset), "Analog triggers");
                slider(trigger_press, layout.Column(0, offset + 24), "Press", this->controller_settings_.trigger_press, 1, 255, 1);
                slider(trigger_release, layout.Column(1, offset + 24), "Release", this->controller_settings_.trigger_release, 0, static_cast<float>(this->controller_settings_.trigger_press - 1), 1);
            }
        }
        controls.push_back({close_setup, layout.Done(), "Done", {}, control_kind::button});
        return controls;
    }

    auto actionbars_module::SetupSliderOption(const control& item, float& value) const -> module_option
    {
        auto option = module_option::Slider(item.text.c_str(), "", &value, item.minimum, item.maximum, item.step);
        option.available = item.enabled;
        return option;
    }

    auto actionbars_module::SetSetupSlider(const int id, const float value) -> void
    {
        const auto controls = this->Controls();
        const auto item = std::find_if(controls.begin(), controls.end(), [&](const auto& item) { return item.id == id && item.enabled && item.kind == control_kind::slider; });
        if (item == controls.end() || !std::isfinite(value)) return;
        const auto normalized = static_cast<uint16_t>(ui::slider_value_input::Normalize(value, item->minimum, item->maximum, item->step));
        auto& settings = this->controller_settings_;
        auto* target = id == trigger_press ? &settings.trigger_press : id == trigger_release ? &settings.trigger_release : id == setup_double_window ? &settings.double_tap_ms : &settings.min_hold_ms;
        if (*target == normalized) return;
        *target = normalized;
        settings.trigger_release = std::min<uint16_t>(settings.trigger_release, settings.trigger_press - 1);
        this->settings_changed_ = true;
    }

    auto actionbars_module::OpenSetupValue(const control& item) -> void
    {
        this->setup_slider_adjust_ = 0; this->setup_control_ = item.id; this->setup_value_ = item.value;
        this->setup_value_input_.Open(this->SetupSliderOption(item, this->setup_value_));
    }

    auto actionbars_module::FinishSetupValue(const bool apply, const bool keepInvalid) -> bool
    {
        if (!this->setup_value_input_.Focused()) return true;
        if (apply)
        {
            const auto value = this->setup_value_input_.Value();
            if (!value && keepInvalid) return false;
            if (value) this->SetSetupSlider(this->setup_control_, *value);
        }
        this->setup_value_input_.Cancel();
        return true;
    }

    auto actionbars_module::OpenSetupChoice(const control& item) -> void
    {
        if (this->dialog_ == dialog::editor) this->CommitField();
        this->FinishSetupValue(true); this->setup_slider_adjust_ = 0;
        this->setup_choice_input_.Cancel(); this->setup_choices_.clear(); this->setup_choice_labels_.clear();
        this->setup_control_ = item.id;
        const auto add = [&](const int value, std::string label) { this->setup_choices_.emplace_back(value, std::move(label)); };
        int current{};
        const auto& mapping = this->Mapping();
        const int preset = this->setup_preset_ < 0 ? PresetFor(mapping, this->setup_backend_) : this->setup_preset_;
        if (auto choices = this->EditorChoices(item.id); !choices.empty())
        { this->setup_choices_ = std::move(choices); current = this->EditorChoiceValue(item.id); }
        else if (item.id == mapping_backend)
        {
            add(0, "XInput"); add(1, "DirectInput"); current = this->setup_backend_;
        }
        else if (item.id == keybind_bar && this->dialog_ == dialog::keybinds)
        {
            for (size_t bar = 0; bar < action_bar_count; ++bar) add(static_cast<int>(bar), "Hotbar " + std::to_string(bar + 1));
            add(static_cast<int>(action_bar_count), "Crossbar");
            current = this->selected_.surface == action_surface::crossbar ? static_cast<int>(action_bar_count) : static_cast<int>(this->selected_.bar);
        }
        else if (item.id == keybind_bank && this->dialog_ == dialog::keybinds)
        {
            constexpr const char* names[]{"Left trigger", "Right trigger", "Left then right", "Right then left", "Double left", "Double right", "Shared expanded"};
            for (int bank = 0; bank < static_cast<int>(action_crossbar_banks); ++bank) add(bank, names[bank]);
            current = static_cast<int>(this->selected_.bar);
        }
        else if (item.id == mapping_preset)
        {
            for (int index = 0; index < 5; ++index) add(index, preset_names[index]);
            current = preset;
        }
        else if (item.id == palette_shoulder)
        {
            add(0, "Left shoulder"); add(1, "Right shoulder");
            current = ShoulderSide(mapping, this->setup_backend_, this->controller_settings_.palette_left_shoulder);
            if (current == 2) add(2, "Custom: " + MappingName(mapping.palette_modifier, this->setup_backend_, preset));
        }
        else if (item.id >= mapping_first && item.id < mapping_first + 11)
        {
            const int index = item.id - mapping_first;
            current = index == 0 ? mapping.left_trigger : index == 1 ? mapping.right_trigger : index == 2 ? mapping.palette_modifier : mapping.slots[index - 3];
            add(-2, "Auto"); add(-1, "Unbound");
            for (int value = 0; value <= (this->setup_backend_ == 0 ? 15 : 127); ++value)
                if (this->setup_backend_ == 0 || value < 32 || value > 41) add(value, MappingName(value, this->setup_backend_, preset));
            if (this->setup_backend_ == 1 && index >= 3 && index <= 6)
                for (const int value : {40, 41, -40, -41}) add(value, MappingName(value, this->setup_backend_, preset));
            if (std::none_of(this->setup_choices_.begin(), this->setup_choices_.end(), [&](const auto& choice) { return choice.first == current; }))
                add(current, "Current binding " + std::to_string(current));
        }
        if (this->setup_choices_.empty()) return;
        this->setup_choice_index_ = 0;
        for (size_t index = 0; index < this->setup_choices_.size(); ++index)
        {
            this->setup_choice_labels_.push_back(this->setup_choices_[index].second.c_str());
            if (this->setup_choices_[index].first == current) this->setup_choice_index_ = static_cast<int>(index);
        }
        this->setup_choice_input_.Open(module_option::Dropdown("", "", &this->setup_choice_index_, this->setup_choice_labels_.data(), this->setup_choice_labels_.size()), item.bounds);
        if (this->sounds_) this->sounds_->Select(this->now_);
    }

    auto actionbars_module::SelectSetupChoice(const int index) -> void
    {
        if (!this->setup_choice_input_.Opened() || index < 0 || static_cast<size_t>(index) >= this->setup_choices_.size()) return;
        const int value = this->setup_choices_[index].first;
        const bool changed = index != this->setup_choice_index_;
        this->setup_choice_input_.Cancel(); this->setup_choice_pressed_.reset();
        if (!changed) return;
        if (this->SelectEditorChoice(this->setup_control_, value)) return;
        if (this->setup_control_ == keybind_bar && this->dialog_ == dialog::keybinds)
        {
            this->OpenKeybinds(value == static_cast<int>(action_bar_count), value == static_cast<int>(action_bar_count) ? 0 : static_cast<size_t>(value));
            this->focused_control_ = keybind_bar;
            return;
        }
        if (this->setup_control_ == keybind_bank && this->dialog_ == dialog::keybinds)
        {
            this->selected_.bar = static_cast<size_t>(value);
            this->selected_bank_ = value;
            this->palette_scroll_ = 0;
            this->SyncStorage();
            return;
        }
        if (this->setup_control_ == mapping_backend)
        { this->setup_backend_ = value; this->setup_preset_ = -1; return; }
        if (this->setup_control_ == mapping_preset)
        {
            this->setup_preset_ = value;
            if (value == 0) this->Mapping() = {};
            else if (value >= 1 && value <= 3)
            { this->setup_backend_ = value == 1 ? 0 : 1; this->Mapping() = StandardMapping(value); }
        }
        else if (this->setup_control_ == palette_shoulder && value < 2)
        { this->controller_settings_.palette_left_shoulder = value == 0; this->Mapping().palette_modifier = -2; }
        else if (this->setup_control_ >= mapping_first && this->setup_control_ < mapping_first + 11)
        {
            auto& mapping = this->Mapping(); const int slot = this->setup_control_ - mapping_first;
            auto& target = slot == 0 ? mapping.left_trigger : slot == 1 ? mapping.right_trigger : slot == 2 ? mapping.palette_modifier : mapping.slots[slot - 3];
            target = static_cast<int16_t>(value); this->setup_preset_ = 4;
        }
        this->settings_changed_ = true;
    }

    auto actionbars_module::CancelSetupControls(void) -> void
    {
        this->setup_value_input_.Cancel(); this->setup_choice_input_.Cancel(); this->setup_choice_pressed_.reset();
        this->setup_slider_drag_ = this->setup_slider_adjust_ = 0;
    }

    auto actionbars_module::SetupViewport(void) const -> D3DVIEWPORT8
    {
        return {static_cast<DWORD>(std::max(0.0f, this->viewport_.x)), static_cast<DWORD>(std::max(0.0f, this->viewport_.y)),
            static_cast<DWORD>(std::max(1.0f, this->viewport_.width)), static_cast<DWORD>(std::max(1.0f, this->viewport_.height)), 0, 1};
    }

    auto actionbars_module::HandleSetupMouse(const uint32_t message, const WPARAM buttons, const float x, const float y) -> bool
    {
        if (this->dialog_ != dialog::setup && this->dialog_ != dialog::keybinds && !this->setup_choice_input_.Opened()) return false;
        const bool press = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK;
        if (this->setup_choice_input_.Opened())
        {
            if (this->setup_choice_input_.HandleScrollPointer(message, x, y, this->SetupViewport()))
            {
                this->setup_choice_pressed_.reset(); this->navigation_focus_ = false;
                if (press) this->pointer_down_ = true;
                else if (message == WM_LBUTTONUP) this->pointer_down_ = false;
                return true;
            }
            if (message == WM_MOUSEWHEEL) this->setup_choice_input_.Scroll(GET_WHEEL_DELTA_WPARAM(buttons) > 0 ? -3 : 3);
            else if (message == WM_RBUTTONDOWN) { this->setup_choice_input_.Cancel(); this->setup_choice_pressed_.reset(); }
            else if (press)
            {
                this->navigation_focus_ = false; this->pointer_down_ = true;
                this->setup_choice_pressed_ = this->setup_choice_input_.IndexAt(x, y, this->SetupViewport());
                if (!this->setup_choice_pressed_) { this->setup_choice_input_.Cancel(); this->pressed_control_ = 0; }
            }
            else if (message == WM_LBUTTONUP && this->pointer_down_)
            {
                this->pointer_down_ = false;
                const auto index = this->setup_choice_input_.IndexAt(x, y, this->SetupViewport());
                if (index && index == this->setup_choice_pressed_) { this->SelectSetupChoice(*index); if (this->sounds_) this->sounds_->Select(this->now_); }
                this->setup_choice_pressed_.reset();
            }
            return true;
        }
        if (press && this->setup_value_input_.Focused()) this->FinishSetupValue(true);
        const auto controls = this->Controls();
        const auto item = std::find_if(controls.begin(), controls.end(), [&](const auto& item) {
            return item.kind == control_kind::slider && item.enabled && (this->setup_slider_drag_ ? item.id == this->setup_slider_drag_ : Contains(item.bounds, x, y));
        });
        if (item == controls.end()) return false;
        if (press)
        {
            this->navigation_focus_ = false; this->setup_slider_adjust_ = 0; this->focused_control_ = item->id;
            if (Contains(ui::config_layout::SliderValue(item->bounds), x, y)) { this->OpenSetupValue(*item); this->pointer_down_ = true; this->pressed_control_ = 0; return true; }
            const auto track = ui::config_layout::SliderTrack(item->bounds, item->label_width);
            if (x < track.x) return true;
            this->setup_slider_drag_ = item->id; this->pointer_down_ = true;
        }
        if (this->setup_slider_drag_ && (press || message == WM_MOUSEMOVE || message == WM_LBUTTONUP))
        {
            const auto track = ui::config_layout::SliderTrack(item->bounds, item->label_width);
            this->SetSetupSlider(item->id, item->minimum + (item->maximum - item->minimum) * std::clamp((x - track.x) / track.width, 0.0f, 1.0f));
            if (message == WM_LBUTTONUP) { this->setup_slider_drag_ = 0; this->pointer_down_ = false; }
            return true;
        }
        return false;
    }

    auto actionbars_module::HandleSetupController(const controller_action action, const bool repeated) -> bool
    {
        if (this->dialog_ != dialog::setup && this->dialog_ != dialog::keybinds && !this->setup_choice_input_.Opened()) return false;
        if (this->setup_choice_input_.Opened())
        {
            if (action == controller_action::cancel || action == controller_action::next_tab || action == controller_action::previous_tab) this->setup_choice_input_.Cancel();
            else if (action == controller_action::confirm && !repeated) this->SelectSetupChoice(this->setup_choice_input_.Highlighted());
            else if (action == controller_action::up || action == controller_action::down) this->setup_choice_input_.Move(action == controller_action::up ? -1 : 1);
            return true;
        }
        if (this->setup_value_input_.Focused())
        {
            if (action == controller_action::cancel) this->FinishSetupValue(false);
            else if (action == controller_action::confirm && !repeated) this->FinishSetupValue(true, true);
            return true;
        }
        const auto controls = this->Controls();
        const auto item = std::find_if(controls.begin(), controls.end(), [&](const auto& item) { return item.id == this->focused_control_ && item.enabled && item.kind == control_kind::slider; });
        if (item == controls.end()) { this->setup_slider_adjust_ = 0; return false; }
        if (action == controller_action::actions && !repeated) { this->OpenSetupValue(*item); return true; }
        if (action == controller_action::confirm && !repeated) { this->setup_slider_adjust_ = this->setup_slider_adjust_ ? 0 : item->id; return true; }
        if (this->setup_slider_adjust_)
        {
            if (action == controller_action::cancel) this->setup_slider_adjust_ = 0;
            else if (action == controller_action::left || action == controller_action::right) this->SetSetupSlider(item->id, item->value + (action == controller_action::left ? -item->step : item->step));
            else if (action == controller_action::up || action == controller_action::down || action == controller_action::next_tab || action == controller_action::previous_tab) { this->setup_slider_adjust_ = 0; return false; }
            return true;
        }
        return false;
    }

    auto actionbars_module::DrawSetupChoice(const module_render_context& context) const -> void
    {
        if (!this->setup_choice_input_.Opened()) return;
        ui::config_controls controls{context.Design(), context.Containers(), context.NativeAssets()};
        if (this->config_text_) controls.SetTextSettings(*this->config_text_);
        controls.ChoiceMenu(this->setup_choice_input_, this->SetupViewport(), this->hover_x_, this->hover_y_);
    }

    auto actionbars_module::ActivateSetupControl(const int id, const int) -> bool
    {
        if (this->dialog_ != dialog::setup) return false;
        if (id >= tab_first && id < tab_first + 3)
        {
            this->FinishSetupValue(true);
            this->CancelSetupControls();
            this->setup_step_ = id - tab_first;
            this->focused_control_ = id;
            return true;
        }
        switch (id)
        {
        case use_triggers:
        {
            const bool enabled = !(this->controller_settings_.enabled && this->controller_settings_.replace_native);
            this->controller_settings_.enabled = this->controller_settings_.replace_native = this->trigger_controls_ = enabled;
            if (enabled) this->bars_[action_bar_count].state.enabled = true;
            this->active_bank_ = crossbar_bank::none; this->triggers_ = 0; this->held_slots_ = {};
            break;
        }
        case ordered_banks: this->controller_settings_.expanded = !this->controller_settings_.expanded; break;
        case double_banks: this->controller_settings_.double_tap = !this->controller_settings_.double_tap; break;
        case setup_palette_cycle: this->controller_settings_.palette_cycle = !this->controller_settings_.palette_cycle; break;
        default: return false;
        }
        this->settings_changed_ = true;
        return true;
    }

}
