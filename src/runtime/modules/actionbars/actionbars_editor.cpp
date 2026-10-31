#include "actionbars_module.hpp"

#include <algorithm>

namespace xiui
{
    namespace
    {
        constexpr const char* editor_targets[]{"<t>", "<me>", "<stpc>", "<stnpc>", "<st>", "<bt>", "<lastst>", "<stal>", "<stpt>", "<pet>", "<p0>", "<p1>", "<p2>", "<p3>", "<p4>", "<p5>"};
        constexpr action_kind recast_kinds[]{action_kind::none, action_kind::none, action_kind::spell, action_kind::ability, action_kind::weapon_skill, action_kind::pet, action_kind::item};
    }

    auto actionbars_module::EditorChoices(const int id) const -> std::vector<std::pair<int, std::string>>
    {
        if (this->dialog_ != dialog::editor) return {};
        std::vector<std::pair<int, std::string>> choices;
        if (id == command_kind)
            for (int kind = 1; kind <= 9; ++kind) choices.emplace_back(kind, ActionKindName(static_cast<action_kind>(kind)));
        else if (id == command_target)
            for (int target = 0; target < static_cast<int>(std::size(editor_targets)); ++target) choices.emplace_back(target, editor_targets[target]);
        else if (id == command_recast)
            for (int kind = 0; kind < static_cast<int>(std::size(recast_kinds)); ++kind)
                choices.emplace_back(kind, kind == 0 ? "Automatic" : kind == 1 ? "None" : ActionKindName(recast_kinds[kind]));
        return choices;
    }

    auto actionbars_module::EditorChoiceValue(const int id) const -> int
    {
        if (id == command_kind) return static_cast<int>(this->draft_.kind);
        if (id == command_target)
        {
            const auto found = std::find(std::begin(editor_targets), std::end(editor_targets), this->draft_.target);
            return found == std::end(editor_targets) ? 0 : static_cast<int>(found - std::begin(editor_targets));
        }
        if (id == command_recast)
        {
            if (this->draft_.recast_disabled) return 1;
            if (this->draft_.recast_kind == action_kind::none) return 0;
            const auto found = std::find(std::begin(recast_kinds) + 2, std::end(recast_kinds), this->draft_.recast_kind);
            return found == std::end(recast_kinds) ? 0 : static_cast<int>(found - std::begin(recast_kinds));
        }
        return 0;
    }

    auto actionbars_module::SelectEditorChoice(const int id, const int value) -> bool
    {
        if (this->dialog_ != dialog::editor) return false;
        this->CommitField();
        if (id == command_kind && value >= 1 && value <= 9)
        {
            this->draft_.kind = static_cast<action_kind>(value); this->draft_.id = 0; this->draft_.name.clear(); this->line_scroll_ = 0;
            if ((this->draft_.kind == action_kind::macro || this->draft_.kind == action_kind::command) && this->draft_.lines.empty()) this->draft_.lines.emplace_back();
        }
        else if (id == command_target && value >= 0 && value < static_cast<int>(std::size(editor_targets))) this->draft_.target = editor_targets[value];
        else if (id == command_recast && value >= 0 && value < static_cast<int>(std::size(recast_kinds)))
        { this->draft_.recast_disabled = value == 1; this->draft_.recast_kind = recast_kinds[value]; this->draft_.recast_name.clear(); this->draft_.recast_id = 0; }
        else return false;
        return true;
    }
    auto actionbars_module::EditorLayout(void) const -> editor_layout
    {
        const auto bounds = this->DialogBounds();
        const bool compact = bounds.height < 500;
        const float left = bounds.x + 16, width = bounds.width - 32;
        const float gap = compact ? 6.0f : 12.0f, heading = compact ? 14.0f : 18.0f;
        editor_layout layout;
        layout.pitch = compact ? 24 : 26;
        layout.icon = {left + width - (compact ? 48 : 64), bounds.y + 44, compact ? 48.0f : 64.0f, compact ? 48.0f : 64.0f};
        layout.type = {left, bounds.y + 64, std::min(240.0f, width - layout.icon.width - 24), 28};
        layout.footer = {left, bounds.y + bounds.height - 44, width, 28};
        layout.commands = {left, bounds.y + (compact ? 136 : 174), width, 0};
        const float recastHeight = this->editor_recast_ ? 68.0f : 0;
        const float tail = gap + 26 + recastHeight + gap + heading + 28 + 16;
        const int maximum = this->draft_.kind == action_kind::command ? 1 : 8;
        layout.rows = static_cast<size_t>(std::clamp(static_cast<int>((layout.footer.y - layout.commands.y - tail) / layout.pitch), 1, maximum));
        layout.commands.height = layout.rows * layout.pitch;
        layout.recast = {left, layout.commands.y + layout.commands.height + gap, width, 26};
        layout.label = {left, layout.recast.y + layout.recast.height + recastHeight + gap + heading, width, 28};
        return layout;
    }

