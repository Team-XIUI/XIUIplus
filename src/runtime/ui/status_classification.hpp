#pragma once

#include <cstdint>

namespace xiui::ui
{
    // Effect-ID metadata from XIUI/libs/bufftable.lua; cancellation is unrelated to polarity.
    // Unlisted effects remain visible with buffs, including neutral and newly introduced effects.
    inline auto IsDebuffStatus(const uint16_t id) -> bool
    {
        if (id <= 23 || (id >= 28 && id <= 31) || (id >= 128 && id <= 149) ||
            (id >= 259 && id <= 264) || (id >= 391 && id <= 400) || (id >= 448 && id <= 452) ||
            (id >= 557 && id <= 567)) return true;
        switch (id)
        {
        case 155: case 156: case 168: case 174: case 175: case 186: case 189:
        case 192: case 193: case 194: case 223: case 291: case 298: case 299: case 309:
        case 404: case 536: case 572: case 576: case 597: case 630: case 631: case 798: case 805:
            return true;
        default: return false;
        }
    }
}
