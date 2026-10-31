#include "native_chat_filters.hpp"
#include "chat_encoding.hpp"
#include "Memory.h"

#include <algorithm>
#include <cstring>

namespace
{
    constexpr uint8_t window_bits[]{0, 10, 11, 1, 2, 3, 4, 6, 12, 13, 7, 8, 9,
        33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
    constexpr const char* window_names[]{"Say", "Shout", "Yell", "Tell", "Party", "Linkshell", "Linkshell 2", "Unity", "Assist J", "Assist E", "Emotes", "Messages", "NPC conversations",
        "HP/MP you recover", "HP/MP you lose", "Beneficial effects you receive", "Detrimental effects you receive", "Effects you resist", "Effects you evade",
        "HP/MP others recover", "HP/MP others lose", "Beneficial effects others receive", "Detrimental effects others receive", "Effects others resist", "Effects others evade",
        "Standard battle messages", "Calls for help", "Basic system messages"};
    static_assert(std::size(window_bits) == xiui::native_chat_filters::window_count && std::size(window_names) == std::size(window_bits));

    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto Label(const uintptr_t function, const uint32_t id, char* output, const size_t capacity) noexcept -> bool
    {
        using text_fn = const char*(__cdecl*)(uint32_t, uint32_t);
        __try
        {
            const auto* text = reinterpret_cast<text_fn>(function)(11, id);
            if (text == nullptr) return false;
            size_t i{};
            for (; i + 1 < capacity && text[i] != '\0'; ++i) output[i] = text[i];
            output[i] = '\0';
            return i != 0 && text[i] == '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto WithBit(const uint32_t value, const unsigned bit, const bool enabled) -> uint32_t
    { return enabled ? value | (1u << bit) : value & ~(1u << bit); }
}

namespace xiui
{
    auto native_chat_filters::Initialize(native_window_config& config) -> void
    {
        this->config_ = &config;
        this->definitions_available_ = false;
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        const auto size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size == 0) return;
        const auto find = [&](const char* pattern) {
            const auto match = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            return match != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0 ? match : 0;
        };
        const auto getter = find("56578B7C240C8BF18A04BD????????3C3375558A04BD????????84C07D1A");
        const auto text = find("56578B7C240C33F6393CF5????????74144683FE1C72F1");
        uintptr_t table{};
        if (getter == 0 || text == 0 || !Read(getter + 11, table) || table < base || sizeof(this->filters_) > size ||
            table - base > size - sizeof(this->filters_) || !Read(table, this->filters_)) return;
        for (size_t i = 0; i < this->filters_.size(); ++i)
        {
            const auto& entry = this->filters_[i];
            char label[256]{};
            if (entry.bit > 51 || entry.hold < -3 || entry.hold > 11 || (entry.bit != 51 && entry.hold < 0) ||
                entry.text > 4095 || !Label(text, entry.text, label, sizeof(label))) return;
            this->names_[i] = FromGameText(label);
        }
        const auto& emote = this->filters_[8];
        this->definitions_available_ = emote.bit == 3 && emote.hold == 5;
        this->Refresh();
    }

    auto native_chat_filters::Refresh(void) -> void
    {
        this->global_available_ = this->window_available_ = false;
        if (this->config_ == nullptr) return;
        const auto first = this->config_->ChatFilterValue(50), second = this->config_->ChatFilterValue(51);
        const auto holds = this->config_->ChatFilterValue(52), level = this->config_->ChatFilterValue(53), tell = this->config_->ChatFilterValue(71);
        if (this->definitions_available_ && first && second && holds && level && tell)
        {
            this->global_available_ = true;
            for (size_t i = 0; i < this->values_.size(); ++i)
            {
                const auto& entry = this->filters_[i];
                if (entry.bit == 51)
                    this->values_[i] = entry.hold < 0 ? *level >= static_cast<unsigned>(-entry.hold) :
                        static_cast<int>((*holds >> entry.hold) & 1) + (entry.hold == 2 ? static_cast<int>(*tell) * 2 : 0);
                else
                {
                    const bool hidden = (((entry.bit < 30 ? *first : *second) >> (entry.bit < 30 ? entry.bit : entry.bit - 30)) & 1) != 0;
                    this->values_[i] = hidden ? 1 : entry.hold < 11 && ((*holds >> entry.hold) & 1) != 0 ? 2 : 0;
                }
            }
        }
        const uint32_t id = this->group_ == 2 ? 187 : 185;
        const auto low = this->config_->ChatFilterValue(id), high = this->config_->ChatFilterValue(id + 1);
        if (low && high)
        {
            this->window_available_ = true;
            for (size_t i = 0; i < this->window_values_.size(); ++i)
            {
                const auto bit = window_bits[i];
                this->window_values_[i] = (((bit < 32 ? *low : *high) >> (bit % 32)) & 1) != 0;
            }
        }
    }

    auto native_chat_filters::WindowHasRouting(const bool secondary) const -> std::optional<bool>
    {
        if (this->config_ == nullptr) return std::nullopt;
        if (secondary)
            if (const auto count = this->config_->ChatWindowCount(); count && *count < 2) return false;
        const uint32_t id = secondary ? 187 : 185;
        const auto low = this->config_->ChatFilterValue(id), high = this->config_->ChatFilterValue(id + 1);
        if (!low || !high) return std::nullopt;
        return std::any_of(std::begin(window_bits), std::end(window_bits), [&](const auto bit) {
            return (((bit < 32 ? *low : *high) >> (bit % 32)) & 1) != 0;
        });
    }

    auto native_chat_filters::Visible(const size_t index) const -> bool
    { return index == 0 || (index <= global_count ? this->group_ == 0 : this->group_ != 0); }

    auto native_chat_filters::Option(const size_t index) -> module_option
    {
        if (index == 0)
        {
            static constexpr const char* groups[]{"Chat Filters", "Log: Window 1", "Log: Window 2"};
            return module_option::Dropdown("Native filter group", "Edits the game's settings directly. Selecting a group leaves every filter unchanged.", &this->group_, groups, std::size(groups)).Ordered(-40);
        }
        module_option result{};
        if (index <= global_count)
        {
            const auto i = index - 1;
            const auto& entry = this->filters_[i];
            static constexpr const char* binary[]{"Show", "Hide"};
            static constexpr const char* hold[]{"Show", "Hold"};
            static constexpr const char* filter[]{"Show", "Hide", "Hold"};
            static constexpr const char* tell[]{"Show", "Hold", "Notify", "Hold + Notify"};
            const bool holdOnly = entry.bit == 51 && entry.hold >= 0;
            const char* const* choices = holdOnly ? (entry.hold == 2 ? tell : hold) : entry.hold < 0 || entry.hold == 11 ? binary : filter;
            const size_t count = holdOnly ? (entry.hold == 2 ? 4 : 2) : entry.hold < 0 || entry.hold == 11 ? 2 : 3;
            result = module_option::Dropdown(this->names_[i].empty() ? "Native filter unavailable" : this->names_[i].c_str(),
                entry.hold < 0 ? "Native system levels are cumulative. Hiding a level also hides lower levels." :
                "Native Chat Filters setting. Show is filter Off; Hide is filter On. Hold retains messages longer. Tell Notify uses the game's notification sound.",
                &this->values_[i], choices, count);
            result.available = this->global_available_;
        }
        else if (index < option_count)
        {
            const auto i = index - 1 - global_count;
            result = module_option::Toggle(window_names[i], "Show this message type in the selected native Log window. XIUI follows that window's routing.", &this->window_values_[i]);
            result.available = this->window_available_;
        }
        return result;
    }

    auto native_chat_filters::Apply(const size_t index) -> bool
    {
        if (index == 0) { this->Refresh(); return true; }
        if (this->config_ == nullptr || index >= option_count) return false;
        const auto option = this->Option(index);
        if (!option.available || (option.choice != nullptr && (*option.choice < 0 || static_cast<size_t>(*option.choice) >= option.choice_count))) return false;
        bool applied = false;
        if (index > global_count && this->window_available_)
        {
            const auto i = index - 1 - global_count;
            const auto bit = window_bits[i];
            const uint32_t id = (this->group_ == 2 ? 187 : 185) + bit / 32;
            if (const auto current = this->config_->ChatFilterValue(id))
                applied = this->config_->SetChatFilterValue(id, WithBit(*current, bit % 32, this->window_values_[i]));
        }
        else if (index <= global_count && this->global_available_)
        {
            const auto& entry = this->filters_[index - 1];
            const auto value = this->values_[index - 1];
            if (entry.bit == 51 && entry.hold < 0)
            {
                if (const auto current = this->config_->ChatFilterValue(53))
                {
                    const uint32_t level = static_cast<uint32_t>(-entry.hold);
                    applied = this->config_->SetChatFilterValue(53, value != 0 ? std::max(*current, level) : std::min(*current, level - 1));
                }
            }
            else
            {
                const auto holds = this->config_->ChatFilterValue(52);
                if (entry.bit == 51)
                {
                    applied = holds && this->config_->SetChatFilterValue(52, WithBit(*holds, entry.hold, (value & 1) != 0));
                    if (applied && entry.hold == 2) applied = this->config_->SetChatFilterValue(71, (value & 2) != 0);
                }
                else
                {
                    const uint32_t id = entry.bit < 30 ? 50 : 51;
                    if (const auto current = this->config_->ChatFilterValue(id))
                    {
                        applied = this->config_->SetChatFilterValue(id, WithBit(*current, entry.bit < 30 ? entry.bit : entry.bit - 30, value == 1));
                        if (applied && entry.hold < 11) applied = holds && this->config_->SetChatFilterValue(52, WithBit(*holds, entry.hold, value == 2));
                    }
                }
            }
        }
        this->Refresh();
        return applied;
    }
}
