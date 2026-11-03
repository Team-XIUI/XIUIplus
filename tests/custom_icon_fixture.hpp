#pragma once

#include "resources/custom_icon_library.hpp"

#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace xiui::tests
{
    inline constexpr std::array<uint8_t, 71> icon_png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,1,8,6,0,0,0,244,34,127,138,0,0,0,14,73,68,65,84,120,156,99,248,207,192,0,66,13,0,15,122,3,126,119,233,127,151,0,0,0,0,73,69,78,68,174,66,96,130};
    inline constexpr const char* icon_format_files[]{"sample.jpg", "sample.bmp", "animated.gif", "pages.tiff", "sample.ico", "sample.webp"};

    inline auto IconFormatImage(const char* name) -> std::vector<uint8_t>
    {
        std::ifstream file{std::filesystem::path{__FILE__}.parent_path() / "fixtures/custom-icons" / name, std::ios::binary};
        if (!file) throw std::runtime_error{"Could not open custom icon format fixture"};
        return {std::istreambuf_iterator<char>{file}, {}};
    }

    template<typename Predicate>
    auto WaitForIcon(Predicate&& predicate) -> void
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (!predicate())
        {
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error{"Custom icon worker timed out"};
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
    }

    struct custom_icon_fixture
    {
        std::filesystem::path temporary{std::filesystem::weakly_canonical(std::filesystem::temp_directory_path())};
        std::filesystem::path root{temporary / (L"XIUIplus-icon-check-" + std::to_wstring(::GetCurrentProcessId()) + L"-" + std::to_wstring(::GetTickCount64()))};
        std::filesystem::path icons{root / "icons"};
        custom_icon_fixture()
        {
            if (!std::filesystem::create_directory(root)) throw std::runtime_error{"Could not create isolated custom icon fixture"};
            std::filesystem::create_directory(icons);
        }
        ~custom_icon_fixture()
        {
            std::error_code error;
            if (root.parent_path() == temporary && root.filename().wstring().starts_with(L"XIUIplus-icon-check-")) std::filesystem::remove_all(root, error);
        }
        auto Write(const std::filesystem::path& relative, std::span<const uint8_t> bytes = icon_png) const -> void
        {
            const auto file = icons / relative;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream output{file, std::ios::binary};
            if (!output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error{"Could not write custom icon fixture"};
        }
    };
}
