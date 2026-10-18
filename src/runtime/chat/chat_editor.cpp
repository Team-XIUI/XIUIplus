#include "chat_editor.hpp"
#include "chat_token.hpp"

#include <algorithm>
#include <cctype>
#include <utility>
#include <unordered_set>

namespace xiui
{
    auto chat_editor::Previous(size_t position) const -> size_t
    {
        if (position == 0) return 0;
        if (position >= 6 && ChatTokenSize(this->text_, position - 6) != 0) return position - 6;
        --position;
        while (position > 0 && (static_cast<uint8_t>(this->text_[position]) & 0xC0) == 0x80) --position;
        return position;
    }

    auto chat_editor::Next(size_t position) const -> size_t
    {
        if (position >= this->text_.size()) return this->text_.size();
        if (const auto token = ChatTokenSize(this->text_, position); token != 0) return position + token;
        ++position;
        while (position < this->text_.size() && (static_cast<uint8_t>(this->text_[position]) & 0xC0) == 0x80) ++position;
        return position;
    }

    auto chat_editor::Selection(void) const -> std::pair<size_t, size_t>
    {
        return std::minmax(this->caret_, this->anchor_);
    }

    auto chat_editor::Set(std::string text, const size_t caret) -> void
    {
        this->text_ = std::move(text);
        if (this->text_.size() >= chat_draft_capacity)
        {
            size_t end{};
            while (end < this->text_.size())
            {
                const auto next = this->Next(end);
                if (next >= chat_draft_capacity) break;
                end = next;
            }
            this->text_.resize(end);
        }
        this->caret_ = std::min(caret, this->text_.size());
        while (this->caret_ < this->text_.size() && this->caret_ > 0 &&
            (static_cast<uint8_t>(this->text_[this->caret_]) & 0xC0) == 0x80) --this->caret_;
        for (size_t at = this->caret_ > 5 ? this->caret_ - 5 : 0; at < this->caret_; ++at)
            if (ChatTokenSize(this->text_, at) > this->caret_ - at) { this->caret_ = at; break; }
        this->anchor_ = this->caret_;
        this->ResetCompletion();
    }

    auto chat_editor::EraseSelection(void) -> bool
    {
        const auto [begin, end] = this->Selection();
        if (begin == end) return false;
        this->text_.erase(begin, end - begin);
        this->caret_ = this->anchor_ = begin;
        return true;
    }

    auto chat_editor::Insert(const std::string_view text) -> void
    {
        std::string clean;
        for (size_t i = 0; i < text.size();)
        {
            if (const auto token = ChatTokenSize(text, i); token != 0) { clean.append(text.substr(i, token)); i += token; }
            else { const auto c = text[i++]; if (static_cast<uint8_t>(c) >= 0x20 && c != 0x7F) clean += c; }
        }
        const auto [begin, end] = this->Selection();
        auto candidate = this->text_;
        candidate.erase(begin, end - begin);
        size_t accepted{};
        while (accepted < clean.size())
        {
            size_t next = accepted + ChatTokenSize(clean, accepted);
            if (next == accepted)
            {
                next = accepted + 1;
                while (next < clean.size() && (static_cast<uint8_t>(clean[next]) & 0xC0) == 0x80) ++next;
            }
            candidate.insert(begin + accepted, clean.substr(accepted, next - accepted));
            if (!this->Fits(candidate)) { candidate.erase(begin + accepted, next - accepted); break; }
            accepted = next;
        }
        if (accepted == 0) return;
        this->text_ = std::move(candidate);
        this->caret_ = this->anchor_ = begin + accepted;
        this->history_index_ = this->history_.size();
        this->ResetCompletion();
    }

    auto chat_editor::Move(const int direction, const bool select, const bool word) -> void
    {
        if (!select && this->caret_ != this->anchor_)
        {
            const auto selection = this->Selection();
            this->caret_ = direction < 0 ? selection.first : selection.second;
        }
        else if (direction < 0)
        {
            do { this->caret_ = this->Previous(this->caret_); }
            while (word && this->caret_ > 0 && !std::isspace(static_cast<unsigned char>(this->text_[this->caret_ - 1])));
        }
        else
        {
            do { this->caret_ = this->Next(this->caret_); }
            while (word && this->caret_ < this->text_.size() && !std::isspace(static_cast<unsigned char>(this->text_[this->caret_])));
        }
        if (!select) this->anchor_ = this->caret_;
        this->ResetCompletion();
    }

    auto chat_editor::Home(const bool end, const bool select) -> void
    {
        this->caret_ = end ? this->text_.size() : 0;
        if (!select) this->anchor_ = this->caret_;
        this->ResetCompletion();
    }

    auto chat_editor::Erase(const bool backward, const bool word) -> void
    {
        if (!this->EraseSelection())
        {
            this->anchor_ = this->caret_;
            this->Move(backward ? -1 : 1, true, word);
            this->EraseSelection();
        }
        this->ResetCompletion();
    }

    auto chat_editor::SelectAll(void) -> void
    {
        this->anchor_ = 0;
        this->caret_ = this->text_.size();
        this->ResetCompletion();
    }

    auto chat_editor::SelectedText(void) const -> std::string
    {
        const auto [begin, end] = this->Selection();
        return this->text_.substr(begin, end - begin);
    }

