#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace xiui::ui
{
    inline constexpr const char* avatar_portrait_names[]{"Carbuncle", "Ifrit", "Shiva", "Garuda", "Titan", "Ramuh", "Leviathan",
        "Fenrir", "Diabolos", "Atomos", "Odin", "Alexander", "Cait Sith", "Siren", "Fire Spirit", "Ice Spirit",
        "Air Spirit", "Earth Spirit", "Thunder Spirit", "Water Spirit", "Light Spirit", "Dark Spirit"};
    inline constexpr std::string_view avatar_portrait_keys[]{"carbuncle", "ifrit", "shiva", "garuda", "titan", "ramuh", "leviathan",
        "fenrir", "diabolos", "atomos", "odin", "alexander", "caitsith", "siren", "firespirit", "icespirit",
        "windspirit", "earthspirit", "thunderspirit", "waterspirit", "lightspirit", "darkspirit"};

    // Cell order matches build-pet-portraits.py. Kinds follow the native Pet module.
    inline auto PetPortraitIndex(const std::string_view name, const int kind) -> int
    {
        if (kind == 1) return 23;
        if (kind == 3) return 22;
        if (kind != 0 || name.size() > 64) return -1;
        std::array<char, 64> normalized{};
        size_t length{};
        for (const unsigned char character : name)
            if (character >= 'A' && character <= 'Z') normalized[length++] = static_cast<char>(character + 'a' - 'A');
            else if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9')) normalized[length++] = static_cast<char>(character);
        auto key = std::string_view{normalized.data(), length};
        if (key == "airspirit") key = "windspirit";
        for (size_t index = 0; index < std::size(avatar_portrait_keys); ++index)
            if (avatar_portrait_keys[index] == key) return static_cast<int>(index);
        return -1;
    }

    inline auto PetPortraitFallbackJob(const int kind) -> uint32_t
    {
        static constexpr uint32_t jobs[]{15, 9, 9, 14, 18};
        return kind >= 0 && kind < static_cast<int>(std::size(jobs)) ? jobs[kind] : 0;
    }
}
