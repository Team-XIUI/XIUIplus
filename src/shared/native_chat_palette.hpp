#pragma once

#include "chat_data.hpp"

namespace xiui
{
    class native_chat_palette
    {
    public:
        auto Initialize(uintptr_t module, uint32_t size) -> void;
        auto Read(void) const -> chat_palette;

    private:
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
        uintptr_t module_{}, modes_{}, colors_{}, fixed_{};
        uint32_t size_{};
        auto Contains(uintptr_t address, size_t bytes) const -> bool;
    };
}
