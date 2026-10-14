#include "hud_runtime.hpp"
#include "settings/profile_share.hpp"
#include "config/clipboard.hpp"

#include <algorithm>

namespace
{
    constexpr uint64_t profile_target = 0xF000000000000000ULL;
}

namespace xiui
{
    auto hud_runtime::CloseProfileControls(void) -> void
    {
        this->profile_name_.Cancel();
        this->profile_dropdown_ = false;
        this->profile_actions_ = false;
        this->profile_share_key_.clear();
        this->profile_key_view_.Reset();
        this->profile_import_.reset();
        this->profile_key_copied_ = false;
        this->profile_dialog_ = profile_dialog::create;
        this->profile_error_.clear();
    }

    auto hud_runtime::OpenProfileDialog(const profile_dialog mode) -> bool
    {
        if (!this->profiles_.Ready()) return false;
        if (mode == profile_dialog::delete_profile && this->profiles_.Selected() == 0)
        { this->profile_error_ = "Default cannot be deleted."; return false; }
        this->FinishConfigInput(true, false);
        this->ReleaseChatInput();
        this->CloseProfileControls();
        this->active_color_ = nullptr;
        this->profile_dialog_ = mode;
        this->profile_source_ = this->profiles_.Selected();
        this->profile_keyboard_selection_ = 0;
        std::string name;
        if (mode == profile_dialog::duplicate) name = this->profiles_.UniqueName(this->profiles_.Current().name + " Copy");
        if (mode == profile_dialog::delete_profile) name = this->profiles_.Current().name;
        if (mode == profile_dialog::share)
        {
            this->SaveSettings();
            if (!this->DrainSettings()) { this->profile_error_ = "Could not save this profile. Try again."; return false; }
            name = this->profiles_.Current().name;
            this->profile_share_key_ = EncodeProfileKey(name, this->CaptureSettings(), this->profile_error_);
            if (this->profile_share_key_.empty()) return false;
        }
        this->profile_name_.Open(std::move(name));
        this->ui_sounds_.MenuOpened(::GetTickCount64());
        return true;
    }

    auto hud_runtime::CopyProfileKey(void) -> bool
    {
        const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        this->profile_key_copied_ = ui::CopyClipboardText(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr, this->profile_share_key_);
        this->profile_error_ = this->profile_key_copied_ ? "" : "Could not copy the key. Try again.";
        return this->profile_key_copied_;
    }

    auto hud_runtime::LoadProfileKey(const std::string_view key) -> bool
    {
        this->profile_import_.reset(); this->profile_share_key_.clear(); this->profile_key_view_.Reset();
        auto decoded = DecodeProfileKey(key, this->profile_error_);
        if (!decoded) return false;
        this->profile_share_key_ = std::string{key};
        std::erase_if(this->profile_share_key_, [](const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; });
        this->profile_name_.Open(this->profiles_.UniqueName(decoded->name));
        this->profile_import_ = std::move(decoded->preferences);
        return true;
    }

    auto hud_runtime::PasteProfileKey(void) -> bool
    {
        const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        const auto key = ui::ReadClipboardText(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr, maximum_profile_key);
        if (!key)
        {
            this->profile_import_.reset(); this->profile_share_key_.clear(); this->profile_key_view_.Reset();
            this->profile_error_ = "Copy an XIUI share key, then paste it here.";
            return false;
        }
        return this->LoadProfileKey(*key);
    }

    auto hud_runtime::ProfileSurface(const D3DVIEWPORT8& viewport) const -> bool
    {
        UNREFERENCED_PARAMETER(viewport);
        return this->editing_ && !this->config_collapsed_ && (this->profile_name_.Focused() || this->profile_dropdown_ || this->profile_actions_);
    }

    auto hud_runtime::DrainSettings(void) const -> bool
    {
        const bool settings = this->settings_writer_.Flush();
        const bool profile = this->profile_writer_.Flush();
        return settings && profile;
    }

