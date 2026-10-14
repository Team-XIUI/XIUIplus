#include "native_chat_input.hpp"
#include "chat_encoding.hpp"
#include "Memory.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace
{
    auto ReadText(const char* source, char* destination, const size_t capacity) noexcept -> bool
    {
        if (source == nullptr) return false;
        __try
        {
            for (size_t i = 0; i < capacity; ++i)
            {
                destination[i] = source[i];
                if (source[i] == '\0') return true;
            }
            return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ReadLastTeller(const uintptr_t function, char* name, const size_t capacity) noexcept -> bool
    {
        using last_teller_fn = const char*(__stdcall*)(void);
        __try { return ReadText(reinterpret_cast<last_teller_fn>(function)(), name, capacity); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto MatchesOffset(const uintptr_t address, const uint32_t offset) noexcept -> bool
    {
        __try { return *reinterpret_cast<const uint32_t*>(address) == offset; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ResetChat(const uintptr_t object, const uint32_t offset, const uintptr_t function, const bool recover = false) noexcept -> bool
    {
        using reset_chat_fn = int32_t(__thiscall*)(void*);
        __try
        {
            auto* state = reinterpret_cast<uint32_t*>(object + offset);
            if (function == 0 || *state != (recover ? 0u : 4u)) return false;
            // Cancel the native session, including its parser and target-selection state.
            return reinterpret_cast<reset_chat_fn>(function)(reinterpret_cast<void*>(object)) == 1 && *state == 3;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ClosePlayerMenu(const uintptr_t manager, const uintptr_t function) noexcept -> void
    {
        using close_menu_fn = void(__thiscall*)(void*, const char*);
        __try { reinterpret_cast<close_menu_fn>(function)(reinterpret_cast<void*>(manager), "menu    playermo"); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

namespace xiui
{
    auto IsTellRecipient(const std::string_view name) -> bool
    {
        return !name.empty() && name.size() < 64 && std::all_of(name.begin(), name.end(), [](const unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '.' || c == '@' || c == '+' || c == '_';
        });
    }

    auto IsTellDraft(const std::string_view text) -> bool
    {
        const auto end = text.find(' ');
        if (end == std::string_view::npos) return false;
        const auto command = text.substr(0, end);
        if (command != "/t" && command != "/tell") return false;
        const auto nameStart = text.find_first_not_of(' ', end);
        return nameStart != std::string_view::npos && IsTellRecipient(text.substr(nameStart, text.find(' ', nameStart) - nameStart));
    }

    auto native_chat_input::Initialize(IAshitaCore* core) -> void
    {
        *this = {};
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        const auto size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base != 0 && size != 0)
        {
            constexpr char pattern[] = "8B0D????????85C9750333C0C36A00";
            const auto match = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            if (match != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0) this->last_teller_ = match;
        }
        const auto* offsets = core != nullptr ? core->GetOffsetManager() : nullptr;
        if (offsets == nullptr) return;
        const auto main = offsets->Get("chat.inputtext", "inputtext.main");
        const auto raw = offsets->Get("chat.inputtext", "inputtext.raw.string");
        const auto state = offsets->Get("chat.inputtext", "inputtext.state");
        if (main < 0 || raw < 0 || state < 0 || main > 0x10000 || raw > 0x10000 || state > 0x10000 || main + raw == 0) return;
        this->raw_offset_ = static_cast<uint32_t>(main + raw);
        this->state_offset_ = static_cast<uint32_t>(state);
        if (base == 0 || size == 0) return;
        constexpr char closePattern[] = "8B81????????83E80474164875058B01FF50348B0D????????C6810D01000000C38B11C781????????03000000FF5230A1????????C6800D01000000C3";
        const auto close = Ashita::Memory::FindPattern(base, size, closePattern, 0, 0);
        if (close == 0 || Ashita::Memory::FindPattern(base, size, closePattern, 0, 1) != 0 ||
            !MatchesOffset(close + 2, this->state_offset_) || !MatchesOffset(close + 37, this->state_offset_)) return;
        constexpr char resetPattern[] =
            "568BF18B06FF505C8B168BCEFF52588B066A008BCEFF50608B168BCEFF5270"
            "8B066A0A6A006A006A006A786A056A006A038BCEFF50288B168BCEFF5234B8010000005EC3";
        const auto reset = Ashita::Memory::FindPattern(base, size, resetPattern, 0, 0);
        if (reset != 0 && Ashita::Memory::FindPattern(base, size, resetPattern, 0, 1) == 0) this->reset_chat_ = reset;
        constexpr char menuPattern[] =
            "8B44240483EC20568BF18D4C240450518BCEE8????????8D5424048BCE52E8????????"
            "85C0740A6A01508BCEE8????????5E83C420C20400";
        constexpr char cleanupPattern[] = "8B01FF503468????????B9????????E8????????C3";
        const auto menu = Ashita::Memory::FindPattern(base, size, menuPattern, 0, 0);
        const auto cleanup = Ashita::Memory::FindPattern(base, size, cleanupPattern, 0, 0);
        if (menu == 0 || cleanup == 0 || Ashita::Memory::FindPattern(base, size, menuPattern, 0, 1) != 0 ||
            Ashita::Memory::FindPattern(base, size, cleanupPattern, 0, 1) != 0) return;
        uintptr_t label{}, manager{}; int32_t relative{};
        std::memcpy(&label, reinterpret_cast<const void*>(cleanup + 6), 4);
        std::memcpy(&manager, reinterpret_cast<const void*>(cleanup + 11), 4);
        std::memcpy(&relative, reinterpret_cast<const void*>(cleanup + 16), 4);
        char name[17]{};
        if (cleanup + 20 + relative == menu && manager >= base && manager - base < size &&
            ReadText(reinterpret_cast<const char*>(label), name, sizeof(name)) && std::strcmp(name, "menu    inline  ") == 0)
        { this->close_menu_ = menu; this->menu_manager_ = manager; }
    }

    auto native_chat_input::LastTeller(void) const -> std::string
    {
        std::array<char, 64> name{};
        if (this->last_teller_ == 0 || !ReadLastTeller(this->last_teller_, name.data(), name.size()) || !IsTellRecipient(name.data())) return {};
        return name.data();
    }

    auto native_chat_input::TakeTell(IChatManager* chat) const -> std::optional<chat_draft>
    {
        return this->TakeDraft(chat, true);
    }

    auto native_chat_input::TakeCommand(IChatManager* chat) const -> std::optional<chat_draft>
    {
        return this->TakeDraft(chat, false);
    }

    auto native_chat_input::TakeDraft(IChatManager* chat, const bool tellOnly) const -> std::optional<chat_draft>
    {
        if (!this->HandoffAvailable() || chat == nullptr || chat->IsInputOpen() != static_cast<uint8_t>(Ashita::ChatInputOpenStatus::OpenedChat)) return std::nullopt;
        const auto* raw = chat->GetInputTextRaw();
        const auto rawAddress = reinterpret_cast<uintptr_t>(raw);
        std::array<char, 512> buffer{};
        if (rawAddress < this->raw_offset_ || !ReadText(raw, buffer.data(), buffer.size()) || buffer[0] != '/' ||
            (tellOnly && !IsTellDraft(buffer.data()))) return std::nullopt;
        const auto length = std::strlen(buffer.data());
        const auto caret = std::min<size_t>(chat->GetInputTextRawCaretPosition(), length);
        chat_draft result{FromGameText(buffer.data()), FromGameText(std::string_view{buffer.data(), caret}).size()};
        // Use the same native object and client-specific offsets as Ashita's input getters.
        if (chat->GetInputTextRaw() != raw || !ResetChat(rawAddress - this->raw_offset_, this->state_offset_, this->reset_chat_)) return std::nullopt;
        // The parent player menu retains its original recipient until it is dismissed.
        if (IsTellDraft(result.text)) ClosePlayerMenu(this->menu_manager_, this->close_menu_);
        return result;
    }

    auto native_chat_input::Recover(IChatManager* chat) const -> bool
    {
        if (!this->HandoffAvailable() || chat == nullptr || chat->IsInputOpen() != 0) return false;
        const auto* raw = chat->GetInputTextRaw();
        const auto address = reinterpret_cast<uintptr_t>(raw);
        std::array<char, 1> buffer{};
        return address >= this->raw_offset_ && ReadText(raw, buffer.data(), buffer.size()) && chat->GetInputTextRaw() == raw &&
            ResetChat(address - this->raw_offset_, this->state_offset_, this->reset_chat_, true);
    }
}
