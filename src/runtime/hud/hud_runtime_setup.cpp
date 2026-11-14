#include "hud/hud_runtime.hpp"
#include "ui/text_wrap.hpp"

#include <algorithm>
#include <limits>

namespace xiui
{
    auto hud_runtime::LoadSetup(const settings_store& store, const bool fresh) -> void
    {
        this->setup_.pending = store.ReadBool("session.setup", "pending", fresh);
        this->setup_.step = static_cast<size_t>(std::clamp(store.ReadInt("session.setup", "step", 0), 0, static_cast<int>(ui::setup_pages.size() - 1)));
        const auto visited = (1u << std::clamp(static_cast<int>(this->setup_.step) - 2, 0, 6)) - 1;
        this->setup_.revealed = static_cast<uint32_t>(store.ReadInt("session.setup", "revealed", static_cast<int>(visited))) & 0xFFFu;
        this->setup_.open = false; this->setup_.pressed = ui::setup_control::none; this->setup_.focus = ui::setup_control::next;
    }

    auto hud_runtime::SetupVisible(void) const -> bool
    {
        return this->setup_.open && this->visible_ && !this->ControllerNativeBusy();
    }

    auto hud_runtime::BeginSetup(void) -> void
    {
        this->FinishScale(false); this->FinishControllerMove(false); this->FinishConfigResize(false);
        this->FinishConfigInput(false); this->CloseChoiceInput(); this->CloseSettingsSearch(); this->CloseProfileControls();
        this->ReleaseControllerFocus(); this->ReleaseChatInput(); this->SuspendSatchelInput(); this->SuspendActionBarsInput();
        this->SetEditing(false);
        this->SetVisible(true);
        this->setup_.pending = true; this->setup_.open = false; this->setup_.step = 0;
        this->setup_.revealed = 0;
        this->setup_.focus = ui::setup_control::next; this->setup_.pressed = ui::setup_control::none;
        ++this->controller_input_revision_;
        this->SaveSettings();
        this->UpdateSetup();
    }

    auto hud_runtime::UpdateSetup(void) -> void
    {
        if (this->setup_.open && !this->SetupVisible()) this->setup_.pressed = ui::setup_control::none;
        if (!this->setup_.pending || this->setup_.open || !this->visible_ || this->ControllerNativeBusy()) return;
        this->SetEditing(false);
        this->ReleaseControllerFocus(); this->ReleaseChatInput(); this->SuspendSatchelInput(); this->SuspendActionBarsInput();
        this->setup_.open = true;
        ++this->controller_input_revision_;
        this->ui_sounds_.MenuOpened(::GetTickCount64());
    }

    auto hud_runtime::SetupModule(void) -> hud_module*
    {
        const std::array<hud_module*, 6> modules{&this->player_, &this->chat_, &this->target_, &this->party_, &this->minimap_, &this->enemy_list_};
        return this->setup_.step >= 2 && this->setup_.step < 8 ? modules[this->setup_.step - 2] : nullptr;
    }

    auto hud_runtime::SetupModules(void) const -> std::array<const hud_module*, 12>
    {
        return {&this->player_, &this->chat_, &this->target_, &this->party_, &this->minimap_, &this->enemy_list_,
            &this->cast_bar_, &this->recast_, &this->buffs_, &this->pet_, &this->experience_, &this->actionbars_};
    }

    auto hud_runtime::SetupModuleVisible(const hud_module& module) const -> bool
    {
        const hud_module* owner = &module;
        if (owner == &this->chat_second_) owner = &this->chat_;
        if (owner == &this->target_cast_ || owner == &this->target_recipient_ || owner == &this->subtarget_ || owner == &this->world_target_scale_)
            owner = &this->target_;
        if (owner == &this->alliance_first_ || owner == &this->alliance_second_) owner = &this->party_;
        if (owner == &this->debuffs_) owner = &this->buffs_;
        if (owner == &this->pet_target_) owner = &this->pet_;
        for (const auto& child : this->actionbar_children_)
            if (owner == &child) { owner = &this->actionbars_; break; }
        if (!owner->State().enabled) return false;
        const auto modules = this->SetupModules();
        for (size_t i = 0; i < modules.size(); ++i)
            if (modules[i] == owner)
                return (this->setup_.revealed & (1u << i)) != 0 || (this->setup_.step >= 2 && this->setup_.step < 8 && i == this->setup_.step - 2);
        return false;
    }