    auto hud_runtime::ProfileSoundTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        if (!this->editing_ || this->config_collapsed_ || !this->profiles_.Ready()) return 0;
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (this->profile_name_.Focused())
        {
            const bool extended = this->profile_dialog_ == profile_dialog::share || this->profile_dialog_ == profile_dialog::import_profile;
            const ui::profile_dialog_layout dialog{this->config_bounds_, viewport, extended};
            if (this->profile_dialog_ != profile_dialog::share && this->profile_dialog_ != profile_dialog::delete_profile && Contains(dialog.Field(), x, y)) return profile_target | 10;
            if (Contains(dialog.Cancel(), x, y)) return profile_target | 11;
            if (Contains(dialog.Create(), x, y) && (this->profile_dialog_ != profile_dialog::import_profile || this->profile_import_)) return profile_target | 12;
            if (Contains(dialog.Close(), x, y)) return profile_target | 13;
            if (extended && Contains(dialog.Key(), x, y)) return profile_target | 14;
            if (this->profile_dialog_ == profile_dialog::import_profile && Contains(dialog.Paste(), x, y)) return profile_target | 15;
            return 0;
        }
        if (Contains(this->ConfigLayout().ProfileSelector(), x, y)) return profile_target | 1;
        if (Contains(this->ConfigLayout().ProfileAdd(), x, y)) return profile_target | 2;
        if (Contains(this->ConfigLayout().ProfileActionsButton(), x, y)) return profile_target | 3;
        if (this->profile_actions_)
            for (size_t i = 0; i < std::size(ui::profile_actions); ++i)
            {
                if (i + 1 == static_cast<size_t>(profile_dialog::delete_profile) && this->profiles_.Selected() == 0) continue;
                if (Contains(this->ConfigLayout().ProfileActionRow(i), x, y)) return profile_target | (20 + i);
            }
        if (this->profile_dropdown_)
            for (size_t row = 0; row < std::min<size_t>(7, this->profiles_.Entries().size()); ++row)
                if (Contains(this->ConfigLayout().ProfileRow(row, this->profiles_.Entries().size()), x, y))
                    return profile_target | (0x100 + row + this->profile_scroll_);
        return 0;
    }

    auto hud_runtime::ProcessProfileClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto target = this->ProfileSoundTarget(viewport) & ~profile_target;
        if (this->profile_name_.Focused())
        {
            if (target == 10 && this->profile_dialog_ != profile_dialog::share) this->profile_name_.SelectAll();
            else if (target == 11 || target == 13) this->CloseProfileControls();
            else if (target == 12 && this->profile_dialog_ == profile_dialog::share) this->CopyProfileKey();
            else if (target == 12 && this->profile_dialog_ == profile_dialog::delete_profile) this->DeleteProfile();
            else if (target == 12) this->CreateProfile();
            else if (target == 14 && this->profile_dialog_ == profile_dialog::share) this->CopyProfileKey();
            else if (target == 14 || target == 15) this->PasteProfileKey();
            return true;
        }
        if (target == 2)
        {
            this->OpenProfileDialog(profile_dialog::create);
            return true;
        }
        if (target == 3)
        {
            this->FinishConfigInput(true, false); this->ReleaseChatInput(); this->active_color_ = nullptr;
            this->profile_dropdown_ = false; this->profile_actions_ = !this->profile_actions_;
            this->profile_keyboard_selection_ = 0; this->profile_error_.clear();
            return true;
        }
        if (this->profile_actions_)
        {
            if (target >= 20 && target < 20 + std::size(ui::profile_actions)) this->OpenProfileDialog(static_cast<profile_dialog>(target - 19));
            else this->profile_actions_ = false;
            return true;
        }
        if (target == 1)
        {
            this->FinishConfigInput(true, false);
            this->ReleaseChatInput();
            this->active_color_ = nullptr;
            this->profile_dropdown_ = !this->profile_dropdown_;
            this->profile_keyboard_selection_ = this->profiles_.Selected();
            this->profile_scroll_ = std::clamp(static_cast<int>(this->profiles_.Selected()) - 3, 0, std::max(0, static_cast<int>(this->profiles_.Entries().size()) - 7));
            this->profile_error_.clear();
            return true;
        }
        if (this->profile_dropdown_)
        {
            if (target >= 0x100) this->SelectProfile(static_cast<size_t>(target - 0x100));
            else this->profile_dropdown_ = false;
            return true;
        }
        return false;
    }

    auto hud_runtime::HandleProfileKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if (!this->profile_name_.Focused() && !this->profile_dropdown_ && !this->profile_actions_) return false;
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        const bool repeat = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        if (key == VK_ESCAPE) { this->CloseProfileControls(); return true; }
        if (this->profile_actions_)
        {
            const auto count = std::size(ui::profile_actions) - (this->profiles_.Selected() == 0 ? 1 : 0);
            if (key == VK_RETURN && !repeat) this->OpenProfileDialog(static_cast<profile_dialog>(this->profile_keyboard_selection_ + 1));
            else if (key == VK_UP || key == VK_DOWN) this->profile_keyboard_selection_ = (this->profile_keyboard_selection_ + (key == VK_UP ? count - 1 : 1)) % count;
            return true;
        }
        if (this->profile_name_.Focused())
        {
            if (this->profile_dialog_ == profile_dialog::delete_profile)
            {
                if (key == VK_TAB || key == VK_LEFT || key == VK_RIGHT) this->profile_keyboard_selection_ ^= 1;
                else if (key == VK_RETURN && !repeat)
                {
                    this->ui_sounds_.Select(::GetTickCount64());
                    if (this->profile_keyboard_selection_ == 1) this->DeleteProfile();
                    else this->CloseProfileControls();
                }
                return true;
            }
            const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool keyView = this->profile_dialog_ == profile_dialog::share || this->profile_dialog_ == profile_dialog::import_profile;
            if (keyView && (key == VK_PRIOR || key == VK_NEXT || (control && (key == VK_HOME || key == VK_END))))
            {
                if (key == VK_HOME) this->profile_key_view_.Home();
                else if (key == VK_END) this->profile_key_view_.End();
                else this->profile_key_view_.Page(key == VK_PRIOR ? -1 : 1);
                return true;
            }
            if (this->profile_dialog_ == profile_dialog::share)
            {
                if (!repeat && (key == VK_RETURN || (control && key == 'C'))) this->CopyProfileKey();
                return true;
            }
            if (this->profile_dialog_ == profile_dialog::import_profile && control && key == 'V')
            { if (!repeat) this->PasteProfileKey(); return true; }
            if (key == VK_RETURN) { if (!repeat) { this->ui_sounds_.Select(::GetTickCount64()); this->CreateProfile(); } }
            else if (key != VK_TAB)
            {
                const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
                this->profile_name_.HandleKey(key, flags, properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr);
                this->profile_error_.clear();
            }
            return true;
        }
        if (key == VK_RETURN && !repeat)
        { this->ui_sounds_.Select(::GetTickCount64()); this->SelectProfile(this->profile_keyboard_selection_); }
        else if (key == VK_UP || key == VK_DOWN)
        {
            const auto count = this->profiles_.Entries().size();
            this->profile_keyboard_selection_ = (this->profile_keyboard_selection_ + (key == VK_UP ? count - 1 : 1)) % count;
            if (this->profile_keyboard_selection_ < static_cast<size_t>(this->profile_scroll_)) this->profile_scroll_ = static_cast<int>(this->profile_keyboard_selection_);
            if (this->profile_keyboard_selection_ >= static_cast<size_t>(this->profile_scroll_ + 7)) this->profile_scroll_ = static_cast<int>(this->profile_keyboard_selection_) - 6;
            this->ui_sounds_.Play(ui::native_ui_sound::hover);
        }
        return true;
    }

    auto hud_runtime::CreateProfile(void) -> bool
    {
        if (this->profile_dialog_ == profile_dialog::import_profile && !this->profile_import_)
        { this->profile_error_ = "Paste a valid share key first."; return false; }
        this->SaveSettings();
        if (!this->DrainSettings())
        { this->profile_error_ = "Could not save your current settings. Try again."; return false; }
        auto snapshot = this->CaptureSettings();
        if (this->profile_dialog_ == profile_dialog::duplicate)
        {
            if (!this->profiles_.Duplicate(this->profile_source_, this->profile_name_.Editor().Text(), this->profile_error_)) return false;
        }
        else if (this->profile_dialog_ == profile_dialog::import_profile)
        {
            snapshot.OverlayPreferences(*this->profile_import_);
            if (!this->profiles_.Create(this->profile_name_.Editor().Text(), snapshot, this->profile_error_)) return false;
            this->LoadSettings(snapshot);
        }
        else if (!this->profiles_.Create(this->profile_name_.Editor().Text(), snapshot, this->profile_error_)) return false;
        this->CloseProfileControls();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::DeleteProfile(void) -> bool
    {
        if (this->profile_dialog_ != profile_dialog::delete_profile || !this->profile_name_.Focused() ||
            this->profile_source_ == 0 || this->profile_source_ != this->profiles_.Selected()) return false;
        this->SaveSettings();
        if (!this->DrainSettings())
        { this->profile_error_ = "Could not save your settings. The profile was not deleted."; return false; }
        const auto fallback = this->profiles_.Read(0, this->profile_error_);
        if (!fallback) return false;
        auto snapshot = this->CaptureSettings();
        snapshot.OverlayPreferences(*fallback);
        if (!this->profiles_.Remove(this->profile_source_, this->profile_error_)) return false;
        this->LoadSettings(snapshot);
        this->CloseProfileControls();
        this->profile_scroll_ = 0;
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::SelectProfile(const size_t index) -> bool
    {
        if (index >= this->profiles_.Entries().size()) return false;
        if (index == this->profiles_.Selected()) { this->profile_dropdown_ = false; return true; }
        this->FinishConfigInput(true, false);
        this->FinishInteraction();
        this->SaveSettings();
        if (!this->DrainSettings())
        { this->profile_error_ = "Could not save your current profile. Selection unchanged."; return false; }
        const auto preferences = this->profiles_.Read(index, this->profile_error_);
        if (!preferences) return false;
        auto snapshot = this->CaptureSettings();
        snapshot.OverlayPreferences(*preferences);
        this->ReleaseChatInput();
        this->profiles_.Select(index);
        this->LoadSettings(snapshot);
        this->profile_dropdown_ = false;
        this->profile_error_.clear();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::DrawProfileControls(void) -> void
    {
        const auto selector = this->ConfigLayout().ProfileSelector(), add = this->ConfigLayout().ProfileAdd();
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        this->config_controls_.Dropdown(selector, this->profiles_.Ready() ? this->profiles_.Current().name.c_str() : "Default", this->profile_dropdown_, Contains(selector, x, y));
        this->config_controls_.IconButton(add, ui::icon_glyph::add, Contains(add, x, y));
        const auto actions = this->ConfigLayout().ProfileActionsButton();
        this->config_controls_.IconButton(actions, ui::icon_glyph::more, this->profile_actions_ || Contains(actions, x, y));
    }

    auto hud_runtime::DrawProfileOverlay(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->config_collapsed_) return;
        const auto& theme = this->design_system_.GetTheme();
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        auto body = theme.body_text; body.scale = 0.76f; body.alignment = ui::text_alignment::left;
        auto caption = theme.caption_text; caption.scale = 0.68f; caption.alignment = ui::text_alignment::left;
        if (this->profile_actions_)
        {
            this->config_controls_.Panel(this->ConfigLayout().ProfileActionsMenu());
            for (size_t i = 0; i < std::size(ui::profile_actions); ++i)
            {
                const auto row = this->ConfigLayout().ProfileActionRow(i);
                const bool available = i + 1 != static_cast<size_t>(profile_dialog::delete_profile) || this->profiles_.Selected() != 0;
                this->config_controls_.NavigationItem(row, ui::profile_actions[i], i == this->profile_keyboard_selection_, Contains(row, x, y), available);
                if (!available && Contains(row, x, y)) this->config_controls_.Tooltip("Default cannot be deleted.", row, viewport);
            }
        }
        if (this->profile_dropdown_)
        {
            this->config_controls_.Panel(this->ConfigLayout().ProfileMenu(this->profiles_.Entries().size()));
            for (size_t row = 0; row < std::min<size_t>(7, this->profiles_.Entries().size()); ++row)
            {
                const auto index = row + this->profile_scroll_;
                if (index >= this->profiles_.Entries().size()) break;
                const auto bounds = this->ConfigLayout().ProfileRow(row, this->profiles_.Entries().size());
                this->config_controls_.NavigationItem(bounds, this->profiles_.Entries()[index].name.c_str(), index == this->profiles_.Selected(),
                    Contains(bounds, x, y) || index == this->profile_keyboard_selection_);
            }
        }
        if (!this->profile_name_.Focused())
        {
            if (!this->profile_error_.empty()) this->config_controls_.Tooltip(this->profile_error_.c_str(), this->ConfigLayout().ProfileSelector(), viewport);
            return;
        }
        this->design_system_.FillRect(this->config_bounds_, 0x66000000);
        const bool sharing = this->profile_dialog_ == profile_dialog::share;
        const bool importing = this->profile_dialog_ == profile_dialog::import_profile;
        const bool duplicate = this->profile_dialog_ == profile_dialog::duplicate;
        const bool deleting = this->profile_dialog_ == profile_dialog::delete_profile;
        const bool extended = sharing || importing;
        const ui::profile_dialog_layout dialog{this->config_bounds_, viewport, extended};
        const auto bounds = dialog.Bounds();
        this->config_controls_.Panel(bounds);
        auto title = body; title.scale = 0.9f;
        this->config_controls_.Text(deleting ? "Delete profile?" : sharing ? "Share profile" : importing ? "Import profile" : duplicate ? "Duplicate profile" : "Create profile",
            dialog.Title(), title);
        const char* help = deleting ? "Delete this profile and switch to Default?" : sharing ? "Copy this key to share your layout and settings." : importing ? "Paste a share key, choose a name, then import." :
            duplicate ? "Copy the selected profile into a separate setup." : "Start with your current layout and module settings.";
        this->config_controls_.Text(help, dialog.Help(), caption);
        this->config_controls_.Text("Profile name", dialog.NameLabel(), extended ? caption : body);
        if (sharing || deleting)
        {
            const auto field = dialog.Field();
            this->design_system_.FillRoundedRect(field, (theme.colors.surface & 0x00FFFFFF) | 0xA0000000, 4);
            this->config_controls_.Text(this->profile_name_.Editor().Text().c_str(), {field.x + 10, field.y, field.width - 20, field.height}, body);
        }
        else this->config_controls_.TextInput(dialog.Field(), this->profile_name_, !this->profile_error_.empty() && (!importing || this->profile_import_.has_value()));
        if (extended)
        {
            this->design_system_.FillRect({dialog.Field().x, bounds.y + 70, dialog.Field().width, 1}, theme.colors.border);
            this->config_controls_.Text("Share key", dialog.KeyLabel(), caption);
            this->profile_key_view_.Draw(this->design_system_, this->config_controls_, dialog.Key(), this->profile_share_key_, Contains(dialog.Key(), x, y),
                importing && !this->profile_import_ && !this->profile_error_.empty());
            const char* status = sharing ? (this->profile_key_copied_ ? "Copied to clipboard. Ready to share." : "Click the key or press Ctrl+C to copy.") :
                this->profile_import_ ? "Key ready. Choose a name above, then Import." : "Click Paste key or press Ctrl+V.";
            this->config_controls_.Text(status, dialog.Status(), caption);
            auto action = body; action.alignment = ui::text_alignment::center;
            if (importing)
            {
                this->config_controls_.Button(dialog.Paste(), "Paste key", action, Contains(dialog.Paste(), x, y));
            }
        }
        if (!this->profile_error_.empty())
        {
            auto error = caption; error.top_color = 0xFFFFA0A0;
            this->config_controls_.Text(this->profile_error_.c_str(), dialog.Error(), error);
        }
        else if (deleting) this->config_controls_.Text("This cannot be undone.", dialog.Error(), caption);
        auto button = body; button.alignment = ui::text_alignment::center;
        this->config_controls_.Button(dialog.Cancel(), sharing ? "Close" : "Cancel", button,
            Contains(dialog.Cancel(), x, y) || (deleting && this->profile_keyboard_selection_ == 0));
        const bool available = !importing || this->profile_import_.has_value();
        if (!available) button.top_color = button.bottom_color = (button.top_color & 0x00FFFFFF) | 0x80000000;
        this->config_controls_.Button(dialog.Create(), deleting ? "Delete" : sharing ? "Copy key" : importing ? "Import" : duplicate ? "Duplicate" : "Create", button,
            available && (Contains(dialog.Create(), x, y) || (deleting && this->profile_keyboard_selection_ == 1)),
            deleting ? ui::button_tone::danger : ui::button_tone::normal);
        this->config_controls_.IconButton(dialog.Close(), ui::icon_glyph::close, Contains(dialog.Close(), x, y));
    }
}
