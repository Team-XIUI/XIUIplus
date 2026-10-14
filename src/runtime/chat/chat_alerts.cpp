#include "chat_alerts.hpp"
#include "chat_encoding.hpp"

#include <algorithm>

namespace
{
    auto Fold(const std::string_view text) -> std::wstring
    {
        auto wide = xiui::FromUtf8(text);
        if (wide.empty()) return wide;
        const auto length = ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr, 0);
        if (length <= 0) return wide;
        std::wstring result(static_cast<size_t>(length), L'\0');
        ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), static_cast<int>(wide.size()), result.data(), length, nullptr, nullptr, 0);
        return result;
    }

}

namespace xiui
{
    auto chat_alerts::Load(const settings_store& store) -> void
    {
        for (size_t i = 0; i < this->channels.size(); ++i)
            this->channels[i] = std::clamp(store.ReadInt("chat.sounds", chat_types[i].key, i == 0 ? 1 : 0), 0, static_cast<int>(std::size(chat_sound_names)) - 1);
        this->keyword_sound = std::clamp(store.ReadInt("chat.sounds", "keyword_sound", 1), 0, static_cast<int>(std::size(chat_sound_names)) - 1);
        this->keywords = store.ReadString("chat.sounds", "keywords");
        this->UpdateKeywords();
    }

    auto chat_alerts::Save(const settings_store& store) const -> void
    {
        for (size_t i = 0; i < this->channels.size(); ++i)
            store.WriteInt("chat.sounds", chat_types[i].key, this->channels[i]);
        store.WriteInt("chat.sounds", "keyword_sound", this->keyword_sound);
        store.WriteString("chat.sounds", "keywords", this->keywords);
    }

    auto chat_alerts::UpdateKeywords(void) -> void
    {
        this->terms_.clear();
        std::string_view remaining{this->keywords};
        while (!remaining.empty())
        {
            const auto comma = remaining.find(',');
            auto term = remaining.substr(0, comma);
            const auto first = term.find_first_not_of(" \t\r\n");
            if (first != std::string_view::npos)
            {
                term = term.substr(first, term.find_last_not_of(" \t\r\n") - first + 1);
                auto folded = Fold(term);
                if (!folded.empty() && std::find(this->terms_.begin(), this->terms_.end(), folded) == this->terms_.end())
                    this->terms_.push_back(std::move(folded));
            }
            if (comma == std::string_view::npos) break;
            remaining.remove_prefix(comma + 1);
        }
    }

    auto chat_alerts::Matches(const chat_line& line) const -> bool
    {
        return this->SoundFor(line) != 0;
    }

    auto chat_alerts::SoundFor(const chat_line& line) const -> int
    {
        if (this->keyword_sound != 0 && !this->terms_.empty())
        {
            std::string text;
            for (const auto& span : line.spans) text += span.text;
            const auto folded = Fold(text);
            if (std::any_of(this->terms_.begin(), this->terms_.end(), [&](const auto& term) { return folded.find(term) != std::wstring::npos; }))
                return this->keyword_sound;
        }
        return this->channels[ChatTypeIndex(line.mode)];
    }
}