    auto hud_runtime::SetupNativeHudMask(void) const -> uint32_t
    {
        auto mask = (1u << native_hud_options.size()) - 1;
        const auto modules = this->SetupModules();
        const uint32_t current = this->setup_.step >= 2 && this->setup_.step < 8 ? 1u << (this->setup_.step - 2) : 0;
        const uint32_t revealed = this->setup_.revealed | current;
        const std::array<std::pair<size_t, uint32_t>, 6> native{{
            {0, NativeHudBit(native_hud_element::player) | NativeHudBit(native_hud_element::party)},
            {1, NativeHudBit(native_hud_element::chat1) | NativeHudBit(native_hud_element::chat2) | NativeHudBit(native_hud_element::chatlog)},
            {2, NativeHudBit(native_hud_element::target)}, {3, NativeHudBit(native_hud_element::party)},
            {6, NativeHudBit(native_hud_element::castbar)}, {8, NativeHudBit(native_hud_element::status)}}};
        for (const auto& [index, bits] : native)
            if ((revealed & (1u << index)) != 0 && !modules[index]->State().enabled) mask &= ~bits;
        return mask;
    }

    auto hud_runtime::SetupBounds(const D3DVIEWPORT8& viewport) -> rect
    {
        const float width = std::min(500.0f, std::max(1.0f, viewport.Width - 24.0f));
        const float height = std::min(this->setup_.step == 3 ? 444.0f : 400.0f, std::max(1.0f, viewport.Height - 24.0f));
        rect bounds{(viewport.Width - width) * .5f, (viewport.Height - height) * .5f, width, height};
        if (auto* module = this->SetupModule())
        {
            const auto highlight = this->MovementBounds(*module);
            constexpr float gap = 16;
            const float centerX = highlight.x + (highlight.width - width) * .5f;
            const float centerY = highlight.y + (highlight.height - height) * .5f;
            const std::array candidates{
                rect{highlight.x + highlight.width + gap, centerY, width, height},
                rect{highlight.x - width - gap, centerY, width, height},
                rect{centerX, highlight.y + highlight.height + gap, width, height},
                rect{centerX, highlight.y - height - gap, width, height}};
            float best = std::numeric_limits<float>::max();
            for (auto candidate : candidates)
            {
                candidate.x = std::clamp(candidate.x, 12.0f, std::max(12.0f, viewport.Width - width - 12));
                candidate.y = std::clamp(candidate.y, 12.0f, std::max(12.0f, viewport.Height - height - 12));
                const float overlap = std::max(0.0f, std::min(candidate.x + width, highlight.x + highlight.width) - std::max(candidate.x, highlight.x)) *
                    std::max(0.0f, std::min(candidate.y + height, highlight.y + highlight.height) - std::max(candidate.y, highlight.y));
                if (overlap < best) { bounds = candidate; best = overlap; }
                if (overlap == 0) break;
            }
        }
        return bounds;
    }

