#include "settings_store.hpp"

#include <cmath>
#include <atomic>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <type_traits>
#include <vector>

namespace xiui
{
    namespace
    {
        constexpr size_t maximum_ini_size = 16 * 1024 * 1024;

        template<typename Char> auto IniText(const std::string_view value) -> std::basic_string<Char>
        {
            if constexpr (std::is_same_v<Char, char>) return std::string{value};
            else
            {
                std::wstring result(::MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0), L'\0');
                if (!result.empty()) ::MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), result.data(), static_cast<int>(result.size()));
                return result;
            }
        }

        template<typename Char> auto IniKey(std::basic_string_view<Char> value) -> std::string
        {
            if constexpr (std::is_same_v<Char, char>) return std::string{value};
            else
            {
                std::string result(::WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr), '\0');
                if (!result.empty()) ::WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()), result.data(), static_cast<int>(result.size()), nullptr, nullptr);
                return result;
            }
        }

        template<typename Char, typename Changes>
        auto MergeIni(const std::basic_string_view<Char> text, const Changes& changes) -> std::basic_string<Char>
        {
            auto pending = changes;
            std::basic_string<Char> result;
            std::string section;
            const auto trim = [](std::basic_string_view<Char> value) {
                while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
                while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
                return value;
            };
            const auto append = [&](const auto& fields) {
                if (!result.empty() && result.back() != '\n') result += IniText<Char>("\r\n");
                for (const auto& [key, value] : fields) result += IniText<Char>(key + "=" + value + "\r\n");
            };
            const auto finish = [&] {
                const auto found = pending.find(section);
                if (found != pending.end()) { append(found->second); pending.erase(found); }
            };
            for (size_t start = 0; start < text.size();)
            {
                const auto newline = text.find('\n', start);
                const auto end = newline == text.npos ? text.size() : newline + 1;
                const auto raw = text.substr(start, end - start);
                auto line = trim(raw.substr(0, raw.size() - (raw.back() == '\n' ? 1 : 0)));
                start = end;
                if (!line.empty() && line.front() == '[')
                {
                    finish();
                    const auto closing = line.find(']');
                    section = closing == line.npos ? "" : IniKey(trim(line.substr(1, closing - 1)));
                }
                else if (!line.empty() && line.front() != ';' && line.front() != '#')
                {
                    const auto equals = line.find('=');
                    const auto found = changes.find(section);
                    if (equals != line.npos && found != changes.end())
                    {
                        const auto key = IniKey(trim(line.substr(0, equals)));
                        if (const auto field = found->second.find(key); field != found->second.end())
                        {
                            result += IniText<Char>(field->first + "=" + field->second + "\r\n");
                            if (const auto remaining = pending.find(section); remaining != pending.end()) remaining->second.erase(key);
                            continue;
                        }
                    }
                }
                result.append(raw);
            }
            finish();
            for (const auto& [name, fields] : pending)
            {
                if (fields.empty()) continue;
                if (!result.empty() && result.back() != '\n') result += IniText<Char>("\r\n");
                result += IniText<Char>("[" + name + "]\r\n");
                append(fields);
            }
            return result;
        }

        // Scanners and other readers hold settings files open for a moment; brief retries outlast them.
        template<typename Attempt>
        auto RetryShared(Attempt attempt) -> bool
        {
            for (DWORD delay = 0;; delay = delay == 0 ? 20 : delay * 2)
            {
                if (delay != 0) ::Sleep(delay);
                if (attempt()) return true;
                const auto error = ::GetLastError();
                if (delay >= 320 || (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION))
                { ::SetLastError(error); return false; }
            }
        }

        auto ReadIniFile(const std::string& path, std::string& text) -> bool
        {
            HANDLE file = INVALID_HANDLE_VALUE;
            RetryShared([&] {
                file = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                return file != INVALID_HANDLE_VALUE;
            });
            if (file == INVALID_HANDLE_VALUE) return ::GetLastError() == ERROR_FILE_NOT_FOUND;
            LARGE_INTEGER size{};
            bool complete = ::GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= maximum_ini_size;
            if (complete)
            {
                text.resize(static_cast<size_t>(size.QuadPart));
                DWORD read{};
                complete = ::ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) && read == text.size();
            }
            ::CloseHandle(file);
            return complete;
        }

        auto ProfileSection(const std::string& section) -> bool
        {
            return ::_strnicmp(section.c_str(), "session.", 8) != 0 && ::_stricmp(section.c_str(), "profile") != 0 &&
                ::_stricmp(section.c_str(), "profiles") != 0 && ::_strnicmp(section.c_str(), "profiles.", 9) != 0 &&
                ::_strnicmp(section.c_str(), "controller.crossbar.device", 26) != 0;
        }
    }

    namespace { std::atomic<uint32_t> file_lookups{}; }

    settings_store::settings_store(std::string path) : path_(std::move(path)) {}

    auto settings_store::FileLookups(void) -> uint32_t { return file_lookups; }

    auto settings_store::CachedFile(std::string path) -> settings_store
    {
        settings_store result{std::move(path)};
        result.cache_sections_ = true;
        result.cache_file_loaded_ = result.CacheFile();
        return result;
    }

    auto settings_store::IndexFile(void) const -> void
    {
        if (this->cache_sections_ || this->path_.empty()) return;
        this->cache_sections_ = true;
        this->cache_file_loaded_ = this->CacheFile();
    }

    auto settings_store::AtomicFile(std::string path) -> settings_store
    {
        settings_store result{std::move(path)};
        result.atomic_file_ = true;
        return result;
    }

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
        {
            if (!ProfileSection(section)) continue;
            // Both maps share an order, so new sections are copied whole at the end.
            if (this->writes_.empty() || this->writes_.key_comp()(std::prev(this->writes_.end())->first, section))
                this->writes_.emplace_hint(this->writes_.end(), section, values);
            else for (const auto& [key, value] : values) this->writes_[section][key] = value;
        }
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

    auto settings_store::CachedValue(const char* section, const char* key) const -> const std::string*
    {
        if (this->cache_file_loaded_)
        {
            const auto foundSection = this->reads_.find(section);
            if (foundSection == this->reads_.end()) return nullptr;
            const auto found = foundSection->second.find(key);
            return found != foundSection->second.end() ? &found->second : nullptr;
        }
        auto [foundSection, inserted] = this->reads_.try_emplace(section);
        if (inserted)
        {
            // Keep Windows' INI parsing, but fetch each section once per load.
            std::vector<char> buffer(4096);
            ++file_lookups;
            while (::GetPrivateProfileSectionA(section, buffer.data(), static_cast<DWORD>(buffer.size()), this->path_.c_str()) == buffer.size() - 2)
                buffer.resize(buffer.size() * 2);
            for (const char* line = buffer.data(); *line != '\0'; line += std::strlen(line) + 1)
            {
                const auto* equals = std::strchr(line, '=');
                if (equals == nullptr) continue;
                std::string_view value{equals + 1};
                if (value.size() >= 2 && (value.front() == '\'' || value.front() == '"') && value.back() == value.front())
                    value = value.substr(1, value.size() - 2);
                foundSection->second.try_emplace(std::string{line, equals}, value);
            }
        }
        const auto found = foundSection->second.find(key);
        return found != foundSection->second.end() ? &found->second : nullptr;
    }

    auto settings_store::CacheFile(void) const -> bool
    {
        std::string bytes;
        return ReadIniFile(this->path_, bytes) && this->CacheText(std::move(bytes));
    }

    auto settings_store::CacheText(std::string bytes) const -> bool
    {
        if (bytes.starts_with("\xFF\xFE"))
        {
            if (bytes.size() % 2 != 0) return false;
            std::wstring wide((bytes.size() - 2) / 2, L'\0');
            std::memcpy(wide.data(), bytes.data() + 2, bytes.size() - 2);
            bytes = IniKey(std::wstring_view{wide});
        }
        std::string_view text{bytes};
        if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
        const auto trim = [](std::string_view value) {
            const auto start = value.find_first_not_of(" \t\r");
            return start == value.npos ? std::string_view{} : value.substr(start, value.find_last_not_of(" \t\r") - start + 1);
        };
        decltype(this->reads_)::mapped_type* fields{};
        while (!text.empty())
        {
            const auto end = text.find_first_of("\r\n\0", 0, 3);
            const auto line = trim(text.substr(0, end));
            text = end == text.npos || text[end] == '\0' ? std::string_view{} : text.substr(end + 1);
            if (line.empty() || line.front() == ';') continue;
            if (line.front() == '[')
            {
                const auto closing = line.find(']');
                const auto name = trim(line.substr(1, closing == line.npos ? line.size() - 1 : closing - 1));
                auto [section, inserted] = this->reads_.try_emplace(std::string{name});
                // Windows reads the first occurrence of a section and each key.
                fields = inserted ? &section->second : nullptr;
                continue;
            }
            const auto equals = line.find('=');
            if (fields == nullptr || equals == line.npos) continue;
            const auto key = trim(line.substr(0, equals));
            auto value = trim(line.substr(equals + 1));
            if (value.size() >= 2 && (value.front() == '\'' || value.front() == '"') && value.back() == value.front())
                value = value.substr(1, value.size() - 2);
            fields->try_emplace(std::string{key}, value);
        }
        return true;
    }

    auto settings_store::ReadText(const char* section, const char* key, const char* fallback, char* value, const DWORD size) const -> void
    {
        if (const auto* pending = this->PendingValue(section, key); pending != nullptr)
            std::snprintf(value, size, "%s", pending->c_str());
        else if (this->cache_sections_)
        {
            const auto* cached = this->CachedValue(section, key);
            std::snprintf(value, size, "%s", cached != nullptr ? cached->c_str() : (fallback != nullptr ? fallback : ""));
        }
        else if (this->path_.empty())
            std::snprintf(value, size, "%s", fallback != nullptr ? fallback : "");
        else
        {
            ++file_lookups;
            ::GetPrivateProfileStringA(section, key, fallback, value, size, this->path_.c_str());
        }
    }

    auto settings_store::WriteText(const char* section, const char* key, const char* value) const -> void
    {
        auto& hint = this->write_hint_;
        if (hint.fields == nullptr || ::_stricmp(hint.name.c_str(), section) != 0)
        {
            const auto found = this->writes_.try_emplace(section).first;
            hint.name = found->first; hint.fields = &found->second;
        }
        const auto field = hint.fields->lower_bound(key);
        if (field != hint.fields->end() && ::_stricmp(field->first.c_str(), key) == 0) field->second = value;
        else hint.fields->emplace_hint(field, key, value);
    }

    auto settings_store::ReadBool(const char* section, const char* key, const bool fallback) const -> bool
    {
        return this->ReadInt(section, key, fallback ? 1 : 0) != 0;
    }

    auto settings_store::ReadInt(const char* section, const char* key, const int fallback) const -> int
    {
        if (const auto* pending = this->PendingValue(section, key); pending != nullptr)
            return static_cast<int>(std::strtol(pending->c_str(), nullptr, 10));
        if (this->cache_sections_)
        {
            const auto* value = this->CachedValue(section, key);
            if (value == nullptr || value->empty()) return fallback;
            int number{};
            const auto parsed = std::from_chars(value->data(), value->data() + value->size(), number);
            if (parsed.ec == std::errc{} && parsed.ptr == value->data() + value->size()) return number;
            ++file_lookups;
            return static_cast<int>(::GetPrivateProfileIntA(section, key, fallback, this->path_.c_str()));
        }
        if (this->path_.empty()) return fallback;
        ++file_lookups;
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
        return end != value && *end == '\0' && std::isfinite(parsed) ? parsed : fallback;
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
        if (this->path_.empty()) return false;
        std::string original;
        if (!ReadIniFile(this->path_, original)) return false;
        std::string text;
        if (this->atomic_file_) text = this->ToIniText();
        else
        {
            auto changes = this->writes_;
            // Diff against the bytes already read; a second read could fail and fall back to per-section parsing.
            settings_store current{this->path_};
            current.cache_sections_ = true;
            current.cache_file_loaded_ = current.CacheText(original);
            for (auto section = changes.begin(); section != changes.end();)
            {
                for (auto field = section->second.begin(); field != section->second.end();)
                {
                    const auto* value = current.CachedValue(section->first.c_str(), field->first.c_str());
                    if (value != nullptr ? field->second == *value : field->second.empty()) field = section->second.erase(field);
                    else ++field;
                }
                if (section->second.empty()) section = changes.erase(section); else ++section;
            }
            if (changes.empty()) { this->writes_.clear(); this->write_hint_.fields = nullptr; return true; }
            if (original.starts_with("\xFF\xFE"))
            {
                if (original.size() % 2 != 0) return false;
                std::wstring wide((original.size() - 2) / 2, L'\0');
                std::memcpy(wide.data(), original.data() + 2, original.size() - 2);
                const auto merged = MergeIni(std::wstring_view{wide}, changes);
                text = "\xFF\xFE";
                text.append(reinterpret_cast<const char*>(merged.data()), merged.size() * sizeof(wchar_t));
            }
            else
            {
                const bool bom = original.starts_with("\xEF\xBB\xBF");
                text = bom ? "\xEF\xBB\xBF" : "";
                text += MergeIni(std::string_view{original}.substr(bom ? 3 : 0), changes);
            }
        }
        if (text.size() > maximum_ini_size) return false;
        if (text == original) { this->writes_.clear(); this->write_hint_.fields = nullptr; return true; }
        static std::atomic<uint32_t> serial{};
        const auto temporary = this->path_ + "." + std::to_string(::GetCurrentProcessId()) + "." + std::to_string(++serial) + ".tmp";
        const auto file = ::CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written{};
        const bool complete = ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE &&
            written == text.size() && ::FlushFileBuffers(file) != FALSE;
        ::CloseHandle(file);
        if (!complete || !RetryShared([&] { return ::MoveFileExA(temporary.c_str(), this->path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE; }))
        { ::DeleteFileA(temporary.c_str()); return false; }
        ::WritePrivateProfileStringA(nullptr, nullptr, nullptr, this->path_.c_str());
        this->reads_.clear();
        this->cache_file_loaded_ = false;
        this->writes_.clear(); this->write_hint_.fields = nullptr;
        return true;
    }
} // namespace xiui
