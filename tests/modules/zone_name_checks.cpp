#include "modules/zonename/zone_name_module.hpp"

#include <cmath>
#include <iostream>

auto RunZoneNameChecks(void) -> bool
{
    const auto check = [](const bool value, const char* label) {
        if (!value) std::cerr << "Zone Name: " << label << '\n';
        return value;
    };
    const auto close = [](const float left, const float right) { return std::abs(left - right) < 0.001f; };
    bool okay = true;
    const auto start = xiui::ZoneNameFrame(0, 1, 3, 2);
    okay &= check(start.visible && close(start.alpha, 0) && close(start.offset, 1), "starts transparent, below its resting line");
    const auto entering = xiui::ZoneNameFrame(500, 1, 3, 2);
    okay &= check(entering.alpha > 0.5f && entering.alpha < 1 && entering.offset > 0 && entering.offset < 0.5f, "entrance fades in while sliding up");
    const auto held = xiui::ZoneNameFrame(2500, 1, 3, 2);
    okay &= check(held.visible && close(held.alpha, 1) && close(held.offset, 0), "holds fully visible at rest");
    const auto leaving = xiui::ZoneNameFrame(5000, 1, 3, 2);
    okay &= check(leaving.visible && close(leaving.alpha, 0.75f) && close(leaving.offset, 0.25f), "exit fades out while sliding down");
    okay &= check(!xiui::ZoneNameFrame(6000, 1, 3, 2).visible && !xiui::ZoneNameFrame(600000, 1, 3, 2).visible, "ends after the exit");
    return okay;
}
