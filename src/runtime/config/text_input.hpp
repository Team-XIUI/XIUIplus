#pragma once

#include "chat/chat_editor.hpp"
#include "Ashita.h"

namespace xiui::ui
{
    class text_input
    {
    public:
        auto Open(std::string value = {}, size_t limit = 64) -> void { this->editor_.Set(std::move(value)); this->editor_.SelectAll(); this->limit_ = limit; this->focused_ = true; }
        auto Cancel(void) -> void { this->focused_ = false; }
        auto Focused(void) const -> bool { return this->focused_; }
        auto Editor(void) const -> const chat_editor& { return this->editor_; }
        auto SelectAll(void) -> void { this->editor_.SelectAll(); }
        auto Insert(std::string_view text) -> void;
        auto HandleKey(WPARAM key, LPARAM flags, HWND window) -> void;

    private:
        chat_editor editor_{};
        bool focused_{};
        size_t limit_{64};
        auto Copy(HWND window) const -> bool;
        auto Paste(HWND window) -> void;
    };
}
