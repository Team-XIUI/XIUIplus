#pragma once

#include <Windows.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xiui
{
    inline auto ReadEntityRenderPosition(const uintptr_t actor, const uintptr_t entity, const uintptr_t clientCode,
        const size_t clientSize, std::array<float, 3>& position) noexcept -> bool
    {
        if (!actor || !entity || !clientCode || clientSize < 7) return false;
        __try
        {
            // The native model draw uses this virtual position getter, not the movement anchor.
            if (*reinterpret_cast<const uintptr_t*>(actor + 0x70) != entity ||
                *reinterpret_cast<const uintptr_t*>(entity + 0xA0) != actor) return false;
            const auto table = *reinterpret_cast<const uintptr_t*>(actor);
            const auto getter = *reinterpret_cast<const uintptr_t*>(table + 0x1BC);
            if (getter < clientCode || getter - clientCode > clientSize - 7) return false;
            const auto* code = reinterpret_cast<const uint8_t*>(getter);
            if (code[0] != 0x8D || code[1] != 0x81 || code[6] != 0xC3) return false;
            uint32_t offset{};
            std::memcpy(&offset, code + 2, sizeof(offset));
            if (offset < sizeof(uintptr_t) || offset > 0x4000 || offset % alignof(float) != 0) return false;
            std::array<float, 3> result{};
            std::memcpy(result.data(), reinterpret_cast<const void*>(actor + offset), sizeof(result));
            for (const auto value : result) if (!std::isfinite(value)) return false;
            position = result;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}