    auto hud_runtime::SetupItems(const D3DVIEWPORT8& viewport) -> std::vector<ui::setup_item>
    {
        using control = ui::setup_control;
        const auto panel = this->SetupBounds(viewport);
        const float x = panel.x + 20, width = panel.width - 40, bottom = panel.y + panel.height - 56;
        std::vector<ui::setup_item> items;
        if (auto* module = this->SetupModule())
        {
            const bool replacement = this->setup_.step <= 5;
            items.push_back({control::enable, {x, panel.y + 200, (width - 12) * .5f, 34}, replacement ? "Use XIUI+" : "On", module->State().enabled});
            items.push_back({control::disable, {x + (width + 12) * .5f, panel.y + 200, (width - 12) * .5f, 34}, replacement ? "Use game HUD" : "Off", !module->State().enabled});
        }
        if (this->setup_.step == 3 && this->chat_.State().enabled)
        {
            const auto layout = this->chat_.WindowLayout();
            items.push_back({control::chat_native, {x, panel.y + 248, (width - 12) * .5f, 34}, "Follow game windows", layout == chat_window_layout::follow_game});
            items.push_back({control::chat_combined, {x + (width + 12) * .5f, panel.y + 248, (width - 12) * .5f, 34}, "One combined window", layout == chat_window_layout::combined});
        }
        if (this->setup_.step == 8)
        {
            const std::array<hud_module*, 6> modules{&this->cast_bar_, &this->recast_, &this->buffs_, &this->pet_, &this->experience_, &this->actionbars_};
            static constexpr const char* names[]{"Cast Bar", "Recast Timers", "Buffs / Debuffs", "Pet Bar", "Experience", "Hotbars"};
            for (size_t i = 0; i < modules.size(); ++i)
                items.push_back({static_cast<control>(static_cast<int>(control::castbar) + i),
                    {x + (i % 2) * (width + 12) * .5f, panel.y + 150 + (i / 2) * 40, (width - 12) * .5f, 32}, names[i],
                    (this->setup_.revealed & (1u << (i + 6))) != 0 && modules[i]->State().enabled});
            items.push_back({control::settings, {x + 94, bottom, width - 200, 32}, "Open settings"});
        }
        if (this->setup_.step > 0) items.push_back({control::back, {x, bottom, 84, 32}, "Back"});
        const float startWidth = std::min(200.0f, width);
        const auto next = this->setup_.step == 0 ? rect{x + (width - startWidth) * .5f, bottom - 16, startWidth, 44} : rect{x + width - 96, bottom, 96, 32};
        items.push_back({control::next, next, this->setup_.step == 8 ? "Finish" : this->setup_.step == 0 ? "Get started" : "Next"});
        if (this->setup_.step != 8) items.push_back({control::skip, {x + width - 100, panel.y + 14, 100, 26}, "Skip setup"});
        return items;
    }

    auto hud_runtime::ApplySetupNativeVisibility(const hud_module& module) -> void
    {
        const auto shown = [&](const native_hud_element element, const bool visible) {
            this->game_hud_.SetWindowShown(static_cast<size_t>(element), visible);
        };
        const bool enabled = module.State().enabled;
        if (&module == &this->party_ && enabled) this->general_.SetPartyEnabled(true);
        if (&module == &this->player_) shown(native_hud_element::player, !enabled);
        if (&module == &this->target_) shown(native_hud_element::target, !enabled);
        if (&module == &this->player_ || &module == &this->party_)
            shown(native_hud_element::party, !this->player_.State().enabled || !this->party_.State().enabled || !this->general_.PartyEnabled());
        if (&module == &this->chat_)
            for (const auto element : {native_hud_element::chat1, native_hud_element::chat2, native_hud_element::chatlog}) shown(element, !enabled);
        if (&module == &this->cast_bar_) shown(native_hud_element::castbar, !enabled);
        if (&module == &this->buffs_) shown(native_hud_element::status, !enabled);
    }