    auto chat_editor::Remember(void) -> void
    {
        if (!this->text_.empty() && (this->history_.empty() || this->history_.back() != this->text_))
            this->history_.push_back(this->text_);
        if (this->history_.size() > 64) this->history_.erase(this->history_.begin());
        this->history_index_ = this->history_.size();
        this->history_draft_.clear();
    }

    auto chat_editor::History(const int direction) -> void
    {
        if (this->history_.empty()) return;
        if (this->history_index_ == this->history_.size()) this->history_draft_ = this->text_;
        if (direction < 0 && this->history_index_ > 0) --this->history_index_;
        if (direction > 0 && this->history_index_ < this->history_.size()) ++this->history_index_;
        this->Set(this->history_index_ < this->history_.size() ? this->history_[this->history_index_] : this->history_draft_);
    }

    auto chat_editor::CompletionPrefix(void) const -> std::string
    {
        if (!this->matches_.empty()) return this->completion_origin_.substr(this->completion_start_, this->completion_caret_ - this->completion_start_);
        size_t begin{};
        bool quoted{};
        for (size_t at = 0; at < this->caret_;)
        {
            if (const auto token = ChatTokenSize(this->text_, at); token != 0) { at += token; begin = at; continue; }
            const auto c = this->text_[at++];
            if (c == '"') { quoted = !quoted; begin = at; }
            else if (!quoted && std::isspace(static_cast<unsigned char>(c))) begin = at;
        }
        return this->text_.substr(begin, this->caret_ - begin);
    }

    auto chat_editor::Complete(const std::vector<std::string>& candidates, const bool reverse) -> void
    {
        std::vector<chat_completion> values;
        for (const auto& candidate : candidates) values.push_back({candidate, candidate});
        this->CompleteCandidates(values, reverse);
    }

    auto chat_editor::CompleteCandidates(const std::vector<chat_completion>& candidates, const bool reverse) -> void
    {
        if (this->matches_.empty())
        {
            const auto prefix = this->CompletionPrefix();
            this->completion_origin_ = this->text_;
            this->completion_start_ = this->caret_ - prefix.size();
            this->completion_end_ = this->caret_;
            this->completion_caret_ = this->caret_;
            this->completion_quote_ = this->completion_start_ > 0 && this->text_[this->completion_start_ - 1] == '"';
            while (this->completion_end_ < this->text_.size() && this->text_[this->completion_end_] != '"' &&
                (this->completion_quote_ || !std::isspace(static_cast<unsigned char>(this->text_[this->completion_end_])))) ++this->completion_end_;
            if (this->completion_quote_ && this->completion_end_ < this->text_.size() && this->text_[this->completion_end_] == '"') ++this->completion_end_;
            std::unordered_set<std::string> seen;
            for (const auto& candidate : candidates)
            {
                if (candidate.label.size() < prefix.size()) continue;
                bool matches = true;
                for (size_t i = 0; i < prefix.size(); ++i)
                    if (std::tolower(static_cast<unsigned char>(prefix[i])) != std::tolower(static_cast<unsigned char>(candidate.label[i]))) { matches = false; break; }
                if (matches && seen.insert(candidate.value).second) this->matches_.push_back(candidate);
            }
            if (this->matches_.empty()) return;
            std::sort(this->matches_.begin(), this->matches_.end(), [](const auto& a, const auto& b) {
                if (a.label != b.label) return a.label < b.label;
                if ((a.kind == chat_completion_kind::auto_translate) != (b.kind == chat_completion_kind::auto_translate)) return a.kind == chat_completion_kind::auto_translate;
                if (a.kind != b.kind) return a.kind < b.kind;
                return a.value < b.value;
            });
            this->match_index_ = reverse ? this->matches_.size() - 1 : 0;
        }
        else
            this->match_index_ = (this->match_index_ + (reverse ? this->matches_.size() - 1 : 1)) % this->matches_.size();
        this->SelectCompletion(this->match_index_);
    }

    auto chat_editor::SelectCompletion(const size_t index) -> void
    {
        if (index >= this->matches_.size()) return;
        this->match_index_ = index;
        const auto& match = this->matches_[index];
        auto replacement = match.value;
        if (this->completion_quote_) replacement += '"';
        if (!this->completion_quote_ && match.kind != chat_completion_kind::auto_translate && replacement.find(' ') != std::string::npos) replacement = '"' + replacement + '"';
        if (this->completion_origin_.size() - (this->completion_end_ - this->completion_start_) + replacement.size() >= chat_draft_capacity) return;
        auto candidate = this->completion_origin_;
        candidate.replace(this->completion_start_, this->completion_end_ - this->completion_start_, replacement);
        if (!this->Fits(candidate)) return;
        this->text_ = std::move(candidate);
        this->caret_ = this->anchor_ = this->completion_start_ + replacement.size();
    }

    auto chat_editor::CancelCompletion(void) -> void
    {
        if (this->matches_.empty()) return;
        auto origin = this->completion_origin_;
        const auto caret = this->completion_caret_;
        this->Set(std::move(origin), caret);
    }

    auto chat_editor::ResetCompletion(void) -> void
    {
        this->matches_.clear();
        this->completion_origin_.clear();
    }
} // namespace xiui
