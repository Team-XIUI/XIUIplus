#include "modules/chat/chat_module.hpp"
#include "modules/party/party_module.hpp"
#include "modules/enemylist/enemy_list_module.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
    class controller_chat_manager final : public IChatManager
    {
    public:
        size_t submissions{};
        std::string command;
        void ParseCommand(int32_t, const char*) override {}
        void QueueCommand(int32_t, const char* text) override { ++this->submissions; this->command = text; }
        void Write(int32_t, bool, const char*) override {}
        void Writef(int32_t, bool, const char*, ...) override {}
        void AddChatMessage(int32_t, bool, const char*) override {}
        int32_t ParseAutoTranslate(const char*, char*, int32_t, bool) const override { return 0; }
        void ExecuteScript(const char*, const char*, bool) override {}
        void ExecuteScriptString(const char*, const char*, bool) override {}
        const char* GetInputTextRaw() const override { return ""; }
        void SetInputTextRaw(const char*, uint32_t) const override {}
        uint32_t GetInputTextRawLength() const override { return 0; }
        uint32_t GetInputTextRawCaretPosition() const override { return 0; }
        const char* GetInputTextParsed() const override { return ""; }
        void SetInputTextParsed(const char*) const override {}
        uint32_t GetInputTextParsedLength() const override { return 0; }
        uint32_t GetInputTextParsedLengthMax() const override { return 255; }
        const char* GetInputTextDisplay() const override { return ""; }
        void SetInputTextDisplay(const char*) const override {}
        void SetInputText(const char*) const override {}
        uint8_t IsInputOpen() const override { return 0; }
        bool GetSilentAliases() const override { return false; }
        void SetSilentAliases(bool) override {}
    };

    auto Check(const bool result, const char* description) -> bool
    {
        if (!result) std::fprintf(stderr, "Controller modules: %s\n", description);
        return result;
    }
}

namespace xiui
{
    struct controller_module_test
    {
        static auto Chat(void) -> bool
        {
            ui::design_system design;
            chat_module chat, second{true};
            chat.AttachSecondWindow(second);
            chat.ApplyTextDefaults(design.GetTheme()); second.ApplyTextDefaults(design.GetTheme());
            second.State().enabled = true;
            chat.editor_.Set("unsent draft");
            chat.channel_ = 1;
            const auto action = [&](const controller_action value, const bool repeated = false) { return chat.HandleControllerAction(value, repeated, design); };
            if (!Check(chat.BeginControllerHistory() && chat.HistoryFocused() && !chat.Focused(), "history entry has separate focus")) return false;
            if (!Check(!chat.HandleHistoryKey('W', 0, false, true, design) && !chat.HandleHistoryKey('W', 0x80000000u, false, true, design) && chat.HistoryFocused(),
                "unrelated movement keys pass through history without losing focus")) return false;
            if (!Check(action(controller_action::confirm, true) && !chat.HistoryExpanded() && chat.editor_.Text() == "unsent draft", "held confirmation neither expands nor sends")) return false;
            if (!Check(action(controller_action::confirm) && chat.HistoryExpanded(), "fresh confirmation expands history")) return false;
            const auto channel = chat.channel_;
            const auto initialTab = chat.history_view_.tab;
            if (!Check(action(controller_action::cycle_window, true) && chat.history_view_.tab == initialTab,
                "held cycle does not skip chat tabs")) return false;
            for (size_t tab = 0; tab < std::size(chat_history_tabs); ++tab)
                if (!Check(action(controller_action::cycle_window) && chat.history_view_.tab == (tab + 1) % std::size(chat_history_tabs) && chat.channel_ == channel,
                    "history tabs wrap without changing send channel")) return false;
            chat.SetHistoryViewport({0, 0, 480, 320});
            for (uint64_t sequence = 1; sequence <= 40; ++sequence)
            {
                chat_event event{}; event.sequence = sequence; event.windows = sequence == 40 ? 2 : 1; event.mode = 9;
                const auto text = std::string{"line "} + std::to_string(sequence) + " " + std::string(140, 'a');
                strcpy_s(event.text, text.c_str()); chat.Receive(nullptr, event, false);
            }
            action(controller_action::up);
            if (!Check(chat.CurrentScroll() > 1 && chat.rows_[chat.rows_.size() - chat.CurrentScroll() - 1].sequence == 38,
                "up moves a complete wrapped message")) return false;
            const auto scroll = chat.CurrentScroll();
            action(controller_action::next_tab); action(controller_action::previous_tab);
            action(controller_action::none);
            if (!Check(chat.CurrentScroll() == scroll, "each history tab retains its position")) return false;
            if (!Check(action(controller_action::cancel) && chat.HistoryFocused() && !chat.HistoryExpanded() &&
                action(controller_action::cancel) && !chat.HistoryFocused() && chat.editor_.Text() == "unsent draft", "cancel collapses then leaves without changing draft")) return false;
            if (!Check(second.BeginControllerHistory() && !chat.HistoryFocused() && second.HandleControllerAction(controller_action::confirm, false, design), "second enabled window receives history focus")) return false;
            second.HandleControllerAction(controller_action::down, false, design);
            if (!Check(!second.rows_.empty() && std::all_of(second.rows_.begin(), second.rows_.end(), [](const auto& row) { return row.sequence == 40; }),
                "second window history respects its native window filter")) return false;
            second.HandleControllerAction(controller_action::actions, false, design);
            if (!Check(chat.ControllerComposing() && !second.HistoryFocused() && chat.editor_.Text() == "unsent draft" && chat.controller_compose_action_ == 0,
                "compose preserves draft and initially focuses Channel")) return false;
            action(controller_action::cancel);
            if (!Check(second.HistoryFocused() && !chat.Focused() && chat.editor_.Text() == "unsent draft", "compose cancel returns to original history window")) return false;

            controller_chat_manager manager;
            chat.BeginControllerCompose();
            chat.HandleControllerAction(controller_action::confirm, false, design, &manager);
            if (!Check(manager.submissions == 0 && chat.controller_channel_adjusting_, "initial Confirm adjusts channel without sending")) return false;
            chat.HandleControllerAction(controller_action::cancel, false, design, &manager);
            chat.HandleControllerAction(controller_action::right, false, design, &manager);
            chat.HandleControllerAction(controller_action::confirm, true, design, &manager);
            if (!Check(manager.submissions == 0 && chat.Focused(), "held Confirm cannot submit Send")) return false;
            chat.HandleControllerAction(controller_action::confirm, false, design, &manager);
            if (!Check(manager.submissions == 1 && manager.command == "/party unsent draft" && !chat.Focused(), "fresh Send uses the validated existing submission path once")) return false;
            chat.HandleControllerAction(controller_action::confirm, true, design, &manager);
            return Check(manager.submissions == 1, "released composition cannot send twice");
        }

