#include "hud_runtime.hpp"

namespace xiui
{
    auto hud_runtime::CloseChoiceInput(void) -> void
    {
        this->config_.choice_input.Cancel();
        this->config_.choice_module = nullptr;
    }

    auto hud_runtime::SelectChoiceInput(const int selected) -> void
    {
        auto* module = this->config_.choice_module;
        const auto index = this->config_.choice_option;
        if (module != nullptr && module == this->config_.module && index < module->OptionCount())
        {
            const auto option = module->Option(index);
            if (option.available && option.choice != nullptr && option.choice == this->config_.choice_input.Option().choice &&
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
        if (!this->config_.choice_input.Opened()) return false;
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        if (key == VK_ESCAPE || key == VK_TAB) this->CloseChoiceInput();
        else if (key == VK_SPACE && (static_cast<uint32_t>(flags) & 0x40000000u) == 0)
            this->PreviewChoiceInput(this->config_.choice_input.Highlighted());
        else if (key == VK_RETURN && (static_cast<uint32_t>(flags) & 0x40000000u) == 0)
        {
            this->ui_sounds_.Select(::GetTickCount64());
            this->SelectChoiceInput(this->config_.choice_input.Highlighted());
        }
        else if (key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END)
        {
            this->config_.choice_input.Move(key == VK_UP ? -1 : key == VK_DOWN ? 1 : key == VK_HOME ? -4096 : 4096);
            this->ui_sounds_.Play(ui::native_ui_sound::hover);
        }
        return true;
    }

    auto hud_runtime::PreviewChoiceInput(const int selected) -> bool
    {
        auto* module = this->config_.choice_module;
        const auto index = this->config_.choice_option;
        if (!this->config_.choice_input.Opened() || module == nullptr || module != this->config_.module || index >= module->OptionCount() ||
            !this->MatchesConfigOption(index)) return false;
        const auto option = module->Option(index);
        if (!option.available || !option.choice_preview || option.choice != this->config_.choice_input.Option().choice ||
            selected < 0 || static_cast<size_t>(selected) < option.preview_first || static_cast<size_t>(selected) >= option.choice_count) return false;
        this->config_.choice_input.Move(selected - this->config_.choice_input.Highlighted());
        return module->PreviewChoice(index, static_cast<size_t>(selected));
    }

    auto hud_runtime::ProcessChoiceClick(const D3DVIEWPORT8& viewport) -> bool
    {
        if (!this->config_.choice_input.Opened()) return false;
        const auto index = this->config_.choice_input.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
        if (index && this->config_.choice_input.PreviewAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport))
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
        if (!this->config_.choice_input.Opened()) return 0;
        const auto index = this->config_.choice_input.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
        const bool preview = this->config_.choice_input.PreviewAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
        return index ? 0xE000000000000000ULL | (static_cast<uint64_t>(*index + 1) << 1) | (preview ? 1 : 0) : 0;
    }

    auto hud_runtime::DrawChoiceInput(const D3DVIEWPORT8& viewport) -> void
    {
        this->config_controls_.ChoiceMenu(this->config_.choice_input, viewport, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
    }
}
