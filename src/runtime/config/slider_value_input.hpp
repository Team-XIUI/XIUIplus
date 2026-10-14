#pragma once

#include "chat/chat_editor.hpp"
#include "modules/module.hpp"

#include <optional>

namespace xiui::ui
{
    class slider_value_input
    {
    public:
        auto Open(const module_option& option) -> void;
        auto Cancel(void) -> void { this->focused_ = this->invalid_ = false; }
        auto Focused(void) const -> bool { return this->focused_; }
        auto Invalid(void) const -> bool { return this->invalid_; }
        auto Option(void) const -> const module_option& { return this->option_; }
        auto Editor(void) const -> const chat_editor& { return this->editor_; }
        auto SelectAll(void) -> void { this->editor_.SelectAll(); }
        auto Insert(std::string_view text) -> void;
        auto HandleKey(WPARAM key, LPARAM flags, HWND window) -> void;
        auto Value(void) -> std::optional<float>;
        static auto Format(float value, float step) -> std::string;
        static auto Normalize(float value, float minimum, float maximum, float step) -> float;

    private:
        module_option option_{};
        chat_editor editor_{};
        bool focused_{}, invalid_{};
        auto Copy(HWND window) const -> bool;
        auto Paste(HWND window) -> void;
    };
}
