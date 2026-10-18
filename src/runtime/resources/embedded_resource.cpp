#include "embedded_resource.hpp"

#include <cstring>
#include <ctime>
#include "miniz_tinfl.h"

namespace xiui::resources
{
    auto DecodeEmbeddedResource(const std::span<const uint8_t> bytes, const size_t maximumSize) -> std::vector<uint8_t>
    {
        if (bytes.empty()) return {};
        if (bytes.size() < 4 || std::memcmp(bytes.data(), "XZIP", 4) != 0)
            return bytes.size() <= maximumSize ? std::vector<uint8_t>{bytes.begin(), bytes.end()} : std::vector<uint8_t>{};
        if (bytes.size() < 8) return {};
        uint32_t size{};
        std::memcpy(&size, bytes.data() + 4, sizeof(size));
        if (size == 0 || size > maximumSize) return {};
        std::vector<uint8_t> result(size);
        if (tinfl_decompress_mem_to_mem(result.data(), result.size(), bytes.data() + 8, bytes.size() - 8,
            TINFL_FLAG_PARSE_ZLIB_HEADER) != result.size()) return {};
        return result;
    }

    auto LoadEmbeddedResource(const HMODULE module, const uint16_t id, const size_t maximumSize) -> std::vector<uint8_t>
    {
        const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
        const auto size = resource != nullptr ? ::SizeofResource(module, resource) : 0;
        const auto loaded = resource != nullptr ? ::LoadResource(module, resource) : nullptr;
        const auto* bytes = loaded != nullptr ? static_cast<const uint8_t*>(::LockResource(loaded)) : nullptr;
        return bytes != nullptr ? DecodeEmbeddedResource({bytes, size}, maximumSize) : std::vector<uint8_t>{};
    }
}
