#include "hud_runtime.hpp"
#include "settings/profile_share.hpp"
#include "config/clipboard.hpp"
#include "chat/chat_encoding.hpp"
#include "ui/text_fit.hpp"

#include <algorithm>
#include <string_view>

namespace
{
    constexpr uint64_t profile_target = 0xF000000000000000ULL;
}

namespace xiui
{
    auto hud_runtime::UpdateCharacterProfile(const uint64_t now) -> void
    {
        if (this->core_ == nullptr) return;
        if (now < this->next_profile_identity_update_) return;
        this->next_profile_identity_update_ = now + 250;
        this->ObserveProfileCharacter(this->profile_identity_.Read(this->core_).value_or(""));
    }

    auto hud_runtime::BindObservedProfile(void) -> void
    {
        if (this->profile_observed_character_.empty()) return;
        this->profile_character_ = this->profile_observed_character_;
        this->profile_character_ready_ = true;
    }

    auto hud_runtime::ObserveProfileCharacter(std::string character) -> bool
    {
        this->profile_tracking_ = true;
        if (!this->profiles_.Ready()) return false;
        if (character.empty())
        {
            if (this->profile_character_ready_)
            {
                this->SaveSettings();
                this->profile_character_ready_ = false;
                this->ReleaseControllerFocus();
                this->CloseProfileControls();
                this->FinishConfigInput(false);
                this->CloseChoiceInput();
                this->ReleaseChatInput();
                this->FinishInteraction();
            }
            this->profile_character_ready_ = false;
            this->profile_observed_character_.clear();
            return false;
        }
        if (character == this->profile_character_)
        {
            this->profile_observed_character_ = std::move(character);
            const bool resumed = !this->profile_character_ready_;
            this->profile_character_ready_ = true;
            if (resumed) this->SaveSettings();
            return true;
        }
        if (character == this->profile_observed_character_) return false;
        if (this->profile_character_ready_) this->SaveSettings();
        this->profile_character_ready_ = false;
        this->profile_observed_character_ = std::move(character);
        this->ReleaseControllerFocus();
        this->CloseProfileControls();
        this->FinishConfigInput(false);
        this->CloseChoiceInput();
        this->ReleaseChatInput();
        this->FinishInteraction();
        if (!this->DrainSettings())
        { this->profile_error_ = "Could not save the previous profile. Choose a profile to retry."; return false; }
        if (const auto bound = this->profiles_.BoundProfile(this->profile_observed_character_))
        {
            const auto preferences = this->profiles_.Read(*bound, this->profile_error_);
            if (!preferences) return false;
            auto snapshot = this->CaptureSettings();
            OverlayProfilePreferences(snapshot, *preferences);
            this->profiles_.Select(*bound);
            this->LoadSettings(snapshot);
        }
        this->BindObservedProfile();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::CloseProfileControls(void) -> void
    {
        this->profile_transfer_.Cancel();
        this->config_.focus.modal_item = 0;
        this->config_.focus.modal_id.clear();
        this->profile_name_.Cancel();
        this->profile_dropdown_ = false;
        this->profile_actions_ = false;
        this->profile_file_.clear();
        this->profile_legacy_import_ = false;
        this->profile_import_.reset();
        this->profile_dialog_ = profile_dialog::create;
        this->profile_error_.clear();
        this->confirmation_module_ = nullptr;
        this->confirmation_message_.clear();
    }

    auto hud_runtime::ActivateConfigAction(hud_module& module, const size_t index) -> bool
    {
        if (index >= module.OptionCount()) return false;
        const auto option = module.Option(index);
        if (!option.available || option.kind != module_option_kind::action) return false;
        if (option.confirmation == nullptr || option.confirmation[0] == '\0')
        {
            module.OnOptionChanged(index);
            if (&module == &this->actionbars_ && this->actionbars_.DialogOpen())
                this->FinishActionBarsLaunch();
            this->SaveSettings();
            return true;
        }
        this->FinishConfigInput(true, false);
        this->ReleaseChatInput();
        this->CloseProfileControls();
        this->config_.active_color = nullptr;
        this->config_.active_slider = nullptr;
        this->confirmation_module_ = &module;
        this->confirmation_option_ = index;
        this->confirmation_view_ = module.ConfigView();
        this->confirmation_message_ = option.confirmation;
        this->profile_dialog_ = profile_dialog::confirm_action;
        this->profile_keyboard_selection_ = 0;
        this->config_.focus.modal_item = 0;
        this->config_.focus.modal_id = "profile/" + std::to_string(static_cast<int>(profile_dialog::confirm_action));
        this->config_.focus.adjusting = false;
        this->profile_name_.Open(option.label != nullptr ? option.label : "Selected settings");
        this->ui_sounds_.MenuOpened(::GetTickCount64());
        return true;
    }

    auto hud_runtime::ApplyConfirmedAction(void) -> bool
    {
        if (this->profile_dialog_ == profile_dialog::reset_module && this->profile_name_.Focused())
        {
            auto* module = this->confirmation_module_;
            const bool valid = module != nullptr && this->ModuleMenuAvailable(*module);
            this->CloseProfileControls();
            return valid && this->ResetModule(*module, true);
        }
        if (this->profile_dialog_ == profile_dialog::reset_defaults && this->profile_name_.Focused() && this->editing_)
        {
            this->CloseProfileControls();
            this->ResetSettings();
            return true;
        }
        auto* module = this->confirmation_module_;
        const auto index = this->confirmation_option_;
        if (this->profile_dialog_ != profile_dialog::confirm_action || !this->profile_name_.Focused() || module == nullptr) return false;
        const bool current = this->editing_ && module == this->config_.module && module->ConfigView() == this->confirmation_view_ &&
            index < module->OptionCount() && this->MatchesConfigOption(index);
        const auto option = current ? module->Option(index) : module_option{};
        const bool valid = current && option.available && option.kind == module_option_kind::action && option.confirmation != nullptr &&
            this->confirmation_message_ == option.confirmation;
        this->CloseProfileControls();
        if (!valid) return false;
        if (module == &this->general_ && index == this->general_.HideAllUiOption())
        {
            this->SetVisible(false);
            return true;
        }
        if (module == &this->general_)
            if (const auto preset = this->general_.HudPresetFromOption(index)) return this->ApplyHudPreset(*preset);
        module->OnOptionChanged(index);
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::OpenProfileDialog(const profile_dialog mode) -> bool
    {
        if (!this->profiles_.Ready() && mode != profile_dialog::reset_defaults) return false;
        if (mode == profile_dialog::delete_profile && this->profiles_.Selected() == 0)
        { this->profile_error_ = "Default cannot be deleted."; return false; }
        if (mode == profile_dialog::rename && this->profiles_.Selected() == 0)
        { this->profile_error_ = "Default cannot be renamed."; return false; }
        this->FinishConfigInput(true, false);
        this->ReleaseChatInput();
        this->CloseProfileControls();
        this->config_.active_color = nullptr;
        this->profile_dialog_ = mode;
        this->profile_source_ = this->profiles_.Selected();
        this->profile_keyboard_selection_ = 0;
        this->config_.focus.modal_id = "profile/" + std::to_string(static_cast<int>(mode));
        this->config_.focus.adjusting = false;
        std::string name;
        if (mode == profile_dialog::reset_defaults)
        {
            name = "Entire XIUI+ configuration";
            this->confirmation_message_ = "This will reset all XIUI+ settings and layouts to defaults.";
        }
        if (mode == profile_dialog::duplicate) name = this->profiles_.UniqueName(this->profiles_.Current().name + " Copy");
        if (mode == profile_dialog::delete_profile || mode == profile_dialog::rename) name = this->profiles_.Current().name;
        if (mode == profile_dialog::share) name = this->profiles_.Current().name;
        this->profile_name_.Open(std::move(name));
        this->ui_sounds_.MenuOpened(::GetTickCount64());
        return true;
    }

    auto hud_runtime::ProfileActionAvailable(const size_t index) const -> bool
    {
        if (index >= std::size(ui::profile_actions)) return false;
        return index == ui::profile_reset_action || (this->profiles_.Ready() &&
            ((index != ui::profile_delete_action && index != ui::profile_rename_action) || this->profiles_.Selected() != 0));
    }

    auto hud_runtime::OpenProfileAction(const size_t index) -> bool
    {
        if (!this->ProfileActionAvailable(index)) return false;
        static constexpr profile_dialog modes[]{profile_dialog::duplicate, profile_dialog::share, profile_dialog::import_profile,
            profile_dialog::rename, profile_dialog::delete_profile, profile_dialog::reset_defaults};
        return this->OpenProfileDialog(modes[index]);
    }

    auto hud_runtime::MoveProfileAction(const int direction) -> void
    {
        const auto count = std::size(ui::profile_actions);
        for (size_t step = 0; step < count; ++step)
        {
            this->profile_keyboard_selection_ = (this->profile_keyboard_selection_ + (direction < 0 ? count - 1 : 1)) % count;
            if (this->ProfileActionAvailable(this->profile_keyboard_selection_)) break;
        }
    }

    auto hud_runtime::StartProfileTransfer(profile_transfer_request request) -> bool
    {
        if (this->profile_transfer_.Busy()) return false;
        this->profile_file_.clear();
        if (request.kind == profile_transfer_kind::import_file || request.kind == profile_transfer_kind::import_key)
        {
            this->profile_import_.reset(); this->profile_legacy_import_ = false;
        }
        this->profile_error_.clear();
        this->config_.focus.modal_item = 0;
        if (this->profile_transfer_.Start(std::move(request))) return true;
        this->profile_error_ = "Could not start the file operation. Try again.";
        return false;
    }

    auto hud_runtime::UpdateProfileTransfer() -> void
    {
        auto result = this->profile_transfer_.Take();
        if (!result || !this->profile_name_.Focused()) return;
        if (result->cancelled) return;
        if (!result->error.empty()) { this->profile_error_ = std::move(result->error); return; }
        this->profile_error_.clear();
        if (result->kind == profile_transfer_kind::export_file) this->profile_file_ = std::move(result->path);
        else if (result->profile)
        {
            this->profile_name_.Open(this->profiles_.UniqueName(result->profile->name));
            this->profile_import_ = std::move(result->profile->preferences);
            this->profile_file_ = std::move(result->path);
            this->profile_legacy_import_ = result->kind == profile_transfer_kind::import_key;
        }
    }

    auto hud_runtime::ExportProfileFile(const std::filesystem::path& path) -> bool
    {
        if (this->profile_transfer_.Busy()) return false;
        profile_transfer_request request;
        request.kind = profile_transfer_kind::export_file; request.path = path;
        request.name = this->profiles_.Current().name;
        request.snapshot = this->CaptureSettings();
        return this->StartProfileTransfer(std::move(request));
    }

    auto hud_runtime::BrowseProfileFile(const bool save) -> void
    {
        if (this->profile_transfer_.Busy()) return;
        profile_transfer_request request;
        request.kind = save ? profile_transfer_kind::export_file : profile_transfer_kind::import_file;
        request.browse = true;
        if (save)
        {
            request.name = this->profiles_.Current().name;
            request.snapshot = this->CaptureSettings();
        }
        this->StartProfileTransfer(std::move(request));
    }

    auto hud_runtime::LoadProfileFile(const std::filesystem::path& path) -> bool
    {
        profile_transfer_request request; request.kind = profile_transfer_kind::import_file; request.path = path;
        return this->StartProfileTransfer(std::move(request));
    }

    auto hud_runtime::LoadProfileKey(const std::string_view key) -> bool
    {
        if (this->profile_transfer_.Busy()) return false;
        profile_transfer_request request; request.kind = profile_transfer_kind::import_key; request.key = key;
        return this->StartProfileTransfer(std::move(request));
    }

    auto hud_runtime::PasteProfileKey(void) -> bool
    {
        if (this->profile_transfer_.Busy()) return false;
        const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        const auto key = ui::ReadClipboardText(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr, maximum_profile_key);
        if (!key)
        {
            this->profile_import_.reset(); this->profile_file_.clear(); this->profile_legacy_import_ = false;
            this->profile_error_ = "Copy an XIUI+ share key, then paste it here.";
            return false;
        }
        return this->LoadProfileKey(*key);
    }

    auto hud_runtime::ProfileSurface(const D3DVIEWPORT8& viewport) const -> bool
    {
        UNREFERENCED_PARAMETER(viewport);
        return this->editing_ && !this->config_.collapsed && (this->profile_name_.Focused() || this->profile_dropdown_ || this->profile_actions_);
    }

    auto hud_runtime::DrainSettings(void) const -> bool
    {
        const bool settings = this->settings_writer_.Flush();
        const bool profile = this->profile_writer_.Flush();
        return settings && profile;
    }

    auto hud_runtime::ProfileSoundTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        if (!this->editing_ || this->config_.collapsed) return 0;
        const auto x = this->ConfigMouseX(), y = this->ConfigMouseY();
        if (this->profile_name_.Focused())
        {
            const bool extended = this->profile_dialog_ == profile_dialog::share || this->profile_dialog_ == profile_dialog::import_profile;
            const ui::profile_dialog_layout dialog{this->config_.bounds, this->ConfigViewport(viewport), extended};
            if (Contains(dialog.Cancel(), x, y)) return profile_target | 11;
            if (Contains(dialog.Close(), x, y)) return profile_target | 13;
            if (this->profile_transfer_.Busy()) return 0;
            if (this->profile_dialog_ != profile_dialog::share && this->profile_dialog_ != profile_dialog::delete_profile && !this->ConfirmingAction() && Contains(dialog.Field(), x, y)) return profile_target | 10;
            if (Contains(dialog.Create(), x, y) && (this->profile_dialog_ != profile_dialog::import_profile || this->profile_import_)) return profile_target | 12;
            if (this->profile_dialog_ == profile_dialog::import_profile && Contains(dialog.Browse(), x, y)) return profile_target | 14;
            if (this->profile_dialog_ == profile_dialog::import_profile && Contains(dialog.Paste(), x, y)) return profile_target | 15;
            return 0;
        }
        if (this->profiles_.Ready() && Contains(this->ConfigLayout().ProfileSelector(), x, y)) return profile_target | 1;
        if (this->profiles_.Ready() && Contains(this->ConfigLayout().ProfileAdd(), x, y)) return profile_target | 2;
        if (Contains(this->ConfigLayout().ProfileActionsButton(), x, y)) return profile_target | 3;
        if (this->profile_actions_)
            for (size_t i = 0; i < std::size(ui::profile_actions); ++i)
            {
                if (!this->ProfileActionAvailable(i)) continue;
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
            else if (target == 11 || target == 13)
            {
                const bool extended = this->profile_dialog_ == profile_dialog::share || this->profile_dialog_ == profile_dialog::import_profile;
                const ui::profile_dialog_layout dialog{this->config_.bounds, this->ConfigViewport(viewport), extended};
                this->BeginCloseInteraction(interaction::close_profile, target == 11 ? dialog.Cancel() : dialog.Close());
            }
            else if (target == 12 && this->profile_dialog_ == profile_dialog::share) this->BrowseProfileFile(true);
            else if (target == 12 && this->profile_dialog_ == profile_dialog::delete_profile) this->DeleteProfile();
            else if (target == 12 && this->profile_dialog_ == profile_dialog::rename) this->RenameProfile();
            else if (target == 12 && this->ConfirmingAction()) this->ApplyConfirmedAction();
            else if (target == 12) this->CreateProfile();
            else if (target == 14) this->BrowseProfileFile(false);
            else if (target == 15) this->PasteProfileKey();
            return true;
        }
        if (target == 2)
        {
            this->OpenProfileDialog(profile_dialog::create);
            return true;
        }
        if (target == 3)
        {
            this->FinishConfigInput(true, false); this->ReleaseChatInput(); this->config_.active_color = nullptr;
            this->profile_dropdown_ = false; this->profile_actions_ = !this->profile_actions_;
            this->profile_keyboard_selection_ = 0; this->profile_error_.clear();
            if (!this->ProfileActionAvailable(0)) this->MoveProfileAction(1);
            return true;
        }
        if (this->profile_actions_)
        {
            if (target >= 20 && target < 20 + std::size(ui::profile_actions)) this->OpenProfileAction(static_cast<size_t>(target - 20));
            else this->profile_actions_ = false;
            return true;
        }
        if (target == 1)
        {
            this->FinishConfigInput(true, false);
            this->ReleaseChatInput();
            this->config_.active_color = nullptr;
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
        if (key == VK_ESCAPE)
        {
            if (this->ConfirmingAction()) this->confirmation_key_release_ = key;
            this->CloseProfileControls();
            return true;
        }
        if (this->profile_actions_)
        {
            if (key == VK_RETURN && !repeat) this->OpenProfileAction(this->profile_keyboard_selection_);
            else if (key == VK_UP || key == VK_DOWN) this->MoveProfileAction(key == VK_UP ? -1 : 1);
            return true;
        }
        if (this->profile_name_.Focused())
        {
            if (this->profile_transfer_.Busy()) return true;
            if (this->profile_dialog_ == profile_dialog::delete_profile || this->ConfirmingAction())
            {
                if (key == VK_TAB || key == VK_LEFT || key == VK_RIGHT)
                {
                    this->profile_keyboard_selection_ ^= 1;
                    this->config_.focus.modal_item = static_cast<uint8_t>(this->profile_keyboard_selection_);
                }
                else if (key == VK_RETURN && !repeat)
                {
                    if (this->ConfirmingAction()) this->confirmation_key_release_ = key;
                    this->ui_sounds_.Select(::GetTickCount64());
                    if (this->profile_keyboard_selection_ == 1)
                    {
                        if (this->ConfirmingAction()) this->ApplyConfirmedAction();
                        else this->DeleteProfile();
                    }
                    else this->CloseProfileControls();
                }
                return true;
            }
            const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (this->profile_dialog_ == profile_dialog::share)
            {
                if (!repeat && key == VK_RETURN) this->BrowseProfileFile(true);
                return true;
            }
            if (this->profile_dialog_ == profile_dialog::import_profile && control && key == 'O')
            { if (!repeat) this->BrowseProfileFile(false); return true; }
            if (this->profile_dialog_ == profile_dialog::import_profile && control && key == 'V')
            { if (!repeat) this->PasteProfileKey(); return true; }
            if (key == VK_RETURN)
            {
                if (!repeat)
                {
                    this->ui_sounds_.Select(::GetTickCount64());
                    if (this->profile_dialog_ == profile_dialog::rename) this->RenameProfile();
                    else this->CreateProfile();
                }
            }
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
        if (this->profile_transfer_.Busy()) return false;
        if (this->profile_dialog_ == profile_dialog::import_profile && !this->profile_import_)
        { this->profile_error_ = "Choose a valid profile file first."; return false; }
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
            OverlayProfilePreferences(snapshot, *this->profile_import_);
            if (!this->profiles_.Create(this->profile_name_.Editor().Text(), snapshot, this->profile_error_)) return false;
            this->LoadSettings(snapshot);
        }
        else if (!this->profiles_.Create(this->profile_name_.Editor().Text(), snapshot, this->profile_error_)) return false;
        this->BindObservedProfile();
        this->CloseProfileControls();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::RenameProfile(void) -> bool
    {
        if (this->profile_dialog_ != profile_dialog::rename || !this->profile_name_.Focused() ||
            this->profile_source_ == 0 || this->profile_source_ != this->profiles_.Selected()) return false;
        this->SaveSettings();
        if (!this->DrainSettings())
        { this->profile_error_ = "Could not save your settings. The profile was not renamed."; return false; }
        if (!this->profiles_.Rename(this->profile_source_, this->profile_name_.Editor().Text(), this->profile_error_)) return false;
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
        OverlayProfilePreferences(snapshot, *fallback);
        if (!this->profiles_.Remove(this->profile_source_, this->profile_error_)) return false;
        this->BindObservedProfile();
        this->LoadSettings(snapshot);
        this->CloseProfileControls();
        this->profile_scroll_ = 0;
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::SelectProfile(const size_t index) -> bool
    {
        if (index >= this->profiles_.Entries().size()) return false;
        if (index == this->profiles_.Selected())
        {
            this->profile_dropdown_ = false;
            if (this->profile_tracking_ && !this->profile_observed_character_.empty())
            { this->BindObservedProfile(); this->profile_error_.clear(); this->SaveSettings(); }
            return true;
        }
        this->FinishConfigInput(true, false);
        this->FinishInteraction();
        this->SaveSettings();
        if (!this->DrainSettings())
        { this->profile_error_ = "Could not save your current profile. Selection unchanged."; return false; }
        const auto preferences = this->profiles_.Read(index, this->profile_error_);
        if (!preferences) return false;
        auto snapshot = this->CaptureSettings();
        OverlayProfilePreferences(snapshot, *preferences);
        this->ReleaseChatInput();
        this->profiles_.Select(index);
        this->LoadSettings(snapshot);
        this->BindObservedProfile();
        this->profile_dropdown_ = false;
        this->profile_error_.clear();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::DrawProfileControls(void) -> void
    {
        const auto selector = this->ConfigLayout().ProfileSelector(), add = this->ConfigLayout().ProfileAdd();
        const auto x = this->ConfigMouseX(), y = this->ConfigMouseY();
        const auto* profileName = this->profiles_.Ready() ? this->profiles_.Current().name.c_str() : "Default";
        this->config_controls_.Dropdown(selector, profileName, this->profile_dropdown_, Contains(selector, x, y));
        this->config_controls_.IconButton(add, ui::icon_glyph::add, Contains(add, x, y));
        const auto actions = this->ConfigLayout().ProfileActionsButton();
        this->config_controls_.IconButton(actions, ui::icon_glyph::more, this->profile_actions_ || Contains(actions, x, y));
        if (Contains(add, x, y) || Contains(actions, x, y))
        {
            this->config_.tooltip = Contains(add, x, y) ? "Create profile" : "Profile actions";
            this->config_.tooltip_anchor = Contains(add, x, y) ? add : actions;
        }
    }

    auto hud_runtime::DrawProfileOverlay(const D3DVIEWPORT8& viewport) -> void
    {
        const auto transform = this->design_system_.Transform(this->ConfigTransform());
        if (this->config_.collapsed) return;
        const auto& theme = this->design_system_.GetTheme();
        const auto x = this->ConfigMouseX(), y = this->ConfigMouseY();
        auto body = theme.body_text; body.scale = 0.76f; body.alignment = ui::text_alignment::left;
        auto caption = theme.caption_text; caption.scale = 0.68f; caption.alignment = ui::text_alignment::left;
        if (this->profile_actions_)
        {
            this->config_controls_.Panel(this->ConfigLayout().ProfileActionsMenu());
            for (size_t i = 0; i < std::size(ui::profile_actions); ++i)
            {
                const auto row = this->ConfigLayout().ProfileActionRow(i);
                const bool available = this->ProfileActionAvailable(i);
                this->config_controls_.NavigationItem(row, ui::profile_actions[i], i == this->profile_keyboard_selection_, Contains(row, x, y), available);
                if (!available && Contains(row, x, y)) this->config_controls_.Tooltip(!this->profiles_.Ready() ? i18n::Text("Profiles are unavailable.") :
                    i == ui::profile_rename_action ? i18n::Text("Default cannot be renamed.") : i18n::Text("Default cannot be deleted."), row, viewport);
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
                    Contains(bounds, x, y) || index == this->profile_keyboard_selection_, true, false);
            }
        }
        if (!this->profile_name_.Focused())
        {
            if (!this->profile_error_.empty()) this->config_controls_.Tooltip(this->profile_error_.c_str(), this->ConfigLayout().ProfileSelector(), viewport);
            return;
        }
        this->design_system_.FillRect(this->config_.bounds, 0x66000000);
        const bool sharing = this->profile_dialog_ == profile_dialog::share;
        const bool importing = this->profile_dialog_ == profile_dialog::import_profile;
        const bool duplicate = this->profile_dialog_ == profile_dialog::duplicate;
        const bool renaming = this->profile_dialog_ == profile_dialog::rename;
        const bool deleting = this->profile_dialog_ == profile_dialog::delete_profile;
        const bool confirming = this->ConfirmingAction();
        const auto confirmationOption = this->profile_dialog_ == profile_dialog::confirm_action && this->confirmation_module_ != nullptr && this->confirmation_option_ < this->confirmation_module_->OptionCount() ?
            this->confirmation_module_->Option(this->confirmation_option_) : module_option{};
        const bool moduleReset = this->profile_dialog_ == profile_dialog::reset_module;
        const bool warning = moduleReset || confirmationOption.warning;
        const bool resetting = this->profile_dialog_ == profile_dialog::reset_defaults;
        const bool extended = sharing || importing;
        const bool busy = this->profile_transfer_.Busy();
        const ui::profile_dialog_layout dialog{this->config_.bounds, this->ConfigViewport(viewport), extended};
        const auto bounds = dialog.Bounds();
        this->config_controls_.Panel(bounds);
        auto title = body; title.scale = 0.9f;
        const auto warningColor = ui::button_renderer::FaceColor(theme, true, ui::button_tone::danger);
        if (warning) title.top_color = title.bottom_color = warningColor;
        const auto warningTitle = warning ? this->profile_name_.Editor().Text() + "?" : std::string{};
        this->config_controls_.Text(moduleReset ? i18n::Text("THIS WILL RESET ALL SETTINGS!") : warning ? warningTitle.c_str() : resetting ? i18n::Text("Reset to defaults?") : confirming ? i18n::Text("Apply changes?") : deleting ? i18n::Text("Delete profile?") : sharing ? i18n::Text("Share profile") : importing ? i18n::Text("Import profile") : duplicate ? i18n::Text("Duplicate profile") : renaming ? i18n::Text("Rename profile") : i18n::Text("Create profile"),
            dialog.Title(), title);
        const char* help = confirming ? this->confirmation_message_.c_str() : deleting ? "Delete this profile and switch to Default?" : sharing ? "Save a profile file, then attach it to your message." : importing ? "Choose an XIUI+ profile file to import." :
            duplicate ? "Copy the selected profile into a separate setup." : renaming ? "Choose a new name for this profile." : "Start with your current layout and module settings.";
        if (warning)
        {
            std::string_view remaining{i18n::Text(help)};
            auto line = dialog.Help();
            while (!remaining.empty())
            {
                const auto end = remaining.find('\n');
                const std::string text{remaining.substr(0, end)};
                auto style = caption;
                if (line.y == dialog.Help().y) style.top_color = style.bottom_color = warningColor;
                this->config_controls_.Text(text.c_str(), line, style);
                if (end == std::string_view::npos) break;
                remaining.remove_prefix(end + 1);
                line.y += 25;
            }
        }
        else
        {
            this->config_controls_.Text(i18n::Text(help), dialog.Help(), caption);
            this->config_controls_.Text(resetting ? i18n::Text("Scope") : confirming ? i18n::Text("Selection") : i18n::Text("Profile name"), dialog.NameLabel(), extended ? caption : body);
        }
        if (!warning && (sharing || deleting || confirming))
        {
            const auto field = dialog.Field();
            this->design_system_.FillRoundedRect(field, (theme.colors.surface & 0x00FFFFFF) | 0xA0000000, 4);
            this->config_controls_.Text(this->profile_name_.Editor().Text().c_str(), {field.x + 10, field.y, field.width - 20, field.height}, body);
        }
        else if (!warning) this->config_controls_.TextInput(dialog.Field(), this->profile_name_, !this->profile_error_.empty() && (!importing || this->profile_import_.has_value()));
        if (extended)
        {
            this->design_system_.FillRect({dialog.Field().x, bounds.y + 70, dialog.Field().width, 1}, theme.colors.border);
            this->config_controls_.Text(i18n::Text("Profile file"), dialog.SourceLabel(), caption);
            const auto source = dialog.Source();
            this->design_system_.FillRoundedRect(source, (theme.colors.chrome & 0x00FFFFFF) | 0xE0000000, 4);
            const float inset = importing ? 136.0f : 12.0f;
            const rect fileBounds{source.x + inset, source.y + 12, source.width - inset - 12, 28};
            const auto filename = !this->profile_file_.empty() ? ToUtf8(this->profile_file_.filename().wstring()) :
                this->profile_legacy_import_ ? std::string{"Legacy share key"} : sharing ? std::string{"Ready to export as .xiui"} : std::string{"No file selected"};
            const auto fitted = ui::FitText(this->design_system_, filename, fileBounds.width, this->config_controls_.Style(body));
            this->config_controls_.Text(fitted.c_str(), fileBounds, body);
            this->config_controls_.Text(sharing ? i18n::Text("Includes your layout and module settings.") : i18n::Text("Imports as a separate profile; existing profiles are kept."),
                {source.x + 12, source.y + 50, source.width - 24, 24}, caption);
            const char* status = busy ? (this->profile_transfer_.Selecting() ? "Choose a file in Windows, or Cancel here." : sharing ? "Saving profile..." : "Reading profile...") : sharing ? (this->profile_file_.empty() ? "Choose Save file to export your profile." : "File saved. Attach it to share your profile.") :
                this->profile_import_ ? "Profile ready. Choose a name above, then Import." : "Choose a .xiui file, or press Ctrl+O.";
            this->config_controls_.Text(i18n::Text(status), dialog.Status(), caption);
            auto action = body; action.alignment = ui::text_alignment::center;
            if (importing)
            {
                if (busy) action.top_color = action.bottom_color = (action.top_color & 0x00FFFFFF) | 0x80000000;
                this->config_controls_.Button(dialog.Browse(), i18n::Text("Choose file"), action, !busy && Contains(dialog.Browse(), x, y));
                this->config_controls_.Button(dialog.Paste(), i18n::Text("Paste old key"), action, !busy && Contains(dialog.Paste(), x, y));
            }
        }
        if (!this->profile_error_.empty())
        {
            auto error = caption; error.top_color = 0xFFFFA0A0;
            this->config_controls_.Text(i18n::Text(this->profile_error_.c_str()), dialog.Error(), error);
        }
        else if (deleting) this->config_controls_.Text(i18n::Text("This cannot be undone."), dialog.Error(), caption);
        auto button = body; button.alignment = ui::text_alignment::center;
        this->config_controls_.Button(dialog.Cancel(), sharing && !busy ? i18n::Text("Close") : i18n::Text("Cancel"), button,
            Contains(dialog.Cancel(), x, y) || ((deleting || confirming) && this->profile_keyboard_selection_ == 0));
        const bool available = !busy && (!importing || this->profile_import_.has_value());
        if (!available) button.top_color = button.bottom_color = (button.top_color & 0x00FFFFFF) | 0x80000000;
        this->config_controls_.Button(dialog.Create(), moduleReset ? i18n::Text("Reset settings") : warning ? i18n::Text(confirmationOption.action_text) : resetting ? i18n::Text("Reset") : confirming ? i18n::Text("Apply") : deleting ? i18n::Text("Delete") : sharing ? i18n::Text("Save file") : importing ? i18n::Text("Import") : duplicate ? i18n::Text("Duplicate") : renaming ? i18n::Text("Rename") : i18n::Text("Create"), button,
            available && (Contains(dialog.Create(), x, y) || ((deleting || confirming) && this->profile_keyboard_selection_ == 1)),
            deleting || resetting || warning ? ui::button_tone::danger : ui::button_tone::normal);
        this->config_controls_.IconButton(dialog.Close(), ui::icon_glyph::close, Contains(dialog.Close(), x, y));
    }
}