    auto hud_runtime::ActivateSetup(const ui::setup_control control) -> void
    {
        using item = ui::setup_control;
        this->setup_.pressed = item::none;
        if (control == item::skip || control == item::settings || (control == item::next && this->setup_.step == 8))
        { this->FinishSetup(control == item::settings); return; }
        if (control == item::next || control == item::back)
        {
            if (control == item::back && this->setup_.step == 0) return;
            if (control == item::next && this->setup_.step == 3 && this->chat_.State().enabled)
                if (const auto layout = this->chat_.WindowLayout()) this->chat_.SetWindowLayout(*layout, this->window_config_.ChatWindowCount());
            if (control == item::next && this->setup_.step >= 2 && this->setup_.step < 8)
            {
                this->setup_.revealed |= 1u << (this->setup_.step - 2);
                this->ApplySetupNativeVisibility(*this->SetupModule());
            }
            this->setup_.step = control == item::next ? std::min(size_t{8}, this->setup_.step + 1) : this->setup_.step - 1;
            this->setup_.focus = item::next;
        }
        else if (control == item::chat_native || control == item::chat_combined)
        {
            if (this->setup_.step != 3 || !this->chat_.State().enabled) return;
            this->chat_.SetWindowLayout(control == item::chat_native ? chat_window_layout::follow_game : chat_window_layout::combined,
                this->window_config_.ChatWindowCount());
        }
        else if (control == item::enable || control == item::disable)
        {
            auto* module = this->SetupModule();
            if (module == nullptr) return;
            const bool enabled = control == item::enable;
            this->setup_.revealed |= 1u << (this->setup_.step - 2);
            module->State().enabled = enabled;
            if (module == &this->chat_)
            {
                if (enabled)
                    if (const auto layout = this->chat_.WindowLayout()) this->chat_.SetWindowLayout(*layout, this->window_config_.ChatWindowCount());
            }
            this->ApplySetupNativeVisibility(*module);
        }
        else if (control >= item::castbar && control <= item::hotbars)
        {
            const std::array<hud_module*, 6> modules{&this->cast_bar_, &this->recast_, &this->buffs_, &this->pet_, &this->experience_, &this->actionbars_};
            const auto index = static_cast<size_t>(control) - static_cast<size_t>(item::castbar);
            auto* module = modules[index];
            const uint32_t bit = 1u << (index + 6);
            module->State().enabled = (this->setup_.revealed & bit) == 0 || !module->State().enabled;
            this->setup_.revealed |= bit;
            if (control == item::buffs)
            {
                this->debuffs_.State().enabled = module->State().enabled;
            }
            this->ApplySetupNativeVisibility(*module);
        }
        this->ui_sounds_.Select(::GetTickCount64());
        this->SaveSettings();
    }

    auto hud_runtime::FinishSetup(const bool settings) -> void
    {
        const auto modules = this->SetupModules();
        for (size_t index = 0; index < modules.size(); ++index)
            if ((this->setup_.revealed & (1u << index)) != 0) this->ApplySetupNativeVisibility(*modules[index]);
        this->setup_.pending = this->setup_.open = false;
        this->setup_.pressed = ui::setup_control::none;
        ++this->controller_input_revision_;
        if (this->setup_.pointer_buttons == 0) { this->SetChatMouseCapture(false); this->ReleasePointerCapture(); }
        this->ui_sounds_.Select(::GetTickCount64());
        this->SaveSettings();
        if (settings)
        {
            this->SetEditing(true);
            this->SetBasicSettings(false);
            this->SelectConfigModule(&this->general_);
        }
    }

