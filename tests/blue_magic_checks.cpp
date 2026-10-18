#include "modules/bluemagic/blue_magic_alert.hpp"

#include <cmath>
#include <iostream>

auto RunBlueMagicChecks(void) -> bool
{
    const auto check = [](const bool value, const char* label) {
        if (!value) std::cerr << "Blue Magic: " << label << '\n';
        return value;
    };
    bool okay = true;
    std::array<uint8_t, 0x1C> packet{};
    const auto set = [&](size_t at, uint32_t value, size_t bytes) { std::memcpy(packet.data() + at, &value, bytes); };
    set(8, 123, 4); set(12, 512, 4); set(24, 419, 2);
    okay &= check(xiui::DecodeBlueLearned(packet, 123) == 512, "local learned message");
    okay &= check(!xiui::DecodeBlueLearned(packet, 999), "unrelated recipient");
    okay &= check(!xiui::DecodeBlueLearned(std::span{packet}.first(25), 123), "truncated message");
    set(24, 38, 2); okay &= check(!xiui::DecodeBlueLearned(packet, 123), "ordinary skill gain");
    set(24, 419, 2); set(12, 2048, 4); okay &= check(!xiui::DecodeBlueLearned(packet, 123), "invalid spell index");
    xiui::blue_magic_alert alert;
    okay &= check(alert.Enqueue(512, 1000) && !alert.Enqueue(512, 1100), "duplicate event is silent");
    okay &= check(alert.Tick(1000, 3) && !alert.Tick(1001, 3), "starts exactly once");
    okay &= check(alert.Frame(1000, 3).entrance == 1 && alert.Frame(1700, 3).entrance == 0, "entrance boundaries");
    okay &= check(std::abs(alert.Frame(1850, 3).scale - 1.18f) < 0.001f, "pulse peak");
    okay &= check(std::abs(alert.Frame(5250, 3).alpha - 0.5f) < 0.001f && !alert.Frame(5500, 3).visible, "fade boundaries");
    for (uint16_t spell = 513; spell < 517; ++spell) okay &= check(alert.Enqueue(spell, 1100), "bounded pending queue accepts four");
    okay &= check(!alert.Enqueue(517, 1100), "pending queue remains bounded");
    okay &= check(alert.Tick(5500, 3) && alert.Frame(5500, 3).entrance == 1, "next event receives its own animation");
    alert.Clear(); okay &= check(!alert.Frame(5500, 3).visible && !alert.Tick(5500, 3), "lifecycle clears animation and pending events");
    return okay;
}
