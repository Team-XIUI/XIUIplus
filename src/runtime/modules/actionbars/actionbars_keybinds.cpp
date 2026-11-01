#include "actionbars_module.hpp"

#include <algorithm>

namespace xiui
{
    namespace
    {
        constexpr const char* slot_names[]{"D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left", "Face Top", "Face Right", "Face Bottom", "Face Left"};
        constexpr const char* banks[]{"Left trigger", "Right trigger", "Left then right", "Right then left", "Double left", "Double right", "Shared expanded"};
    }

    auto actionbars_module::OpenKeybinds(const bool crossbar, const size_t bar) -> void
    {
        this->library_popup_.reset();
        this->CommitField();
        this->CancelSetupControls();
        this->binding_capture_ = false;
        this->selected_ = {crossbar ? action_surface::crossbar : action_surface::hotbar,
            std::min(bar, crossbar ? action_crossbar_banks - 1 : action_bar_count - 1), 0};
        this->view_ = crossbar ? action_bar_count : this->selected_.bar;
        if (crossbar) this->selected_bank_ = static_cast<int>(this->selected_.bar);
        this->dialog_ = dialog::keybinds;
        this->palette_scroll_ = 0;
        this->setup_backend_ = this->controller_status_.backend == controller_backend::direct_input ? 1 : 0;
        this->setup_preset_ = -1;
        this->focused_control_ = crossbar ? slot_mapping_first : keybind_key_first;
        this->message_.clear();
        this->SetWindowFocused(true);
        this->SyncStorage();
        if (this->sounds_) this->sounds_->MenuOpened(this->now_);
    }

    auto actionbars_module::KeybindRows(void) const -> size_t
    {
        const bool crossbar = this->selected_.surface == action_surface::crossbar;
        const float top = crossbar ? 150.0f : 116.0f;
        return static_cast<size_t>(std::clamp(static_cast<int>((this->DialogBounds().height - top - 60) / 32), 1,
            static_cast<int>(crossbar ? action_crossbar_slots : action_bar_slots)));
    }

    auto actionbars_module::KeybindControls(const ui::rect& bounds) const -> std::vector<control>
    {
        std::vector<control> controls;
        this->AddWindowControls(controls);
        const bool crossbar = this->selected_.surface == action_surface::crossbar;
        const float left = bounds.x + 16, right = bounds.x + bounds.width - 28;
        controls.push_back({8000, {left, bounds.y + 44, 46, 28}, "Bar", {}, control_kind::label, false});
        controls.push_back({keybind_bar, {left + 54, bounds.y + 44, right - left - 54, 28},
            crossbar ? "Crossbar" : "Hotbar " + std::to_string(this->selected_.bar + 1), {}, control_kind::dropdown});
        if (crossbar)
        {
            const float half = (right - left - 12) / 2;
            controls.push_back({keybind_bank, {left, bounds.y + 80, half, 28}, banks[this->selected_.bar], {}, control_kind::dropdown});
            controls.push_back({mapping_backend, {left + half + 12, bounds.y + 80, half, 28},
                this->setup_backend_ == 0 ? "XInput" : "DirectInput", {}, control_kind::dropdown});
            controls.push_back({8001, {left, bounds.y + 116, right - left, 24}, "Button mappings apply to every trigger bank.", {}, control_kind::label, false});
        }
        else
            controls.push_back({8001, {left, bounds.y + 80, right - left, 24}, "Choose a slot, then press a key combination. Escape cancels.", {}, control_kind::label, false});

        const size_t count = crossbar ? action_crossbar_slots : action_bar_slots;
        const auto scroll = std::min(this->palette_scroll_, count - this->KeybindRows());
        for (size_t row = 0; row < this->KeybindRows(); ++row)
        {
            const size_t slot = scroll + row;
            const float y = bounds.y + (crossbar ? 150 : 116) + static_cast<float>(row) * 32;
            const float bindingX = right - 232;
            std::string label = crossbar ? slot_names[slot] : "Slot " + std::to_string(slot + 1);
            if (this->store_ != nullptr)
            {
                const auto& action = this->store_->Resolve(this->selected_.surface, this->selected_.bar, slot);
                const auto name = !action.label.empty() ? action.label : !action.name.empty() ? action.name : action.Empty() ? "Empty" : ActionKindName(action.kind);
                label += "  " + name;
            }
            controls.push_back({8100 + static_cast<int>(slot), {left, y, bindingX - left - 8, 28}, label, {}, control_kind::label, false});
            if (crossbar)
                controls.push_back({slot_mapping_first + static_cast<int>(slot), {bindingX, y, 160, 28}, this->KeybindMappingLabel(slot), {}, control_kind::dropdown});
            else
            {
                const address address{action_surface::hotbar, this->selected_.bar, slot};
                const auto caption = this->binding_capture_ && this->selected_.slot == slot ? "Press a key..." : BindingLabel(this->Binding(address));
                controls.push_back({keybind_key_first + static_cast<int>(slot), {bindingX, y, 160, 28}, caption, {}, control_kind::button});
            }
            const bool bound = crossbar ? this->Mapping().slots[slot] != -1 : this->bars_[this->selected_.bar].keys[slot].key != 0;
            controls.push_back({keybind_clear_first + static_cast<int>(slot), {right - 64, y, 64, 28}, "Clear", {}, control_kind::button, bound});
        }
        controls.push_back({close_setup, {right - 96, bounds.y + bounds.height - 44, 96, 28}, "Done", {}, control_kind::button});
        return controls;
    }

