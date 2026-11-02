#include "profile_share.hpp"
#include "profile_share_delta.hpp"

#include <compressapi.h>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <span>
#include <vector>

#pragma comment(lib, "Cabinet.lib")

namespace
{
    using xiui::maximum_profile_payload;
    using xiui::profile_share_detail::Checksum;
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    auto CompressPayload(const std::string_view text) -> std::vector<uint8_t>
    {
        if (text.empty() || text.size() > maximum_profile_payload) return {};
        COMPRESSOR_HANDLE compressor{};
        if (!::CreateCompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &compressor)) return {};
        SIZE_T size{};
        ::Compress(compressor, const_cast<char*>(text.data()), text.size(), nullptr, 0, &size);
        std::vector<uint8_t> data(size <= maximum_profile_payload ? size : 0);
        const bool compressed = !data.empty() && ::Compress(compressor, const_cast<char*>(text.data()), text.size(), data.data(), data.size(), &size) != FALSE;
        ::CloseCompressor(compressor);
        if (!compressed) return {};
        data.resize(size);
        return data;
    }

    auto DecompressPayload(const std::span<const uint8_t> data) -> std::string
    {
        if (data.empty() || data.size() > maximum_profile_payload) return {};
        DECOMPRESSOR_HANDLE decompressor{};
        if (!::CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &decompressor)) return {};
        SIZE_T size{};
        ::Decompress(decompressor, const_cast<uint8_t*>(data.data()), data.size(), nullptr, 0, &size);
        std::string text(size > 0 && size <= maximum_profile_payload ? size : 0, '\0');
        const bool decompressed = !text.empty() && ::Decompress(decompressor, const_cast<uint8_t*>(data.data()), data.size(), text.data(), text.size(), &size) != FALSE;
        ::CloseDecompressor(decompressor);
        return decompressed && size == text.size() ? text : std::string{};
    }

    auto Encode(const std::vector<uint8_t>& data) -> std::string
    {
        std::string result; uint32_t bits{}; int count{};
        for (const auto byte : data)
        {
            bits = (bits << 8) | byte; count += 8;
            while (count >= 6) { count -= 6; result += alphabet[(bits >> count) & 63]; }
        }
        if (count != 0) result += alphabet[(bits << (6 - count)) & 63];
        return result;
    }

    auto Decode(const std::string_view text) -> std::vector<uint8_t>
    {
        if (text.empty() || text.size() % 4 == 1) return {};
        const size_t size = text.size() / 4 * 3 + text.size() % 4 * 3 / 4;
        if (size > maximum_profile_payload) return {};
        std::vector<uint8_t> result; uint32_t bits{}; int count{};
        result.reserve(size);
        for (const auto character : text)
        {
            const auto value = alphabet.find(character);
            if (value == std::string_view::npos) return {};
            bits = (bits << 6) | static_cast<uint32_t>(value); count += 6;
            if (count >= 8) { count -= 8; result.push_back(static_cast<uint8_t>(bits >> count)); }
        }
        if (count != 0 && (bits & ((1u << count) - 1)) != 0) return {};
        return result;
    }
}

namespace xiui
{
    auto EncodeProfileKey(const std::string& name, const settings_store& snapshot, std::string& error) -> std::string
    {
        if (!profile_share_detail::ValidName(name)) { error = "Use a valid profile name before sharing."; return {}; }
        auto preferences = snapshot.Preferences("");
        preferences.WriteString("profile", "name", name); preferences.WriteInt("profile", "version", 1);
        const auto text = preferences.ToIniText();
        if (text.size() > maximum_profile_payload) { error = "This profile exceeds the 4 MB sharing limit."; return {}; }
        std::string key;
        const auto data = CompressPayload(text);
        if (!data.empty())
        {
            char prefix[24]{}; std::snprintf(prefix, sizeof(prefix), "XIUI1.%08X.", Checksum(text));
            key = std::string{prefix} + Encode(data);
        }
        const auto delta = profile_share_detail::EncodeDelta(name, preferences);
        if (!delta.empty())
        {
            auto packed = CompressPayload(delta);
            if (!packed.empty() && packed.size() + 4 <= maximum_profile_payload)
            {
                const auto checksum = Checksum(delta);
                std::vector<uint8_t> compact;
                for (unsigned shift = 0; shift < 32; shift += 8) compact.push_back(static_cast<uint8_t>(checksum >> shift));
                compact.insert(compact.end(), packed.begin(), packed.end());
                auto candidate = std::string{"XIUI2."} + Encode(compact);
                if (key.empty() || candidate.size() < key.size()) key = std::move(candidate);
            }
        }
        error = key.empty() ? "Could not prepare a share key." : "";
        return key;
    }

    auto DecodeProfileKey(const std::string_view key, std::string& error) -> std::optional<shared_profile>
    {
        error = "Invalid or damaged XIUI share key.";
        if (key.size() > maximum_profile_key) { error = "The old share key exceeds the 8 MB paste limit. Use a .xiui file instead."; return std::nullopt; }
        auto input = key;
        const auto trim = [](std::string_view text) {
            const auto first = text.find_first_not_of(" \t\r\n");
            return first == text.npos ? std::string_view{} : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        };
        input = trim(input);
        if (input.starts_with("```"))
        {
            const auto firstLine = input.find('\n');
            if (firstLine == input.npos || !input.ends_with("```")) { error = "Copy the complete share key, including both ends."; return std::nullopt; }
            input = trim(input.substr(firstLine + 1, input.size() - firstLine - 4));
        }
        std::string compact;
        compact.reserve(input.size());
        for (const auto c : input) if (c != ' ' && c != '\t' && c != '\r' && c != '\n') compact += c;
        if (compact.starts_with("XIUI2."))
        {
            const auto data = Decode(std::string_view{compact}.substr(6));
            if (data.size() <= 4) return std::nullopt;
            uint32_t checksum{};
            for (unsigned i = 0; i < 4; ++i) checksum |= uint32_t{data[i]} << (i * 8);
            const auto payload = DecompressPayload(std::span{data}.subspan(4));
            if (payload.empty() || Checksum(payload) != checksum) return std::nullopt;
            auto profile = profile_share_detail::DecodeDelta(payload);
            if (profile) error.clear();
            return profile;
        }
        if (!compact.starts_with("XIUI1."))
        { error = compact.starts_with("XIUI") ? "This share-key version is unsupported. Ask for a .xiui profile file." : "Paste an XIUI1 or XIUI2 share key, or choose a .xiui file."; return std::nullopt; }
        if (compact.size() <= 15 || compact[14] != '.') return std::nullopt;
        uint32_t checksum{};
        const auto parsed = std::from_chars(compact.data() + 6, compact.data() + 14, checksum, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != compact.data() + 14) return std::nullopt;
        const auto data = Decode(std::string_view{compact}.substr(15));
        const auto text = DecompressPayload(data);
        if (text.empty() || Checksum(text) != checksum || text.find('\0') != text.npos) return std::nullopt;
        auto store = settings_store::FromIniText(text);
        const auto name = store.ReadString("profile", "name");
        if (store.ReadInt("profile", "version", 0) != 1 || !profile_share_detail::ValidName(name)) return std::nullopt;
        error.clear();
        return shared_profile{name, store.Preferences("")};
    }
}
