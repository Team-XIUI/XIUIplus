#pragma once

#include "Ashita.h"
#include <optional>
#include <string>

namespace xiui
{
    class profile_identity_source
    {
    public:
        auto Initialize(void) -> void;
        auto Read(IAshitaCore* core) const -> std::optional<std::string>;

    private:
        uintptr_t characters_{}, selection_{};
    };
}