    auto actionbars_module::ActivateKeybindControl(const int id, const int) -> bool
    {
        if (this->dialog_ != dialog::keybinds) return false;
        if (id >= keybind_key_first && id < keybind_key_first + static_cast<int>(action_bar_slots) && this->selected_.surface == action_surface::hotbar)
        {
            this->selected_.slot = static_cast<size_t>(id - keybind_key_first);
            this->binding_capture_ = true;
            this->Notice("Press a key combination. Escape cancels.");
            return true;
        }
        const size_t count = this->selected_.surface == action_surface::crossbar ? action_crossbar_slots : action_bar_slots;
        if (id < keybind_clear_first || id >= keybind_clear_first + static_cast<int>(count)) return false;
        const auto slot = static_cast<size_t>(id - keybind_clear_first);
        this->binding_capture_ = false;
        if (this->selected_.surface == action_surface::crossbar)
        {
            this->Mapping().slots[slot] = -1;
            this->setup_preset_ = 4;
        }
        else this->bars_[this->selected_.bar].keys[slot] = {};
        this->settings_changed_ = true;
        this->message_.clear();
        return true;
    }

    auto actionbars_module::MoveKeybindControl(const int, const int dy) -> bool
    {
        if (this->dialog_ != dialog::keybinds || dy == 0) return false;
        const bool crossbar = this->selected_.surface == action_surface::crossbar;
        const int first = crossbar ? slot_mapping_first : keybind_key_first;
        const int count = static_cast<int>(crossbar ? action_crossbar_slots : action_bar_slots);
        const bool clear = this->focused_control_ >= keybind_clear_first && this->focused_control_ < keybind_clear_first + count;
        const int slot = this->focused_control_ - (clear ? keybind_clear_first : first);
        if (slot < 0 || slot >= count || slot + dy < 0 || slot + dy >= count) return false;
        const int next = slot + dy;
        const bool bound = crossbar ? this->Mapping().slots[next] != -1 : this->bars_[this->selected_.bar].keys[next].key != 0;
        this->focused_control_ = (clear && bound ? keybind_clear_first : first) + next;
        if (static_cast<size_t>(next) < this->palette_scroll_) this->palette_scroll_ = static_cast<size_t>(next);
        else if (static_cast<size_t>(next) >= this->palette_scroll_ + this->KeybindRows())
            this->palette_scroll_ = static_cast<size_t>(next) - this->KeybindRows() + 1;
        return true;
    }
}
