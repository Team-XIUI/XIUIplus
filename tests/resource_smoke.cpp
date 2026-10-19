#include "resources/embedded_resource.hpp"
#include "modules/minimap/minimap_maps.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace xiui
{
    struct minimap_checks
    {
        static auto Decode(const std::span<const uint8_t> bytes) -> std::optional<resources::menu_texture>
        { return minimap_maps::DecodeImage(bytes); }
    };
}

namespace
{
    auto Check(const bool value, const char* message) -> void
    { if (!value) throw std::runtime_error{message}; }

    auto RawResource(const HMODULE module, const uint16_t id) -> std::span<const uint8_t>
    {
        const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
        const auto loaded = resource != nullptr ? ::LoadResource(module, resource) : nullptr;
        const auto* bytes = loaded != nullptr ? static_cast<const uint8_t*>(::LockResource(loaded)) : nullptr;
        return bytes != nullptr ? std::span<const uint8_t>{bytes, ::SizeofResource(module, resource)} : std::span<const uint8_t>{};
    }
}

int main(const int argc, char** argv)
{
    try
    {
        Check(argc == 3, "pass the release DLL and project directory");
        const auto dll = std::filesystem::absolute(argv[1]);
        const auto module = ::LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        Check(module != nullptr, "load release resources without starting the plugin");
        struct cleanup { HMODULE module; ~cleanup() { ::FreeLibrary(module); } } unload{module};
        const auto root = std::filesystem::absolute(argv[2]);
        struct fixture { uint16_t id; const char* path; };
        for (const auto& item : {fixture{101, "assets/cursors/pointer.bgra"}, fixture{201, "assets/jobs/xivparty.bgra"},
            fixture{205, "assets/recast/recast.bgra"}, fixture{701, "assets/mobs/phoenix_mobs.bin"},
            fixture{800, "assets/icon-themes/status-hd.bgra"}, fixture{1100, "assets/actionbars/actions.bgra"},
            fixture{1102, "assets/actionbars/vincerno/icons.bgra"}})
        {
            std::ifstream file{root / item.path, std::ios::binary};
            const std::vector<uint8_t> original{std::istreambuf_iterator<char>{file}, {}};
            Check(!original.empty(), "read original asset fixture");
            const auto packed = RawResource(module, item.id);
            Check(packed.size() >= 8 && packed.size() < original.size() && std::memcmp(packed.data(), "XZIP", 4) == 0,
                "release embeds the compressed asset");
            Check(xiui::resources::LoadEmbeddedResource(module, item.id, original.size()) == original,
                "production decompression retains every original asset byte");
        }
        const auto packed = RawResource(module, 201);
        auto corrupt = std::vector<uint8_t>{packed.begin(), packed.end()};
        corrupt.back() ^= 1;
        Check(xiui::resources::DecodeEmbeddedResource(corrupt, 1024 * 1024).empty(), "reject a failed compressed checksum");
        Check(xiui::resources::DecodeEmbeddedResource(packed.first(packed.size() - 1), 1024 * 1024).empty(), "reject truncated compression");
        Check(xiui::resources::DecodeEmbeddedResource(packed, 16).empty(), "enforce decompression allocation bounds");
        const std::vector<uint8_t> legacy{1, 2, 3, 4};
        Check(xiui::resources::DecodeEmbeddedResource(legacy, 4) == legacy, "uncompressed resources remain supported");

        const auto maps = RawResource(module, 1000);
        Check(maps.size() >= 8 && std::memcmp(maps.data(), "XMAP", 4) == 0, "retain the native map record index");
        uint32_t count{};
        std::memcpy(&count, maps.data() + 4, 4);
        Check(count == 662 && maps.size() >= 8 + count * 16, "retain all regular and enhanced map entries");
        size_t decoded{};
        for (uint32_t index = 0; index < count; ++index)
        {
            const auto* row = maps.data() + 8 + index * 16;
            uint16_t zone{}; int16_t floor{}; uint32_t offset{}, size{};
            std::memcpy(&zone, row, 2); std::memcpy(&floor, row + 2, 2);
            if (!((zone == 100 && floor == 0 && row[4] == 0) || (zone == 184 && floor == 15 && row[4] == 1))) continue;
            std::memcpy(&offset, row + 8, 4); std::memcpy(&size, row + 12, 4);
            Check(offset <= maps.size() && size <= maps.size() - offset && size >= 12, "representative map payload is bounded");
            const auto imageBytes = maps.subspan(offset, size);
            Check(std::memcmp(imageBytes.data(), "RIFF", 4) == 0 && std::memcmp(imageBytes.data() + 8, "WEBP", 4) == 0,
                "regular and enhanced maps use WebP");
            const auto image = xiui::minimap_checks::Decode(imageBytes);
            Check(image && image->width == 512 && image->height == 512 && image->pixels.size() == 512 * 512 &&
                image->pixels[0] >> 24 == 0 && image->pixels[128 * 512 + 128] >> 24 == 255,
                "the statically linked x86 decoder loads map pixels without a Windows WebP codec");
            ++decoded;
        }
        Check(decoded == 2, "decode regular and nonsequential enhanced floor examples");
        const std::array<uint8_t, 12> brokenWebp{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
        Check(!xiui::minimap_checks::Decode(brokenWebp), "reject invalid WebP data");
        std::puts("Compressed resources: exact asset bytes, bounds/checksum failures and x86 WebP map decoding passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "resource check failed: %s\n", error.what());
        return 1;
    }
}
