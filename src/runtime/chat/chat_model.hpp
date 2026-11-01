#pragma once

#include "chat_data.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace xiui
{
    struct chat_type { const char* key; const char* label; };
    inline constexpr chat_type chat_types[]{
        {"incoming_tell", "Incoming tells"}, {"outgoing_tell", "Outgoing tells"},
        {"say", "Say"}, {"party", "Party / alliance"}, {"linkshell", "Linkshell"}, {"linkshell2", "Linkshell 2"},
        {"shout", "Shout"}, {"yell", "Yell"}, {"unity", "Unity"}, {"emote", "Emotes"},
        {"other_chat", "Other chat"}, {"combat", "Combat"}, {"system", "System"}};
    auto ChatTypeIndex(uint32_t mode) -> size_t;

    enum class chat_tab : uint8_t { all, chat, party, linkshell, tell, combat, system, window1, window2, count };
    inline constexpr std::array<const char*, static_cast<size_t>(chat_tab::count)> chat_tab_names{
        "All", "Chat", "Party", "Linkshell", "Tell", "Combat", "System", "Window 1", "Window 2"};

    struct chat_span
    {
        std::string text;
        int color_index{-1};
        bool emphasis{};
    };

    struct chat_line
    {
        uint64_t sequence{}, timestamp{};
        uint32_t mode{}, color{}, windows{};
        std::vector<chat_span> spans;
        bool dialogue_prompt{};
    };

    auto ChatCategory(uint32_t mode) -> chat_tab;
    auto ChatTabMatches(chat_tab tab, uint32_t mode, uint32_t windows) -> bool;
    auto ParseChatText(std::string_view text) -> std::vector<chat_span>;

    class chat_model
    {
    public:
        auto Add(const chat_event& event, std::string_view expanded) -> void;
        auto Clear(void) -> void { this->lines_.clear(); this->dialogue_prompt_sequence_ = 0; ++this->revision_; }
        auto DialoguePromptSequence(void) const -> uint64_t { return this->dialogue_prompt_sequence_; }
        auto DismissDialoguePrompt(void) -> void { this->dialogue_prompt_sequence_ = 0; }
        auto Lines(void) const -> const std::deque<chat_line>& { return this->lines_; }
        auto Revision(void) const -> uint64_t { return this->revision_; }

    private:
        std::deque<chat_line> lines_;
        uint64_t revision_{};
        uint64_t dialogue_prompt_sequence_{};
    };
} // namespace xiui
