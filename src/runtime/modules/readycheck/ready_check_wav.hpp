#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace xiui
{
    struct ready_check_wav
    {
        uint32_t rate{};
        uint16_t channels{};
        std::vector<int16_t> samples{};
    };

    inline auto ReadyCheckWavFilename(const std::string_view filename) -> bool
    {
        if (filename.size() < 5 || filename.size() > 128 || filename.find_first_of("/\\:") != std::string_view::npos ||
            filename.find("..") != std::string_view::npos) return false;
        const auto extension = filename.substr(filename.size() - 4);
        return extension[0] == '.' && (extension[1] == 'w' || extension[1] == 'W') &&
            (extension[2] == 'a' || extension[2] == 'A') && (extension[3] == 'v' || extension[3] == 'V');
    }

    inline auto DecodeReadyCheckWav(const std::span<const uint8_t> bytes) -> ready_check_wav
    {
        ready_check_wav result;
        if (bytes.size() < 44 || bytes.size() > 8 * 1024 * 1024 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
            std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return result;
        const auto word = [](const uint8_t* at) { uint16_t value{}; std::memcpy(&value, at, 2); return value; };
        const auto dword = [](const uint8_t* at) { uint32_t value{}; std::memcpy(&value, at, 4); return value; };
        const auto length = dword(bytes.data() + 4);
        if (length < 36 || length > bytes.size() - 8) return result;
        uint16_t encoding{}, bits{}, align{};
        uint32_t average{};
        std::span<const uint8_t> data;
        for (size_t offset = 12; offset + 8 <= length + 8;)
        {
            const auto size = dword(bytes.data() + offset + 4);
            if (size > length + 8 - offset - 8) return {};
            const auto* chunk = bytes.data() + offset + 8;
            if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && size >= 16)
            {
                encoding = word(chunk); result.channels = word(chunk + 2); result.rate = dword(chunk + 4);
                average = dword(chunk + 8); align = word(chunk + 12); bits = word(chunk + 14);
            }
            if (std::memcmp(bytes.data() + offset, "data", 4) == 0) data = {chunk, size};
            offset += 8 + static_cast<size_t>(size) + (size & 1);
        }
        if (encoding != 1 || (result.channels != 1 && result.channels != 2) || (bits != 8 && bits != 16) ||
            result.rate < 8000 || result.rate > 192000 || align != result.channels * bits / 8 ||
            average != result.rate * align || data.empty() || data.size() % align != 0 || data.size() / align > result.rate * 30) return {};
        result.samples.resize(data.size() / (bits / 8));
        for (size_t index = 0; index < result.samples.size(); ++index)
            if (bits == 8) result.samples[index] = static_cast<int16_t>((static_cast<int>(data[index]) - 128) * 256);
            else std::memcpy(&result.samples[index], data.data() + index * 2, 2);
        return result;
    }
}
