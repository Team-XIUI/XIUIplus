#include "hud_runtime.hpp"

#include <algorithm>
#include <cctype>

namespace xiui
{
    auto hud_runtime::SatchelInputAvailable(const bool pointer) const -> bool
    {
        return this->visible_ && !this->editing_ && this->satchel_.ShouldRender({}) && this->ContextVisible(this->satchel_) &&
            this->inventory_source_.State().available && this->GameWindowFocused() && !this->ControllerNativeBusy() &&
            (pointer || !this->ControllerNativeMenuOpen()) && this->controller_surface_ != controller_surface::picker && !this->controller_pointer_down_ &&
            !this->actionbars_.InputFocused() && !this->ChatFocused() && !this->chat_.HistoryExpanded() && !this->chat_second_.HistoryExpanded();
    }

    auto hud_runtime::SuspendSatchelInput(void) -> void
    {
        const bool pointer = this->satchel_.PointerDown();
        if (pointer) this->satchel_pointer_release_ |= MK_LBUTTON;
        this->satchel_.SuspendInput();
        this->satchel_.SetHover(-1, -1);
        this->satchel_actions_.Cancel();
        if (pointer) { this->ReleasePointerCapture(); this->SetChatMouseCapture(false); }
    }

    auto hud_runtime::UpdateSatchel(const uint64_t now) -> void
    {
        this->satchel_source_.Update(this->core_, this->inventory_source_.State(), this->profile_identity_.Read(this->core_), now);
        this->satchel_cache_.Observe(this->satchel_source_.Snapshot(), now);
        // The pending mouse-up commits the drag, so held state follows button messages rather than polling or capture.
        if (this->satchel_.PointerDown() && (this->mouse_input_.Buttons() & (MK_LBUTTON | MK_RBUTTON)) == 0)
            this->SuspendSatchelInput();
        if (!this->SatchelInputAvailable(true)) this->SuspendSatchelInput();
        this->satchel_actions_.Update(this->core_, this->satchel_source_.Snapshot(), now);
        this->satchel_.SetFocusStyle(this->general_.FocusStyle());
        this->satchel_.SetConfigText(&this->general_.ConfigText());
        this->satchel_.Update(now);
        if (this->satchel_.TakeSettingsChanged()) this->SaveSettings();
    }

    auto hud_runtime::HandleSatchelCommand(std::string_view command) -> bool
    {
        std::string normalized{command};
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
        command = normalized;
        if (command == "/satchel" || command.starts_with("/satchel ") || command == "/satch" || command.starts_with("/satch "))
        {
            if (!this->satchel_.OverridesSatchel()) return false;
            command.remove_prefix(command.starts_with("/satchel") ? 8 : 6);
            while (!command.empty() && command.front() == ' ') command.remove_prefix(1);
            normalized = "satchel" + (command.empty() ? std::string{} : " " + std::string{command});
            command = normalized;
        }
        if (command != "satchel" && !command.starts_with("satchel ")) return false;
        if (command == "satchel config" || command == "satchel settings")
        {
            this->SetVisible(true);
            this->SelectConfigModule(&this->satchel_);
            this->SetEditing(true);
        }
        else if (command == "satchel status")
        {
            const auto& snapshot = this->satchel_source_.Snapshot();
            size_t items{}, bags{};
            for (const auto& bag : snapshot.bags) { items += bag.items.size(); bags += bag.ready && bag.capacity != 0; }
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false,
                    "[XIUI+] Satchel: %s; %zu loaded bags, %zu items; %zu cached characters; identity %s; window %s. %s",
                    snapshot.ready ? "settled" : "loading", bags, items, this->satchel_cache_.Characters().size(),
                    snapshot.identity.empty() ? "unavailable" : snapshot.identity.c_str(), this->satchel_.Opened() ? "open" : "closed", this->satchel_actions_.Status().c_str());
        }
        else if (command == "satchel" || command == "satchel toggle" || command == "satchel open" || command == "satchel show" || command == "satchel close" || command == "satchel hide")
        {
            const bool open = command == "satchel open" || command == "satchel show" ||
                ((command == "satchel" || command == "satchel toggle") && !this->satchel_.Opened());
            if (open)
            {
                this->SetVisible(true);
                this->SetEditing(false);
                this->ReleaseControllerFocus();
                this->ReleaseChatInput();
                this->satchel_.State().enabled = true;
            }
            else this->SuspendSatchelInput();
            this->satchel_.Open(open);
            this->SaveSettings();
        }
        else if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->Write(1, false, "[XIUI+] /xiuiplus satchel [open|close|config|status]; /xiuiplus focus satchel for controller browsing.");
        return true;
    }

    auto hud_runtime::HandleSatchelKey(const WPARAM key, const LPARAM flags, const bool blocked) -> bool
    {
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (key < this->satchel_key_release_.size() && this->satchel_key_release_[key] && released)
        { this->satchel_key_release_[key] = false; return true; }
        // Focused Satchel input holds Ashita's BlockInput, so Ashita-bound keys stay with its editors.
        if (released || (blocked && !this->satchel_.InputFocused()) || !this->SatchelInputAvailable(this->satchel_.InputFocused())) return false;
        const bool focused = this->satchel_.InputFocused();
        const bool consumed = this->satchel_.HandleKey(key, flags);
        if (consumed && !focused) this->ReleaseControllerFocus();
        if (consumed && !released && key < this->satchel_key_release_.size()) this->satchel_key_release_[key] = true;
        return consumed;
    }

    auto hud_runtime::HandleSatchelMouse(const uint32_t message, const WPARAM buttons, const float x, const float y, const bool inputBlocked) -> bool
    {
        const bool blocked = this->PointerInputBlocked(inputBlocked);
        const auto release = message == WM_LBUTTONUP ? MK_LBUTTON : message == WM_RBUTTONUP ? MK_RBUTTON : 0;
        if (release != 0 && (this->satchel_pointer_release_ & release) != 0 && (!this->SatchelInputAvailable(true) || !this->satchel_.PointerDown()))
        {
            this->satchel_pointer_release_ &= ~release;
            this->ReleasePointerCapture();
            this->SetChatMouseCapture(false);
            return true;
        }
        if (message == WM_MOUSEMOVE && this->satchel_pointer_release_ != 0 && !this->satchel_.PointerDown())
        {
            this->SetChatMouseCapture(true);
            return true;
        }
        if (!this->SatchelInputAvailable(true)) return false;
        this->satchel_.SetHover(x, y);
        const bool owned = this->satchel_.PointerDown();
        if (!this->satchel_.HandleMouse(message, buttons, x, y, blocked))
        {
            if (!owned && !Contains(this->satchel_.State().transform.Map(this->satchel_.State().bounds), x, y) && this->chat_mouse_block_saved_ && this->chat_pointer_ == nullptr && !this->ready_check_.PointerDown())
                this->SetChatMouseCapture(false);
            return false;
        }
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN)
        {
            this->ReleaseControllerFocus();
            this->chat_.Focus(false);
            this->chat_second_.Focus(false);
            this->satchel_pointer_release_ |= message == WM_RBUTTONDOWN ? MK_RBUTTON : MK_LBUTTON;
            const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
            const auto window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
            if (window != nullptr)
            { ::SetCapture(window); if (::GetCapture() == window) this->mouse_capture_window_ = window; }
        }
        this->SetChatMouseCapture(true);
        if (release != 0) { this->satchel_pointer_release_ &= ~release; this->ReleasePointerCapture(); }
        return true;
    }
}
