#pragma once

#include "chat_data.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <functional>

namespace xiui
{
    enum class chat_completion_kind : uint8_t { text, command, player, auto_translate };
    struct chat_completion
    {
        std::string label, value;
        chat_completion_kind kind{chat_completion_kind::text};
    };

    class chat_editor
    {
    public:
        auto SetConstraint(std::function<bool(std::string_view)> constraint) -> void { this->constraint_ = std::move(constraint); }
        auto Text(void) const -> const std::string& { return this->text_; }
        auto Caret(void) const -> size_t { return this->caret_; }
        auto Selection(void) const -> std::pair<size_t, size_t>;
        auto Set(std::string text, size_t caret = std::string::npos) -> void;
        auto Insert(std::string_view text) -> void;
        auto Move(int direction, bool select, bool word = false) -> void;
        auto Home(bool end, bool select) -> void;
        auto Erase(bool backward, bool word = false) -> void;
        auto SelectAll(void) -> void;
        auto SelectedText(void) const -> std::string;
        auto Remember(void) -> void;
        auto History(int direction) -> void;
        auto RestartHistory(void) -> void { this->history_index_ = this->history_.size(); this->history_draft_.clear(); }
        auto Complete(const std::vector<std::string>& candidates, bool reverse) -> void;
        auto CompleteCandidates(const std::vector<chat_completion>& candidates, bool reverse) -> void;
        auto SelectCompletion(size_t index) -> void;
        auto AcceptCompletion(void) -> void { this->ResetCompletion(); }
        auto CancelCompletion(void) -> void;
        auto ResetCompletion(void) -> void;
        auto Completions(void) const -> const std::vector<chat_completion>& { return this->matches_; }
        auto CompletionIndex(void) const -> size_t { return this->match_index_; }
        auto CompletionPrefix(void) const -> std::string;

    private:
        std::function<bool(std::string_view)> constraint_{};
        auto Fits(std::string_view text) const -> bool { return text.size() < chat_draft_capacity && (!this->constraint_ || this->constraint_(text)); }
        std::string text_;
        size_t caret_{}, anchor_{};
        std::vector<std::string> history_;
        size_t history_index_{};
        std::string history_draft_;
        std::vector<chat_completion> matches_;
        size_t match_index_{};
        std::string completion_origin_;
        size_t completion_start_{}, completion_end_{}, completion_caret_{};
        bool completion_quote_{};

        auto EraseSelection(void) -> bool;
        auto Previous(size_t position) const -> size_t;
        auto Next(size_t position) const -> size_t;
    };
} // namespace xiui