    auto actionbars_module::EditorControls(void) const -> std::vector<control>
    {
        const auto bounds = this->DialogBounds(); const auto layout = this->EditorLayout();
        const float left = bounds.x + 16, right = bounds.x + bounds.width - 16, width = right - left;
        std::vector<control> controls;
        int labelId = label_first;
        const auto label = [&](const ui::rect& at, const std::string& text) { controls.push_back({labelId++, at, text, {}, control_kind::label, false}); };
        const auto button = [&](int id, const ui::rect& at, const std::string& text, bool enabled = true, bool danger = false) {
            controls.push_back({id, at, text, {}, control_kind::button, enabled, false, danger});
        };
        const auto field = [&](int id, float y, const std::string& caption, const std::string& value) {
            label({left, y, 112, 28}, caption);
            controls.push_back({id, {left + 116, y, width - 116, 28}, value, {}, control_kind::field});
        };
        const auto choice = [&](int id, float y, const std::string& caption, const std::string& value, bool enabled = true) {
            label({left, y, 112, 28}, caption);
            controls.push_back({id, {left + 116, y, width - 116, 28}, value, {}, control_kind::choice, enabled});
        };
        label({layout.type.x, layout.type.y - 20, layout.type.width, 18}, "Action Type");
        controls.push_back({command_kind, layout.type, ActionKindName(this->draft_.kind), {}, control_kind::dropdown});
        const float toolsY = bounds.y + (bounds.height < 500 ? 102 : 116);
        button(command_editor_options, {left, toolsY, 136, 24}, this->editor_options_ ? "Back to macro" : "More options");
        button(command_icon_picker, {layout.icon.x - 10, toolsY, layout.icon.width + 20, 24}, "Change");
        if (this->editor_options_)
        {
            float y = bounds.y + 154;
            field(field_name, y, "Library name", this->draft_.name); y += 36;
            choice(command_library, y, "Save to", this->library_draft_ || !this->draft_.macro_id.empty() ? "Shared library" : "This slot", !this->library_edit_); y += 36;
            choice(command_available, y, "Availability", this->draft_.always_available ? "Always available" : "Follow action"); y += 36;
            if (this->selected_.surface == action_surface::hotbar && !this->library_edit_)
            {
                button(command_binding, {left, y, width - 100, 28}, this->binding_capture_ ? "Press a key..." : "Binding: " + this->BindingLabel(this->Binding(this->selected_)));
                button(command_unbind, {right - 92, y, 92, 28}, "Unbind", this->Binding(this->selected_).key != 0); y += 36;
            }
            if (this->store_ && this->store_->PetAware(this->selected_.surface, this->selected_.bar) && !this->store_->Context().pet.empty())
            { button(command_pet, {left, y, width, 28}, this->draft_pet_ ? "Current pet" : "All pets"); y += 36; }
            button(command_clear, {left, y, 100, 28}, "Clear slot", !this->library_edit_, true);
            if (!this->draft_.macro_id.empty())
            {
                button(command_macro_copy, {left + 108, y, 124, 28}, "Make a copy");
                button(command_macro_delete, {left + 240, y, width - 240, 28}, "Delete shared...", true, true);
            }
        }
        else if (this->draft_.kind == action_kind::macro || this->draft_.kind == action_kind::command)
        {
            label({left, layout.commands.y - 24, width - 160, 20}, this->draft_.kind == action_kind::macro ? "Macro Commands" : "Command");
            if (this->draft_.kind == action_kind::macro)
            {
                const float y = layout.commands.y - 28;
                button(command_previous, {right - 152, y, 28, 24}, "<", this->line_scroll_ > 0);
                controls.back().hint = "Previous command lines";
                button(command_next, {right - 120, y, 28, 24}, ">", this->line_scroll_ + layout.rows < this->draft_.lines.size());
                controls.back().hint = "Next command lines";
                button(command_more_lines, {right - 88, y, 40, 24}, "+", this->draft_.lines.size() < 32);
                controls.back().hint = "Add a command line";
                button(command_remove_line, {right - 44, y, 44, 24}, "-", this->draft_.lines.size() > 1);
                controls.back().hint = "Remove the last command line";
            }
            for (size_t row = 0; row < layout.rows && row + this->line_scroll_ < 32; ++row)
            {
                const auto line = row + this->line_scroll_;
                controls.push_back({line_first + static_cast<int>(line), {left + 26, layout.commands.y + row * layout.pitch, width - 26, layout.pitch - 1},
                    line < this->draft_.lines.size() ? this->draft_.lines[line] : "", std::to_string(line + 1), control_kind::field});
            }
            button(command_editor_recast, layout.recast, this->editor_recast_ ? "-  Recast Source (Optional)" : "+  Recast Source (Optional)");
            if (this->editor_recast_)
            {
                choice(command_recast, layout.recast.y + 32, "Source Type", this->draft_.recast_disabled ? "None" : this->draft_.recast_kind == action_kind::none ? "Automatic" : ActionKindName(this->draft_.recast_kind));
                controls.back().kind = control_kind::dropdown;
                if (this->draft_.recast_kind != action_kind::none)
                    field(field_recast, layout.recast.y + 66, "Action", this->draft_.recast_name.empty() && this->draft_.recast_id ? std::to_string(this->draft_.recast_id) : this->draft_.recast_name);
            }
            label({left, layout.label.y - 20, width, 18}, "Slot Label");
            controls.push_back({field_label, layout.label, this->draft_.label, {}, control_kind::field});
        }
        else
        {
            float y = bounds.y + 154;
            if (this->draft_.kind == action_kind::native_macro)
            {
                label({left, y, width, 18}, "Uses an existing macro from FFXI's macro books."); y += 30;
                choice(command_native_book, y, "Book", std::to_string(this->draft_.native.book)); y += 36;
                choice(command_native_set, y, "Set", std::to_string(this->draft_.native.set)); y += 36;
                choice(command_native_slot, y, "Slot", std::to_string(this->draft_.native.slot)); y += 36;
                choice(command_native_alt, y, "Row", this->draft_.native.alt ? "Alt" : "Ctrl"); y += 36;
            }
            else
            {
                label({left, y, width, 18}, "Action"); y += 20;
                controls.push_back({field_name, {left, y, width - 108, 28}, this->draft_.name, {}, control_kind::field});
                button(command_picker, {right - 100, y, 100, 28}, "Choose...", !this->library_edit_); y += 40;
                if (this->draft_.kind == action_kind::equip) choice(command_equipment, y, "Equipment slot", this->draft_.equipment_slot.empty() ? "main" : this->draft_.equipment_slot);
                else { choice(command_target, y, "Target", this->draft_.target); controls.back().kind = control_kind::dropdown; }
                y += 40;
            }
            label({left, y, width, 18}, "Slot Label"); y += 20;
            controls.push_back({field_label, {left, y, width, 28}, this->draft_.label, {}, control_kind::field});
        }
        button(command_save, {left, layout.footer.y, 90, 28}, "Save");
        button(command_cancel, {left + 98, layout.footer.y, 90, 28}, "Cancel");
        return controls;
    }

