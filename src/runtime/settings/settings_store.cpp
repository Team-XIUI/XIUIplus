#include "settings_store.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace xiui
{
    namespace
    {
        auto ProfileSection(const std::string& section) -> bool
        {
            return ::_strnicmp(section.c_str(), "session.", 8) != 0 && ::_stricmp(section.c_str(), "profile") != 0 && ::_stricmp(section.c_str(), "profiles") != 0;
        }
    }

    settings_store::settings_store(std::string path) : path_(std::move(path)) {}

    auto settings_store::FromIniText(std::string_view text) -> settings_store
    {
        settings_store result{""};
        const auto trim = [](const std::string_view value) {
            const auto start = value.find_first_not_of(" \t\r\n");
            return start == std::string_view::npos ? std::string_view{} : value.substr(start, value.find_last_not_of(" \t\r\n") - start + 1);
        };
        if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
        std::string section;
        while (!text.empty())
        {
            const auto end = text.find('\n');
            const auto line = trim(text.substr(0, end));
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            if (line.empty() || line.front() == ';' || line.front() == '#') continue;
            if (line.front() == '[')
            {
                const auto closing = line.find(']');
                section = closing == std::string_view::npos ? std::string{} : std::string{trim(line.substr(1, closing - 1))};
                continue;
            }
            const auto equals = line.find('=');
            if (section.empty() || equals == std::string_view::npos) continue;
            const auto key = trim(line.substr(0, equals));
            if (!key.empty()) result.writes_[section][std::string{key}] = std::string{trim(line.substr(equals + 1))};
        }
        return result;
    }

    auto settings_store::key_less::operator()(const std::string& left, const std::string& right) const -> bool
    {
        return ::_stricmp(left.c_str(), right.c_str()) < 0;
    }

    auto settings_store::ReadString(const char* section, const char* key, const char* fallback) const -> std::string
    {
        char value[1024]{};
        this->ReadText(section, key, fallback, value, static_cast<DWORD>(sizeof(value)));
        return value;
    }

    auto settings_store::WriteString(const char* section, const char* key, const std::string& value) const -> void
    { this->WriteText(section, key, value.c_str()); }

    auto settings_store::Preferences(std::string path) const -> settings_store
    {
        settings_store result{std::move(path)};
        result.OverlayPreferences(*this);
        return result;
    }

    auto settings_store::OverlayPreferences(const settings_store& source) const -> void
    {
        for (const auto& [section, values] : source.writes_)
            if (ProfileSection(section)) for (const auto& [key, value] : values) this->writes_[section][key] = value;
    }

    auto settings_store::ToIniText(void) const -> std::string
    {
        std::string text;
        for (const auto& [section, values] : this->writes_)
        {
            text += "[" + section + "]\r\n";
            for (const auto& [key, value] : values) text += key + "=" + value + "\r\n";
        }
        return text;
    }

    auto settings_store::VisitPreferences(const std::function<void(std::string_view, std::string_view, std::string_view)>& visit) const -> void
    {
        for (const auto& [section, fields] : this->writes_)
            if (ProfileSection(section)) for (const auto& [key, value] : fields) visit(section, key, value);
    }

    auto settings_store::PendingValue(const char* section, const char* key) const -> const std::string*
    {
        const auto foundSection = this->writes_.find(section);
        if (foundSection == this->writes_.end()) return nullptr;
        const auto found = foundSection->second.find(key);
        return found != foundSection->second.end() ? &found->second : nullptr;
    }

    auto settings_store::ReadText(const char* section, const char* key, const char* fallback, char* value, const DWORD size) const -> void
    {
        if (const auto* pending = this->PendingValue(section, key); pending != nullptr)
            std::snprintf(value, size, "%s", pending->c_str());
        else if (this->path_.empty())
            std::snprintf(value, size, "%s", fallback != nullptr ? fallback : "");
        else
            ::GetPrivateProfileStringA(section, key, fallback, value, size, this->path_.c_str());
    }

    auto settings_store::WriteText(const char* section, const char* key, const char* value) const -> void
    {
        this->writes_[section][key] = value;
    }

    auto settings_store::ReadBool(const char* section, const char* key, const bool fallback) const -> bool
    {
        return this->ReadInt(section, key, fallback ? 1 : 0) != 0;
    }

    auto settings_store::ReadInt(const char* section, const char* key, const int fallback) const -> int
    {
        if (const auto* pending = this->PendingValue(section, key); pending != nullptr)
            return static_cast<int>(std::strtol(pending->c_str(), nullptr, 10));
        if (this->path_.empty()) return fallback;
        return static_cast<int>(::GetPrivateProfileIntA(section, key, fallback, this->path_.c_str()));
    }

    auto settings_store::ReadFloat(const char* section, const char* key, const float fallback) const -> float
    {
        char fallbackText[32]{};
        char value[32]{};
        std::snprintf(fallbackText, sizeof(fallbackText), "%.3f", fallback);
        this->ReadText(section, key, fallbackText, value, static_cast<DWORD>(sizeof(value)));
        char* end{};
        const float parsed = std::strtof(value, &end);
        return end != value && *end == '\0' ? parsed : fallback;
    }

    auto settings_store::ReadColor(const char* section, const char* key, const D3DCOLOR fallback) const -> D3DCOLOR
    {
        char fallbackText[16]{};
        char value[16]{};
        std::snprintf(fallbackText, sizeof(fallbackText), "%06X", fallback & 0x00FFFFFFu);
        this->ReadText(section, key, fallbackText, value, static_cast<DWORD>(sizeof(value)));
        char* end{};
        const auto parsed = std::strtoul(value, &end, 16);
        return end != value && *end == '\0'
            ? D3DCOLOR_ARGB(255, (parsed >> 16) & 0xFF, (parsed >> 8) & 0xFF, parsed & 0xFF)
            : fallback;
    }

    auto settings_store::ReadArgb(const char* section, const char* key, const D3DCOLOR fallback) const -> D3DCOLOR
    {
        char fallbackText[16]{};
        char value[16]{};
        std::snprintf(fallbackText, sizeof(fallbackText), "%08X", fallback);
        this->ReadText(section, key, fallbackText, value, static_cast<DWORD>(sizeof(value)));
        char* end{};
        const auto parsed = std::strtoul(value, &end, 16);
        if (end == value || *end != '\0')
            return fallback;
        return std::strlen(value) <= 6
            ? static_cast<D3DCOLOR>(parsed | 0xFF000000u)
            : static_cast<D3DCOLOR>(parsed);
    }

    auto settings_store::ReadRect(const char* section, const ui::rect& fallback) const -> ui::rect
    {
        return {
            static_cast<float>(this->ReadInt(section, "x", static_cast<int>(fallback.x))),
            static_cast<float>(this->ReadInt(section, "y", static_cast<int>(fallback.y))),
            static_cast<float>(this->ReadInt(section, "width", static_cast<int>(fallback.width))),
            static_cast<float>(this->ReadInt(section, "height", static_cast<int>(fallback.height)))};
    }

    auto settings_store::WriteBool(const char* section, const char* key, const bool value) const -> void
    {
        this->WriteText(section, key, value ? "1" : "0");
    }

    auto settings_store::WriteInt(const char* section, const char* key, const int value) const -> void
    {
        char text[24]{};
        std::snprintf(text, sizeof(text), "%d", value);
        this->WriteText(section, key, text);
    }

    auto settings_store::WriteFloat(const char* section, const char* key, const float value) const -> void
    {
        char text[32]{};
        std::snprintf(text, sizeof(text), "%.3f", value);
        this->WriteText(section, key, text);
    }

    auto settings_store::WriteColor(const char* section, const char* key, const D3DCOLOR value) const -> void
    {
        char text[16]{};
        std::snprintf(text, sizeof(text), "%06X", value & 0x00FFFFFFu);
        this->WriteText(section, key, text);
    }

    auto settings_store::WriteArgb(const char* section, const char* key, const D3DCOLOR value) const -> void
    {
        char text[16]{};
        std::snprintf(text, sizeof(text), "%08X", value);
        this->WriteText(section, key, text);
    }

    auto settings_store::WriteRect(const char* section, const ui::rect& value) const -> void
    {
        this->WriteInt(section, "x", static_cast<int>(std::lround(value.x)));
        this->WriteInt(section, "y", static_cast<int>(std::lround(value.y)));
        this->WriteInt(section, "width", static_cast<int>(std::lround(value.width)));
        this->WriteInt(section, "height", static_cast<int>(std::lround(value.height)));
    }

    auto settings_store::Flush(void) const -> bool
    {
        bool saved = true;
        bool changed = false;
        for (const auto& [section, values] : this->writes_)
        {
            for (const auto& [key, value] : values)
            {
                char current[64]{};
                ::GetPrivateProfileStringA(section.c_str(), key.c_str(), nullptr, current, sizeof(current), this->path_.c_str());
                if (value == current) continue;
                changed = true;
                if (::WritePrivateProfileStringA(section.c_str(), key.c_str(), value.c_str(), this->path_.c_str()) == FALSE)
                    saved = false;
            }
        }
        if (changed)
        {
            // A successful profile-cache flush also returns zero.
            ::WritePrivateProfileStringA(nullptr, nullptr, nullptr, this->path_.c_str());
        }
        if (saved) this->writes_.clear();
        return saved;
    }
} // namespace xiui
