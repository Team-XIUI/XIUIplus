#pragma once

#include "Ashita.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xiui
{
    struct chat_draft { std::string text; size_t caret{}; };

    auto IsTellRecipient(std::string_view name) -> bool;
    auto IsTellDraft(std::string_view text) -> bool;

    class native_chat_input
    {
    public:
        auto Initialize(IAshitaCore* core) -> void;
        auto LastTeller(void) const -> std::string;
        auto ReplyTargets(void) const -> std::vector<std::string>;
        auto SendPrefix(void) const -> std::optional<std::string>;
        auto TakeTell(IChatManager* chat) const -> std::optional<chat_draft>;
        auto TakeCommand(IChatManager* chat) const -> std::optional<chat_draft>;
        auto TakeInput(IChatManager* chat) const -> std::optional<chat_draft>;
        auto Recover(IChatManager* chat) const -> bool;
        auto ReplyAvailable(void) const -> bool { return this->last_teller_ != 0; }
        auto HandoffAvailable(void) const -> bool { return this->raw_offset_ > 0 && this->reset_chat_ != 0 && this->close_menu_ != 0 && this->menu_manager_ != 0; }

    private:
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
        auto TakeDraft(IChatManager* chat, bool tellOnly, bool allowText = false) const -> std::optional<chat_draft>;
        uintptr_t last_teller_{}, reset_chat_{};
        uintptr_t reply_teller_{}, reply_manager_{};
        uintptr_t close_menu_{}, menu_manager_{};
        uintptr_t send_prefix_slot_{};
        uint32_t raw_offset_{}, state_offset_{};
    };
}