    auto hud_runtime::HandleSetupAction(const controller_action action, const bool repeated) -> bool
    {
        if (!this->SetupVisible()) return false;
        this->setup_.show_focus = true;
        if (action == controller_action::up || action == controller_action::down || action == controller_action::left ||
            action == controller_action::right || action == controller_action::next_tab || action == controller_action::previous_tab)
        {
            const auto items = this->SetupItems(this->hud_viewport_);
            const auto found = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.id == this->setup_.focus; });
            const int index = found == items.end() ? 0 : static_cast<int>(found - items.begin());
            const int direction = action == controller_action::up || action == controller_action::left || action == controller_action::previous_tab ? -1 : 1;
            this->setup_.focus = items[(index + direction + static_cast<int>(items.size())) % items.size()].id;
            this->ui_sounds_.Play(ui::native_ui_sound::hover);
        }
        else if (!repeated && action == controller_action::confirm) this->ActivateSetup(this->setup_.focus);
        else if (!repeated && action == controller_action::cancel)
            this->ActivateSetup(this->setup_.step == 0 ? ui::setup_control::skip : ui::setup_control::back);
        return true;
    }

    auto hud_runtime::HandleSetupKey(const WPARAM key, const LPARAM flags, const bool blocked) -> bool
    {
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (key < this->setup_.key_releases.size() && this->setup_.key_releases[key])
        {
            if (released) { this->setup_.key_releases.reset(key); return true; }
            if (!this->SetupVisible()) return true;
        }
        if (blocked || !this->SetupVisible() || !this->GameWindowFocused()) return false;
        if (released) return true;
        const auto action = key == VK_UP ? controller_action::up : key == VK_DOWN ? controller_action::down :
            key == VK_LEFT ? controller_action::left : key == VK_RIGHT ? controller_action::right :
            key == VK_RETURN || key == VK_SPACE ? controller_action::confirm : key == VK_ESCAPE ? controller_action::cancel :
            key == VK_TAB ? ((::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? controller_action::previous_tab : controller_action::next_tab) : controller_action::none;
        if (action != controller_action::none)
        {
            if (key < this->setup_.key_releases.size()) this->setup_.key_releases.set(key);
            this->controller_presented_ = false;
            this->HandleSetupAction(action, (static_cast<uint32_t>(flags) & 0x40000000u) != 0);
        }
        return true;
    }

    auto hud_runtime::HandleSetupMouse(const uint32_t message, const WPARAM buttons, const bool blocked, const D3DVIEWPORT8& viewport) -> bool
    {
        const auto button = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_LBUTTONUP ? MK_LBUTTON :
            message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK || message == WM_RBUTTONUP ? MK_RBUTTON :
            message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK || message == WM_MBUTTONUP ? MK_MBUTTON :
            message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK || message == WM_XBUTTONUP ? (HIWORD(buttons) == XBUTTON1 ? MK_XBUTTON1 : MK_XBUTTON2) : 0;
        const bool up = message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP || message == WM_XBUTTONUP;
        const bool owned = button != 0 && (this->setup_.pointer_buttons & button) != 0;
        if (!this->SetupVisible() && !owned) return false;
        if (blocked && !owned) return false;
        auto hit = ui::setup_control::none;
        for (const auto& item : this->SetupItems(viewport))
            if (Contains(item.bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) { hit = item.id; break; }
        if (up && owned)
        {
            this->setup_.pointer_buttons &= ~button;
            if (message == WM_LBUTTONUP)
            {
                const auto pressed = this->setup_.pressed;
                this->setup_.pressed = ui::setup_control::none;
                if (this->SetupVisible() && hit != ui::setup_control::none && pressed == hit) this->ActivateSetup(hit);
            }
            if (this->setup_.pointer_buttons == 0) this->ReleasePointerCapture();
            return true;
        }
        if (!this->SetupVisible()) return owned;
        if (button != 0 && !up)
        {
            this->setup_.pointer_buttons |= button;
            this->setup_.show_focus = false;
            if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) this->setup_.pressed = hit;
            this->SetChatMouseCapture(true);
        }
        return true;
    }

    auto hud_runtime::DrawSetup(const hud_snapshot& snapshot, const D3DVIEWPORT8& viewport) -> void
    {
        if (!this->SetupVisible()) return;
        const auto& theme = this->design_system_.GetTheme();
        this->design_system_.FillRect({0, 0, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, 0xA0000000);
        if (auto* module = this->SetupModule(); module != nullptr && module->State().enabled)
        {
            const module_render_context preview{this->design_system_, &this->container_renderer_, true, &this->game_icons_,
                {0, 0, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, &this->native_assets_, &this->general_.HealthFeedback()};
            const auto draw = [&](hud_module& target) {
                this->design_system_.BeginContentBounds();
                { const auto transform = this->design_system_.Transform(target.State().transform); target.Render(snapshot, preview); }
                const auto content = this->design_system_.EndContentBounds();
                auto& state = target.State();
                state.content_bounds = {content.x - state.bounds.x, content.y - state.bounds.y, content.width, content.height};
                this->design_system_.DrawFocusFrame(this->MovementBounds(target), this->general_.FocusStyle());
            };
            draw(*module);
            if (module == &this->chat_ && this->chat_second_.State().enabled) draw(this->chat_second_);
        }
        const auto bounds = this->SetupBounds(viewport);
        this->config_controls_.Panel(bounds);
        auto caption = theme.caption_text; caption.scale = .75f; caption.alignment = ui::text_alignment::left;
        auto title = theme.accent_text; title.scale = 1.15f; title.alignment = ui::text_alignment::left;
        auto body = theme.body_text; body.scale = .88f; body.alignment = ui::text_alignment::left;
        auto button = theme.body_text; button.scale = .82f; button.alignment = ui::text_alignment::center;
        const auto& page = ui::setup_pages[this->setup_.step];
        const auto progress = i18n::Format("XIUI+ Setup  %zu / %zu", this->setup_.step + 1, ui::setup_pages.size());
        this->config_controls_.Text(progress.c_str(), {bounds.x + 20, bounds.y + 16, bounds.width - 144, 24}, caption);
        this->config_controls_.Text(i18n::Text(page.title), {bounds.x + 20, bounds.y + 52, bounds.width - 40, 32}, title);
        const auto paragraph = [&](const char* text, float y, const ui::text_style& style) {
            const auto measured = this->config_controls_.Style(style);
            const float height = this->design_system_.TextHeight(measured) + 5;
            const auto lines = ui::WrapText(i18n::Text(text), bounds.width - 44,
                [&](const char* value) { return this->design_system_.MeasureText(value, measured); });
            for (const auto& line : lines)
            { this->config_controls_.Text(line.c_str(), {bounds.x + 20, y, bounds.width - 40, height}, style); y += height; }
            return y;
        };
        const float after = paragraph(page.description, bounds.y + 96, body);
        const bool chatLayout = this->setup_.step == 3 && this->chat_.State().enabled;
        paragraph(this->setup_.step == 3 && !chatLayout ? "The game's chat windows stay in use." : page.tip,
            this->setup_.step == 8 ? bounds.y + 276 : chatLayout ? bounds.y + 294 : this->setup_.step >= 2 ? bounds.y + 250 : after + 18, caption);
        if (auto* module = this->SetupModule())
            this->config_controls_.Text(module->State().enabled ? i18n::Text("Preview - enabled in your HUD") : this->setup_.step <= 5 ? i18n::Text("Using game HUD") : i18n::Text("Off"),
                {bounds.x + 20, bounds.y + 174, bounds.width - 40, 22}, caption);
        for (const auto& item : this->SetupItems(viewport))
        {
            const bool hover = this->GameWindowFocused() && Contains(item.bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            const auto label = std::string{i18n::Text(item.label)} + (item.id >= ui::setup_control::castbar && item.id <= ui::setup_control::hotbars ? (item.selected ? std::string{" - "} + i18n::Text("On") : std::string{" - "} + i18n::Text("Off")) : "");
            auto labelStyle = button;
            if (this->setup_.step == 0 && item.id == ui::setup_control::next) labelStyle.scale = 1.0f;
            this->config_controls_.Button(item.bounds, label.c_str(), labelStyle, item.selected || hover);
            if (item.selected) this->design_system_.FillRect({item.bounds.x + 8, item.bounds.y + 8, 3, item.bounds.height - 16}, theme.colors.control_selected);
            if (this->setup_.show_focus && item.id == this->setup_.focus && this->GameWindowFocused())
                this->design_system_.DrawFocusFrame(item.bounds, this->general_.FocusStyle());
        }
        auto hint = caption; hint.scale = .65f; hint.alignment = ui::text_alignment::center;
        this->config_controls_.Text(this->ControllerPresented() ? i18n::Text("Directions: choose   Confirm: select   Cancel: back") : i18n::Text("Tab / arrows: choose   Enter: select   Esc: back"),
            {bounds.x + 20, bounds.y + bounds.height - 22, bounds.width - 40, 18}, hint);
    }
}