        static auto Party(void) -> bool
        {
            party_source source; source.player_id_ = 100;
            for (size_t slot = 0; slot < 3; ++slot)
            {
                auto& member = source.members_[slot]; member.active = true; member.id = 100 + static_cast<uint32_t>(slot);
                member.slot = static_cast<uint32_t>(slot); member.index = 200 + static_cast<uint32_t>(slot); member.in_zone = member.entity_valid = slot != 2;
            }
            source.members_[0].selected = true;
            bool enabled = true;
            party_module party; party.SetSource(&source); party.SetModuleEnabled(&enabled);
            if (!Check(party.BeginControllerBrowse() && party.ControllerMember()->id == 100, "party browse starts at current selection")) return false;
            if (!Check(party.MoveControllerBrowse(1) && party.ControllerMember()->id == 101 && source.members_[0].selected && !source.members_[1].selected,
                "browsing never changes native selection")) return false;
            std::swap(source.members_[0], source.members_[1]); source.members_[0].slot = 0; source.members_[1].slot = 1;
            if (!Check(party.ControllerMember()->id == 101 && party.ControllerMember()->slot == 0, "member ID survives row reorder")) return false;
            party.MoveControllerBrowse(1); party.MoveControllerBrowse(1);
            const auto remote = party.ControllerMember();
            if (!Check(remote && remote->id == 102 && !remote->in_zone && !remote->entity_valid && party.ControllerBounds().height > 0,
                "remote members remain browseable without target eligibility")) return false;
            if (!Check(party.MoveControllerBrowse(1) && party.ControllerMember()->id == 101 &&
                party.MoveControllerBrowse(-1) && party.ControllerMember()->id == 102, "party cycling wraps through the current row order")) return false;
            source.members_[2].active = false;
            if (!Check(!party.ControllerMember() && !party.ControllerFocused(), "departed member cannot activate a replacement row")) return false;
            party.BeginControllerBrowse(); enabled = false;
            if (!Check(!party.ControllerMember() && !party.ControllerFocused(), "disabled party releases browsing")) return false;
            enabled = true;
            party_module alliance{1}; alliance.SetSource(&source); alliance.State().enabled = false;
            return Check(!alliance.BeginControllerBrowse(), "disabled alliance cannot receive focus");
        }

        static auto Enemies(void) -> bool
        {
            enemy_list_module enemies;
            enemies.ApplyTextDefaults(ui::theme::CreateDefault());
            for (uint32_t row = 0; row < 3; ++row)
            {
                enemy_entry entry; entry.id = 1000 + row; entry.index = 200 + row; entry.selected = row == 1;
                enemies.source_.entries_.push_back(std::move(entry));
            }
            if (!Check(enemies.BeginControllerBrowse() && enemies.ControllerMember()->id == 1001, "enemy browse starts at visible native selection")) return false;
            if (!Check(enemies.MoveControllerBrowse(-1) && enemies.ControllerMember()->id == 1000 &&
                enemies.source_.entries_[1].selected && !enemies.source_.entries_[0].selected,
                "enemy browse never changes native targeting")) return false;
            std::swap(enemies.source_.entries_[0], enemies.source_.entries_[1]);
            if (!Check(enemies.ControllerMember()->id == 1000 && enemies.ControllerMember()->index == 200 && enemies.ControllerBounds().height > 0,
                "enemy identity survives card reorder")) return false;
            enemies.source_.entries_[1].id = 3000;
            if (!Check(!enemies.ControllerMember() && !enemies.ControllerFocused(), "reused entity index cannot activate a replacement enemy")) return false;
            if (!Check(enemies.BeginControllerBrowse(1002, 202), "explicit visible enemy identity can receive focus")) return false;
            enemies.max_rows_ = 1;
            if (!Check(!enemies.ControllerMember() && !enemies.ControllerFocused(), "card hidden by visible row limit loses focus")) return false;
            enemies.max_rows_ = 3;
            enemies.BeginControllerBrowse(); enemies.State().enabled = false;
            if (!Check(!enemies.ControllerMember() && !enemies.ControllerFocused(), "disabled enemy list releases browsing")) return false;
            enemies.State().enabled = true;
            enemies.BeginControllerBrowse(); enemies.source_.entries_.clear();
            return Check(!enemies.ControllerMember() && !enemies.BeginControllerBrowse(), "empty enemy list cannot retain or acquire focus");
        }
    };

    auto RunControllerModuleChecks(void) -> bool
    { return controller_module_test::Chat() && controller_module_test::Party() && controller_module_test::Enemies(); }
}
