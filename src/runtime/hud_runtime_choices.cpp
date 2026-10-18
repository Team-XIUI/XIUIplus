#include "hud_runtime.hpp"

namespace xiui
{
    auto hud_runtime::CloseChoiceInput(void) -> void
    {
        this->choice_input_.Cancel();
        this->choice_input_module_ = nullptr;
    }

    auto hud_runtime::SelectChoiceInput(const int selected) -> void
    {
        auto* module = this->choice_input_module_;
        const auto index = this->choice_input_option_index_;
        if (module != nullptr && module == this->active_module_ && index < module->OptionCount())
        {
            const auto option = module->Option(index);
            if (option.available && option.choice != nullptr && option.choice == this->choice_input_.Option().choice &&
                selected >= 0 && static_cast<size_t>(selected) < option.choice_count && *option.choice != selected)
            {
                *option.choice = selected;
                module->OnOptionChanged(index);
                this->SaveSettings();
            }
        }
        this->CloseChoiceInput();
    }

    auto hud_runtime::HandleChoiceKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if (!this->choice_input_.Opened()) return false;
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        if (key == VK_ESCAPE || key == VK_TAB) this->CloseChoiceInput();
        else if (key == VK_SPACE && (static_cast<uint32_t>(flags) & 0x40000000u) == 0)
            this->PreviewChoiceInput(this->choice_input_.Highlighted());
        else if (key == VK_RETURN && (static_cast<uint32_t>(flags) & 0x40000000u) == 0)
        {
            this->ui_sounds_.Select(::GetTickCount64());
            this->SelectChoiceInput(this->choice_input_.Highlighted());
        }
        else if (key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END)
        {
            this->choice_input_.Move(key == VK_UP ? -1 : key == VK_DOWN ? 1 : key == VK_HOME ? -4096 : 4096);
            this->ui_sounds_.Play(ui::native_ui_sound::hover);
        }
        return true;
    }

    auto hud_runtime::PreviewChoiceInput(const int selected) -> bool
    {
        auto* module = this->choice_input_module_;
        const auto index = this->choice_input_option_index_;
        if (!this->choice_input_.Opened() || module == nullptr || module != this->active_module_ || index >= module->OptionCount() ||
            !this->MatchesConfigOption(index)) return false;
        const auto option = module->Option(index);
        if (!option.available || !option.choice_preview || option.choice != this->choice_input_.Option().choice ||
            selected < 0 || static_cast<size_t>(selected) < option.preview_first || static_cast<size_t>(selected) >= option.choice_count) return false;
        this->choice_input_.Move(selected - this->choice_input_.Highlighted());
        return module->PreviewChoice(index, static_cast<size_t>(selected));
    }

    auto hud_runtime::ProcessChoiceClick(const D3DVIEWPORT8& viewport) -> bool
    {
        if (!this->choice_input_.Opened()) return false;
        const auto index = this->choice_input_.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
        if (index && this->choice_input_.PreviewAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport))
        {
            this->PreviewChoiceInput(*index);
            return true;
        }
        if (index) this->SelectChoiceInput(*index);
        else this->CloseChoiceInput();
        return true;
    }

    auto hud_runtime::ChoiceSoundTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        if (!this->choice_input_.Opened()) return 0;
        const auto index = this->choice_input_.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
        const bool preview = this->choice_input_.PreviewAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
        return index ? 0xE000000000000000ULL | (static_cast<uint64_t>(*index + 1) << 1) | (preview ? 1 : 0) : 0;
    }

    auto hud_runtime::DrawChoiceInput(const D3DVIEWPORT8& viewport) -> void
    {
        this->config_controls_.ChoiceMenu(this->choice_input_, viewport, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
    }
}
