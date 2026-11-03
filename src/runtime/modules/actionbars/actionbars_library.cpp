#include "actionbars_module.hpp"
#include "ui/game_icon_cache.hpp"
#include "config/config_controls.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace xiui
{
    auto actionbars_module::ReplacesNativeMacroKey(const WPARAM key) const -> bool
    {
        return this->allow_native_keys_ && !this->InputFocused() && key >= '0' && key <= '9' &&
            ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000));
    }

    auto actionbars_module::ObserveNativeMacroKey(const WPARAM key, const LPARAM flags, const bool claimed) -> void
    {
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        if (released)
        {
            if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL || key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
                this->native_macro_modifiers_ = 0;
        }
        else if (claimed)
            this->native_macro_modifiers_ = ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) | ((GetKeyState(VK_MENU) & 0x8000) ? 2 : 0);
        else if (!repeated && key >= '0' && key <= '9') this->native_macro_modifiers_ = 0;
    }

    auto actionbars_module::OpenLibrary(const bool crossbar, const size_t bar, const size_t slot) -> void
    {
        this->CommitField(); this->binding_capture_ = false;
        this->selected_ = {crossbar ? action_surface::crossbar : action_surface::hotbar,
            std::min(bar, crossbar ? action_crossbar_banks - 1 : action_bar_count - 1),
            std::min(slot, crossbar ? action_crossbar_slots - 1 : action_bar_slots - 1)};
        this->view_ = crossbar ? action_bar_count : this->selected_.bar;
        if (crossbar) this->selected_bank_ = static_cast<int>(this->selected_.bar);
        this->draft_ = {};
        this->draft_pet_ = this->store_ && this->store_->PetAware(this->selected_.surface, this->selected_.bar) && !this->store_->Context().pet.empty();
        this->message_.clear(); this->SyncStorage(); this->OpenPicker();
        if (this->sounds_) this->sounds_->MenuOpened(this->now_);
    }

    auto actionbars_module::OpenSlotMenu(const address& value) -> void
    {
        this->RememberLibrary();
        this->CommitField(); this->binding_capture_ = false; this->selected_ = value;
        this->view_ = value.surface == action_surface::crossbar ? action_bar_count : value.bar;
        if (value.surface == action_surface::crossbar) this->selected_bank_ = static_cast<int>(value.bar);
        this->draft_pet_ = this->store_ && this->store_->PetAware(value.surface, value.bar) && !this->store_->Context().pet.empty();
        this->dialog_ = dialog::slot_menu; this->focused_control_ = 502; this->message_.clear();
    }

    auto actionbars_module::OpenBarMenu(const address& value) -> void
    {
        this->RememberLibrary();
        this->CommitField(); this->binding_capture_ = false; this->selected_ = value;
        this->view_ = value.surface == action_surface::crossbar ? action_bar_count : value.bar;
        if (value.surface == action_surface::crossbar) this->selected_bank_ = static_cast<int>(value.bar);
        this->dialog_ = dialog::bar_menu; this->focused_control_ = 550; this->message_.clear(); this->SyncStorage();
    }

    auto actionbars_module::PrepareAction(action_slot& value) const -> void
    {
        if (value.target.empty()) value.target = "<t>";
        if (value.kind != action_kind::equip || !value.equipment_slot.empty()) return;
        static constexpr const char* names[]{"main", "sub", "range", "ammo", "head", "body", "hands", "legs", "feet", "neck", "waist", "ear1", "ear2", "ring1", "ring2", "back"};
        auto* resources = this->core_ ? this->core_->GetResourceManager() : nullptr;
        const auto* item = resources ? resources->GetItemById(value.id) : nullptr;
        if (item)
            for (size_t index = 0; index < std::size(names); ++index)
                if ((item->Slots & (1u << index)) != 0) { value.equipment_slot = names[index]; return; }
        value.equipment_slot = "main";
    }

    auto actionbars_module::AssignAction(const address& destination, action_slot value) -> bool
    {
        this->PrepareAction(value);
        const auto error = ValidateAction(value);
        if (!error.empty()) { this->Notice(error); return false; }
        const bool pet = this->store_ && this->store_->PetAware(destination.surface, destination.bar) && !this->store_->Context().pet.empty();
        if (!this->store_ || !this->store_->Assign(destination.surface, destination.bar, destination.slot, value, pet))
        { this->Notice("The action could not be assigned here."); return false; }
        this->Presentation(destination).state.enabled = true;
        this->settings_changed_ = true; this->inspected_.clear();
        return true;
    }

    auto actionbars_module::UpdateLibraryMetrics(const ui::design_system& design) -> void
    {
        ui::config_controls controls{design};
        if (this->config_text_) controls.SetTextSettings(*this->config_text_);
        const auto style = [&](const float scale, const bool button = false) {
            auto value = design.GetTheme().body_text; value.scale = scale;
            return controls.Style(value, button);
        };
        auto& metrics = this->library_metrics_;
        metrics.name_height = std::ceil(design.TextHeight(style(0.76f)));
        metrics.caption_height = std::ceil(design.TextHeight(style(0.64f)));
        metrics.nav_height = std::ceil(design.TextHeight(style(0.72f, true)));
        metrics.button_height = std::ceil(design.TextHeight(style(0.76f, true)));
        metrics.sidebar_width = 128;
        for (const auto* label : {"All actions", "Spells", "Abilities", "Weapon skills", "Items", "Equipment", "Pet commands", "Macros", "Commands"})
            metrics.sidebar_width = std::max(metrics.sidebar_width, std::ceil(design.MeasureText(i18n::Text(label), style(0.72f, true))) + 24);
        metrics.filter_width = std::ceil(std::max(design.MeasureText(i18n::Text("Your actions"), style(0.76f, true)), design.MeasureText(i18n::Text("All actions"), style(0.76f, true)))) + 24;
        metrics.search_with_shortcut_width = std::ceil(design.MeasureText(i18n::Text("Search actions and macros..."), style(0.76f)) + design.MeasureText("Ctrl+F", style(0.64f))) +
            design.GetTheme().spacing.sm * 2 + design.GetTheme().spacing.md + 12;
        metrics.create_width = 96;
        for (const auto* label : {"New macro", "Single command", "Game macro"})
            metrics.create_width = std::max(metrics.create_width, std::ceil(design.MeasureText(i18n::Text(label), style(0.76f, true))) + 24);
        if (this->dialog_ == dialog::picker) this->SetScroll(static_cast<int>(this->result_scroll_));
    }

    auto actionbars_module::LibraryLayout(void) const -> library_layout
    {
        library_layout layout{}; layout.bounds = this->DialogBounds();
        const auto& bounds = layout.bounds; const auto& metrics = this->library_metrics_;
        const float left = bounds.x + 12, right = bounds.x + bounds.width - 12, top = bounds.y + 42;
        const float gap = 8, controlHeight = std::max(28.0f, metrics.button_height + 10);
        const float sidebarWidth = std::min(metrics.sidebar_width, std::max(80.0f, (right - left) * 0.48f));
        const float contentLeft = left + sidebarWidth + 12, contentWidth = std::max(1.0f, right - contentLeft);
        const size_t columns = static_cast<size_t>(std::clamp(static_cast<int>((contentWidth + gap) / (metrics.create_width + gap)), 1, 3));
        const size_t footerRows = (3 + columns - 1) / columns;
        const float footerHeight = static_cast<float>(footerRows) * (controlHeight + gap) - gap;
        const float footerY = bounds.y + bounds.height - 12 - footerHeight;
        layout.footer = {left, footerY, right - left, footerHeight};
        layout.done = {left, footerY, sidebarWidth, controlHeight};
        const float createWidth = std::max(1.0f, (contentWidth - static_cast<float>(columns - 1) * gap) / static_cast<float>(columns));
        for (size_t index = 0; index < layout.create.size(); ++index)
            layout.create[index] = {contentLeft + static_cast<float>(index % columns) * (createWidth + gap), footerY + static_cast<float>(index / columns) * (controlHeight + gap), createWidth, controlHeight};
        const float searchMinimum = std::min(120.0f, contentWidth / 3);
        const float filterWidth = std::min(metrics.filter_width, std::max(1.0f, (contentWidth - 2 * gap - searchMinimum) / 2));
        layout.your_actions = {contentLeft, top, filterWidth, controlHeight};
        layout.all_actions = {contentLeft + filterWidth + gap, top, filterWidth, controlHeight};
        const float searchLeft = contentLeft + 2 * (filterWidth + gap);
        layout.search = {searchLeft, top, std::max(1.0f, right - searchLeft), controlHeight};
        layout.nav_pitch = std::max(28.0f, metrics.nav_height + 10);
        layout.sidebar = {left, top, sidebarWidth, std::max(0.0f, footerY - top - gap)};
        layout.name_height = metrics.name_height; layout.caption_height = this->category_ == 0 ? metrics.caption_height : 0;
        layout.row_height = std::max(40.0f, layout.name_height + layout.caption_height + (layout.caption_height > 0 ? 2 : 0) + 10);
        layout.row_pitch = layout.row_height + 2;
        const float listTop = top + controlHeight + gap;
        const float listHeight = std::max(0.0f, footerY - gap - listTop);
        layout.rows = static_cast<size_t>(std::clamp(static_cast<int>((listHeight + 2) / layout.row_pitch), 1, 64));
        layout.list = {contentLeft, listTop, std::max(1.0f, contentWidth - 14), std::min(listHeight, static_cast<float>(layout.rows) * layout.row_pitch - 2)};
        layout.column_width = std::max(1.0f, (layout.list.width - gap) / static_cast<float>(layout.columns));
        layout.column_pitch = layout.column_width + gap;
        layout.scrollbar = {right - 10, listTop, 10, layout.list.height};
        return layout;
    }

    auto actionbars_module::PickerRows(void) const -> size_t
    { return this->LibraryLayout().rows; }

    auto actionbars_module::LibraryStatus(void) const -> std::string
    {
        if (this->dialog_ != dialog::picker) return {};
        static constexpr const char* names[]{"All actions", "Spells", "Abilities", "Weapon skills", "Items", "Equipment", "Pet commands", "Macros", "Commands"};
        return std::string{names[std::clamp(this->category_, 0, 8)]} + ": " + std::to_string(this->results_.size()) + " shown / " +
            (this->only_learned_ ? "Your actions" : "All actions") + " / " + (this->window_focused_ ? "focused" : "unfocused");
    }

    auto actionbars_module::DrawLibraryIcon(const action_slot& value, const ui::rect& bounds, const module_render_context& context) const -> void
    {
        const auto& design = context.Design();
        design.FillRoundedRect(bounds, 0xE01B202B, 3);
        auto* icons = context.Icons(); ui::game_icon_view icon;
        if (icons)
        {
            auto key = this->executor_ != nullptr ? this->executor_->SuggestedIcon(this->core_, value) : value.icon;
            if (key.empty() && value.id)
            {
                if (value.kind == action_kind::spell) key = "spell:" + std::to_string(value.id);
                else if (value.kind == action_kind::ability || value.kind == action_kind::pet) key = "ability:" + std::to_string(value.id);
                else if (value.kind == action_kind::weapon_skill) key = "weapon:" + std::to_string(value.id);
            }
            if (!key.empty()) icon = icons->ActionIcon(key);
            if (!icon.texture && (value.kind == action_kind::item || value.kind == action_kind::equip)) icon.texture = icons->Item(value.id);
        }
        if (icon.texture) design.DrawTextureRoundedRect(icon.texture, bounds, icon.uv, 3, 0xFFFFFFFF);
        else
        {
            auto profile = this->text_[0]; profile.style.alignment = ui::text_alignment::center;
            context.DrawText(value.kind == action_kind::macro || value.kind == action_kind::native_macro ? "M" : value.kind == action_kind::command ? "/" : "?", bounds, bounds, profile);
        }
        design.BorderRect(bounds, 0x807C818A);
    }

    auto actionbars_module::CycleShape(const size_t index) -> void
    {
        if (index >= action_bar_count) return;
        static constexpr float columns[]{12, 6, 4, 3, 2, 1};
        auto& bar = this->bars_[index];
        const auto current = std::find(std::begin(columns), std::end(columns), bar.columns);
        const auto next = current == std::end(columns) ? 0 : (current - std::begin(columns) + 1) % std::size(columns);
        bar.columns = columns[next]; bar.slots = 12;
        this->Layout(index); this->SyncStorage(); this->settings_changed_ = true;
    }

    auto actionbars_module::MoveAssignment(const controller_action action) -> void
    {
        const bool crossbar = this->selected_.surface == action_surface::crossbar;
        if (action == controller_action::next_tab || action == controller_action::previous_tab)
        {
            const int step = action == controller_action::next_tab ? 1 : -1;
            if (crossbar)
            {
                this->selected_.bar = static_cast<size_t>((static_cast<int>(this->selected_.bar) + step + static_cast<int>(action_crossbar_banks)) % static_cast<int>(action_crossbar_banks));
                this->selected_bank_ = static_cast<int>(this->selected_.bar);
            }
            else for (int offset = 1; offset <= static_cast<int>(action_bar_count); ++offset)
            {
                const auto next = static_cast<size_t>((static_cast<int>(this->selected_.bar) + offset * step + static_cast<int>(action_bar_count)) % static_cast<int>(action_bar_count));
                if (this->BarVisible(next)) { this->selected_.bar = next; this->selected_.slot = std::min(this->selected_.slot, static_cast<size_t>(this->bars_[next].slots) - 1); break; }
            }
            return;
        }
        const int dx = action == controller_action::left ? -1 : action == controller_action::right ? 1 : 0;
        const int dy = action == controller_action::up ? -1 : action == controller_action::down ? 1 : 0;
        if (!dx && !dy) return;
        const auto origin = this->SlotBounds(this->selected_);
        float score = std::numeric_limits<float>::max(); address best = this->selected_;
        const auto consider = [&](const address& candidate) {
            if (candidate == this->selected_) return;
            const auto bounds = this->SlotBounds(candidate);
            const float x = bounds.x - origin.x, y = bounds.y - origin.y;
            if (dx && x * dx <= 1 || dy && y * dy <= 1) return;
            const float distance = dx ? std::abs(x) + std::abs(y) * 2 : std::abs(y) + std::abs(x) * 2;
            if (distance < score) { score = distance; best = candidate; }
        };
        if (crossbar)
            for (const auto bank : this->CrossbarGroups())
                for (size_t slot = 0; slot < action_crossbar_slots; ++slot) consider({action_surface::crossbar, bank, slot});
        else
            for (size_t bar = 0; bar < action_bar_count; ++bar)
                if (this->BarVisible(bar)) for (size_t slot = 0; slot < static_cast<size_t>(this->bars_[bar].slots); ++slot) consider({action_surface::hotbar, bar, slot});
        this->selected_ = best;
        if (crossbar) this->selected_bank_ = static_cast<int>(best.bar);
    }
}
