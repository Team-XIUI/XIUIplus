#include "native_pattern.hpp"
#include "Ashita.h"
#include "Memory.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <random>
#include <stdexcept>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    { if (!condition) throw std::runtime_error{message}; }

    auto Reference(const uintptr_t base, const size_t size, const char* pattern) -> uintptr_t
    {
        const auto first = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
        return first != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0 ? first : 0;
    }
}

int main(void)
{
    try
    {
        const std::array<uint8_t, 8> bytes{0x11, 0x22, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        const auto base = reinterpret_cast<uintptr_t>(bytes.data());
        for (const auto* pattern : {"11", "1122", "22??44", "??2233", "33445566", "66", "6677", "????????????????", "1122??2233445566", "aa??BB"})
            Check(xiui::FindUniquePattern(base, bytes.size(), pattern) == Reference(base, bytes.size(), pattern), "exact, wildcard, overlapping, boundary and ambiguous matches agree with the SDK");
        Check(xiui::FindUniquePattern(base, 1, "??") == base, "one all-wildcard position is unique");
        for (const auto* pattern : {"", "A", "GG", "?1", "1?", "112233445566778899"})
            Check(xiui::FindUniquePattern(base, bytes.size(), pattern) == 0, "invalid and oversized signatures fail closed");
        Check(xiui::FindUniquePattern(0, bytes.size(), "11") == 0 && xiui::FindUniquePattern(base, 0, "11") == 0, "empty ranges fail closed");

        // The game's window move, intact and then with another plugin's jump and padding over its first two instructions.
        constexpr const char* move = "83EC0C668B44241453558B6C2418568BF1";
        std::array<uint8_t, 20> entry{0xCC, 0xCC, 0x83, 0xEC, 0x0C, 0x66, 0x8B, 0x44, 0x24, 0x14, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x18, 0x56, 0x8B, 0xF1, 0xCC};
        const auto entryBase = reinterpret_cast<uintptr_t>(entry.data());
        Check(xiui::FindUniqueEntry(entryBase, entry.size(), move, 8) == entryBase + 2, "an intact entry matches exactly");
        const std::array<uint8_t, 8> gate{0xE9, 0x10, 0x20, 0x30, 0x40, 0x90, 0x90, 0x90};
        std::memcpy(entry.data() + 2, gate.data(), gate.size());
        Check(xiui::FindUniquePattern(entryBase, entry.size(), move) == 0 && xiui::FindUniqueEntry(entryBase, entry.size(), move, 8) == entryBase + 2,
            "an entry detoured by another plugin is still found");
        entry[2] = 0x90;
        Check(xiui::FindUniqueEntry(entryBase, entry.size(), move, 8) == 0 && xiui::FindUniqueEntry(entryBase, entry.size(), move, 4) == 0,
            "a changed prologue that is not a jump fails closed");

        SYSTEM_INFO info{}; ::GetSystemInfo(&info);
        auto* pages = static_cast<uint8_t*>(::VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(pages != nullptr, "allocate guarded range");
        struct cleanup { void* memory; ~cleanup() { ::VirtualFree(memory, 0, MEM_RELEASE); } } release{pages};
        DWORD old{};
        Check(::VirtualProtect(pages + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old) != FALSE, "guard the search boundary");
        std::memcpy(pages + info.dwPageSize - bytes.size(), bytes.data(), bytes.size());
        Check(xiui::FindUniquePattern(reinterpret_cast<uintptr_t>(pages + info.dwPageSize - bytes.size()), bytes.size(), "??445566") ==
            reinterpret_cast<uintptr_t>(pages + info.dwPageSize - 4), "search and comparisons stay within the supplied range");

        std::mt19937 random{19};
        std::vector<uint8_t> image(12 * 1024 * 1024);
        for (auto& byte : image) byte = static_cast<uint8_t>(random());
        const auto imageBase = reinterpret_cast<uintptr_t>(image.data());
        constexpr const char* signature = "8B0D????????85C974108B4424088B5424045052E8????????C383C8FFC3";
        constexpr std::array<uint8_t, 30> needle{0x8B, 0x0D, 1, 2, 3, 4, 0x85, 0xC9, 0x74, 0x10, 0x8B, 0x44, 0x24, 8, 0x8B, 0x54, 0x24, 4, 0x50, 0x52, 0xE8, 1, 2, 3, 4, 0xC3, 0x83, 0xC8, 0xFF, 0xC3};
        constexpr size_t offset = 5 * 1024 * 1024;
        std::memcpy(image.data() + offset, needle.data(), needle.size());
        const auto start = std::chrono::steady_clock::now();
        const auto expected = Reference(imageBase, image.size(), signature);
        const auto referenceEnd = std::chrono::steady_clock::now();
        Check(expected == imageBase + offset && xiui::FindUniquePattern(imageBase, image.size(), signature) == expected, "native signature address agrees across a module-sized range");
        const auto end = std::chrono::steady_clock::now();
        std::printf("Unique native patterns and guarded bounds passed. 12 MiB: SDK %.2f ms; anchored search %.2f ms.\n",
            std::chrono::duration<double, std::milli>(referenceEnd - start).count(),
            std::chrono::duration<double, std::milli>(end - referenceEnd).count());
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "native pattern check failed: %s\n", error.what()); return 1; }
}
