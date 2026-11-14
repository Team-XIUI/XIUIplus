#include "hud/hud_runtime.hpp"

namespace xiui
{
    auto hud_runtime::TreasureInputAvailable(const bool pointer) const -> bool
    {
        return this->visible_ && !this->editing_ && this->ContextVisible(this->treasure_) &&
            this->treasure_.ShouldRender({}) && !this->ControllerNativeBusy() && (pointer || !this->ControllerNativeMenuOpen()) &&
            !this->chat_.HistoryExpanded() && !this->chat_second_.HistoryExpanded();
    }

    auto hud_runtime::UpdateTreasure(const uint64_t now) -> void
    {
        this->treasure_source_.Update(this->core_, now);
        this->treasure_.SetFocusStyle(this->general_.FocusStyle());
        this->treasure_.Update(this->core_, this->TreasureInputAvailable(true));
        const auto selected = !this->editing_ && this->ContextVisible(this->treasure_) ? this->treasure_.MemberSelection() : std::nullopt;
        for (auto* party : {&this->party_, &this->alliance_first_, &this->alliance_second_})
            party->SetTreasureSelection(selected ? &this->treasure_source_ : nullptr, selected ? selected->slot : 0, selected ? selected->generation : 0);
        if (this->treasure_.TakeSettingsChanged()) this->SaveSettings();
    }

    auto hud_runtime::HandleTreasureKey(const WPARAM key, const LPARAM flags, const bool blocked) -> bool
    {
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (key < this->treasure_key_release_.size() && this->treasure_key_release_[key])
        {
            if (released) this->treasure_key_release_[key] = false;
            if (released || !this->treasure_.InputFocused()) return true;
        }
        if (!this->treasure_.InputFocused()) return false;
        if (blocked || !this->TreasureInputAvailable())
        {
            this->ReleaseControllerFocus();
            return false;
        }
        const auto action = key == VK_UP ? controller_action::up : key == VK_DOWN ? controller_action::down :
            key == VK_LEFT ? controller_action::left : key == VK_RIGHT ? controller_action::right :
            key == VK_RETURN ? controller_action::confirm : key == VK_ESCAPE ? controller_action::cancel : controller_action::none;
        if (action == controller_action::none) return false;
        if (!released)
        {
            this->treasure_key_release_[key] = true;
            this->treasure_.HandleControllerAction(action, (static_cast<uint32_t>(flags) & 0x40000000u) != 0);
            this->UpdateControllerFocus();
        }
        return true;
    }

    auto hud_runtime::HandleTreasureMouse(const uint32_t message, const float x, const float y, const bool inputBlocked) -> bool
    {
        const bool blocked = this->PointerInputBlocked(inputBlocked);
        const bool owned = this->treasure_.PointerDown();
        const bool available = !blocked && this->TreasureInputAvailable(true);
        if (!owned && (!available || !this->treasure_.Contains(x, y))) return false;
        if (!available)
        {
            this->treasure_.EndInput();
            if (message != WM_LBUTTONUP) return true;
        }
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) this->ReleaseControllerFocus();
        if (!this->treasure_.HandleMouse(message, x, y)) return owned;
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
        {
            this->chat_.Focus(false);
            auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
            const HWND window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
            if (window != nullptr)
            {
                ::SetCapture(window);
                if (::GetCapture() == window) this->mouse_capture_window_ = window;
            }
        }
        if (message == WM_LBUTTONUP) this->ReleasePointerCapture();
        this->SetChatMouseCapture(true);
        return true;
    }
}
