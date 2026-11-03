#include "custom_icon_fixture.hpp"
#include "resources/wic_image.hpp"

#include <algorithm>
#include <cstdio>
#include <objbase.h>

namespace
{
    using namespace xiui::resources;
    auto Check(bool value, const char* message) -> void { if (!value) throw std::runtime_error{message}; }
    auto Decoded(custom_icon_library& library, std::string_view key) -> std::optional<menu_texture>
    {
        library.Request(key);
        std::optional<menu_texture> image;
        bool completed{};
        xiui::tests::WaitForIcon([&] {
            for (auto& result : library.TakeImages())
                if (result.key == key) { image = std::move(result.image); completed = true; }
            return completed;
        });
        return image;
    }
    auto Decode(void) -> void
    {
        const auto com = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        Check(SUCCEEDED(com), "initialize isolated WIC test apartment");
        struct apartment { ~apartment() { ::CoUninitialize(); } } lifetime;
        const auto image = custom_icon_library::DecodeImage(xiui::tests::icon_png);
        Check(image && image->width == 128 && image->height == 128 && image->pixels.front() == 0,
            "rectangular PNGs fit a square texture with transparent letterboxing");
        Check(image->pixels[64 * 128 + 10] == 0xFFFF0000 && image->pixels[64 * 128 + 110] == 0x800000FF,
            "PNG channel order and straight alpha survive decoding and scaling");
        const std::vector<uint8_t> palette{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,1,8,3,0,0,0,195,252,143,184,0,0,0,6,80,76,84,69,255,0,0,0,255,0,210,135,239,113,0,0,0,2,116,82,78,83,255,0,229,183,48,74,0,0,0,11,73,68,65,84,120,156,99,96,96,4,0,0,4,0,2,191,122,63,74,0,0,0,0,73,69,78,68,174,66,96,130};
        const auto indexed = custom_icon_library::DecodeImage(palette);
        Check(indexed && indexed->pixels[64 * 128 + 10] == 0xFFFF0000 && (indexed->pixels[64 * 128 + 110] >> 24) == 0,
            "indexed PNG palettes and transparency are supported");
        const std::vector<uint8_t> oversize{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,8,1,0,0,0,1,8,6,0,0,0,165,214,37,204,0,0,0,33,73,68,65,84,120,156,237,195,1,9,0,0,12,4,161,235,95,250,151,99,160,96,171,169,170,170,170,170,170,170,170,255,31,211,10,242,224,41,129,202,200,0,0,0,0,73,69,78,68,174,66,96,130};
        Check(!custom_icon_library::DecodeImage(oversize) && !custom_icon_library::DecodeImage(std::span{xiui::tests::icon_png}.first(20)),
            "oversized dimensions and truncated PNGs fail without creating images");
        for (const auto* name : xiui::tests::icon_format_files)
        {
            const auto decoded = custom_icon_library::DecodeImage(xiui::tests::IconFormatImage(name));
            Check(decoded && decoded->width == 128 && decoded->height == 128, name);
            const auto color = decoded->pixels[64 * 128 + 10];
            Check(((color >> 16) & 255) > 180 && ((color >> 8) & 255) < 80, "supported formats retain first-frame/page artwork");
            if (std::string_view{name} == "sample.webp")
                Check((decoded->pixels[64 * 128 + 110] >> 24) == 128, "the bundled WebP decoder preserves alpha when resizing");
        }
        for (const auto* suffix : {".png", ".jpg", ".JPEG", ".bmp", ".GIF", ".tif", ".TIFF", ".ico", ".WEBP"})
            Check(custom_icon_library::RelativePath(std::string{"custom:Icon"} + suffix).has_value(), "supported extensions and aliases are case-insensitive");
        Check(!DecodeWicImage(xiui::tests::icon_png, 1024, true), "the minimap decoder retains its square-image constraint");
    }
    auto Library(void) -> void
    {
        xiui::tests::custom_icon_fixture files;
        files.Write("Healing/Cure.PNG"); files.Write(std::filesystem::path{L"\u56de\u5fa9.png"});
        for (const auto* name : xiui::tests::icon_format_files) files.Write(std::filesystem::path{"Formats"} / name, xiui::tests::IconFormatImage(name));
        const std::array<uint8_t, 3> invalid{1,2,3}; files.Write("invalid.png", invalid); files.Write("ignored.txt", invalid);
        custom_icon_library library; library.SetDirectory(files.icons); library.Refresh();
        xiui::tests::WaitForIcon([&] { return !library.Catalog().scanning; });
        const auto catalog = library.Catalog();
        Check(catalog.scanned && catalog.error == custom_icon_scan_error::none && catalog.entries->size() == 3 + std::size(xiui::tests::icon_format_files),
            "the background catalog scans supported image files and nested folders only");
        for (const auto* name : xiui::tests::icon_format_files)
            Check(Decoded(library, std::string{"custom:Formats/"} + name).has_value(), "all supported file formats load through the background worker");
        const auto found = std::find_if(catalog.entries->begin(), catalog.entries->end(), [](const auto& entry) { return entry.key == "custom:Healing/Cure.PNG"; });
        Check(found != catalog.entries->end() && found->label == "Cure" && found->category == "Healing", "relative keys retain the actual filename and folder category");
        const auto japanese = std::find_if(catalog.entries->begin(), catalog.entries->end(), [](const auto& entry) { return entry.label == "\xE5\x9B\x9E\xE5\xBE\xA9"; });
        Check(japanese != catalog.entries->end() && Decoded(library, japanese->key).has_value(), "Unicode filenames survive cataloging and file lookup");
        Check(Decoded(library, found->key).has_value() && !Decoded(library, "custom:invalid.png"), "only successfully decoded custom files become ready");
        files.Write("invalid.png"); library.Request("custom:invalid.png");
        Check(library.State("custom:invalid.png") == custom_icon_state::failed && library.TakeImages().empty(), "failed files are not read every frame");
        const auto generation = catalog.generation;
        library.Refresh(); xiui::tests::WaitForIcon([&] { return !library.Catalog().scanning; });
        Check(library.Catalog().generation > generation && Decoded(library, "custom:invalid.png").has_value(), "Refresh invalidates failed files and reloads replacements");
        for (const auto* key : {"custom:../outside.png", "custom:C:/outside.png", "custom:/outside.png", "custom:folder/../outside.png", "custom:unsupported.svg"})
        { Check(!custom_icon_library::RelativePath(key), "custom keys cannot escape the icon root or select another format"); library.Request(key); }
        Check(library.TakeImages().empty(), "invalid imported keys never enqueue file reads");
        library.Request("custom:Healing/Cure.PNG"); library.Refresh();
        Check(library.TakeImages().empty(), "Refresh drops results queued by an older generation");
        library.Shutdown();
        Check(!library.Catalog().scanning && library.Catalog().entries->empty(), "shutdown joins workers and clears transient icon state");
        const auto optional = files.root / "new-icons";
        library.SetDirectory(optional); library.Refresh();
        xiui::tests::WaitForIcon([&] { return !library.Catalog().scanning; });
        Check(std::filesystem::is_directory(optional) && library.Catalog().error == custom_icon_scan_error::none,
            "the optional icon folder is created by the background scan rather than required at HUD startup");
        library.Shutdown();
        std::ofstream{files.root / "blocked-icons"} << "file";
        library.SetDirectory(files.root / "blocked-icons"); library.Refresh();
        xiui::tests::WaitForIcon([&] { return !library.Catalog().scanning; });
        Check(library.Catalog().error == custom_icon_scan_error::folder && library.Catalog().entries->empty(),
            "an unavailable optional folder remains a catalog error instead of failing the HUD");
    }
}

int main(void)
{
    try { Decode(); Library(); std::puts("Custom image formats, decoding, catalog, path confinement, refresh and worker lifetime passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "Custom icon check failed: %s\n", error.what()); return 1; }
}