    auto actionbars_module::RevealEditorLine(const size_t line) -> void
    {
        const auto rows = this->EditorLayout().rows;
        if (line < this->line_scroll_) this->line_scroll_ = line;
        else if (line >= this->line_scroll_ + rows) this->line_scroll_ = line + 1 - rows;
        this->focused_control_ = line_first + static_cast<int>(line);
    }

    auto actionbars_module::PasteEditorLines(const std::string_view text) -> bool
    {
        if (this->draft_.kind != action_kind::macro || this->editing_field_ < line_first || this->editing_field_ >= line_first + 32 || text.find_first_of("\r\n") == std::string_view::npos) return false;
        const auto line = static_cast<size_t>(this->editing_field_ - line_first);
        const auto& editor = this->input_.Editor(); const auto [begin, end] = editor.Selection();
        std::string pasted = editor.Text().substr(0, begin) + std::string{text} + editor.Text().substr(end);
        std::vector<std::string> lines(1);
        for (size_t index = 0; index < pasted.size(); ++index)
        {
            const unsigned char c = pasted[index];
            if (c == '\r' || c == '\n') { if (c == '\r' && index + 1 < pasted.size() && pasted[index + 1] == '\n') ++index; lines.emplace_back(); }
            else if (c < 32 || c == 127) { this->Notice("Macro lines cannot contain control characters."); return true; }
            else lines.back() += static_cast<char>(c);
        }
        const size_t existing = std::max(this->draft_.lines.size(), line + 1);
        if (existing + lines.size() - 1 > 32 || std::any_of(lines.begin(), lines.end(), [](const auto& value) { return value.size() > 240; }))
        { this->Notice("This paste exceeds the macro's line or command-length limit."); return true; }
        auto updated = this->draft_.lines; updated.resize(existing);
        updated[line] = std::move(lines.front());
        updated.insert(updated.begin() + line + 1, lines.begin() + 1, lines.end());
        this->draft_.lines = std::move(updated); this->input_.Cancel(); this->editing_field_ = 0;
        this->RevealEditorLine(line + lines.size() - 1); this->BeginField(this->focused_control_);
        return true;
    }
}
