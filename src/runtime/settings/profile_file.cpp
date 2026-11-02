#include "profile_file.hpp"
#include "profile_share_delta.hpp"

#include <atomic>
#include <charconv>
#include <cstdio>
#include <fstream>

namespace
{
    constexpr std::string_view signature = "; XIUIplus profile 1 ";
    constexpr std::string_view legacy_signature = "; XIUI Native profile 1 ";
    constexpr size_t maximum_header_size = legacy_signature.size() + 10;
}

namespace xiui
{
    auto SerializeProfileFile(const std::string& name, const settings_store& snapshot, std::string& error) -> std::string
    {
        if (!profile_share_detail::ValidName(name)) { error = "Use a valid profile name before exporting."; return {}; }
        auto preferences = snapshot.Preferences("");
        preferences.WriteString("profile", "name", name);
        preferences.WriteInt("profile", "version", 1);
        const auto text = preferences.ToIniText();
        if (text.size() > maximum_profile_payload) { error = "This profile exceeds the 4 MB file limit."; return {}; }
        char checksum[16]{};
        std::snprintf(checksum, sizeof(checksum), "%08X\r\n", profile_share_detail::Checksum(text));
        error.clear();
        return std::string{signature} + checksum + text;
    }

    auto ParseProfileFile(const std::string_view bytes, std::string& error) -> std::optional<shared_profile>
    {
        error = "Invalid, damaged or unsupported XIUI+ profile file.";
        const auto prefix = bytes.starts_with(signature) ? signature : legacy_signature;
        const auto header = prefix.size() + 10;
        if (bytes.size() <= header || bytes.size() > header + maximum_profile_payload || !bytes.starts_with(prefix) ||
            bytes.substr(prefix.size() + 8, 2) != "\r\n") return std::nullopt;
        uint32_t checksum{};
        const auto parsed = std::from_chars(bytes.data() + prefix.size(), bytes.data() + prefix.size() + 8, checksum, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != bytes.data() + prefix.size() + 8) return std::nullopt;
        const auto text = bytes.substr(header);
        if (profile_share_detail::Checksum(text) != checksum || text.find('\0') != std::string_view::npos) return std::nullopt;
        const auto store = settings_store::FromIniText(text);
        const auto name = store.ReadString("profile", "name");
        if (store.ReadInt("profile", "version", 0) != 1 || !profile_share_detail::ValidName(name)) return std::nullopt;
        error.clear();
        return shared_profile{name, store.Preferences("")};
    }

    auto SaveProfileFile(const std::filesystem::path& path, const std::string& name, const settings_store& snapshot, std::string& error, const std::stop_token stop) -> bool
    {
        if (stop.stop_requested()) return false;
        const auto bytes = SerializeProfileFile(name, snapshot, error);
        if (bytes.empty() || stop.stop_requested()) return false;
        static std::atomic<uint64_t> sequence{};
        auto temporary = path;
        temporary += L".tmp." + std::to_wstring(::GetCurrentProcessId()) + L"." + std::to_wstring(++sequence);
        const auto file = ::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) { error = "Could not create the profile file. Choose a writable folder."; return false; }
        DWORD written{};
        const bool complete = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE &&
            written == bytes.size() && ::FlushFileBuffers(file) != FALSE;
        ::CloseHandle(file);
        if (!complete || stop.stop_requested() || !::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            ::DeleteFileW(temporary.c_str());
            error = "Could not save the profile file. The existing file is unchanged.";
            return false;
        }
        error.clear();
        return true;
    }

    auto ReadProfileFile(const std::filesystem::path& path, std::string& error) -> std::optional<shared_profile>
    {
        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file) { error = "Could not open the profile file."; return std::nullopt; }
        const auto size = file.tellg();
        if (size <= 0 || size > static_cast<std::streamoff>(maximum_header_size + maximum_profile_payload))
        { error = "The profile file is empty or exceeds the 4 MB file limit."; return std::nullopt; }
        std::string bytes(static_cast<size_t>(size), '\0');
        file.seekg(0);
        if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) || file.peek() != std::char_traits<char>::eof())
        { error = "Could not read the complete profile file."; return std::nullopt; }
        return ParseProfileFile(bytes, error);
    }
}
