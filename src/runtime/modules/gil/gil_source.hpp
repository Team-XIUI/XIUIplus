#pragma once

#include "Ashita.h"
#include "settings/settings_store.hpp"

#include <span>
#include <optional>

namespace xiui
{
    struct gil_session
    {
        uint32_t player{}, balance{}, baseline{};
        uint64_t started{};
        bool initialized{}, ready{};

        auto Observe(uint32_t playerId, uint32_t amount, uint64_t now) -> void;
        auto Reset(uint64_t now) -> void;
        auto Net(void) const -> int64_t { return static_cast<int64_t>(this->balance) - this->baseline; }
        auto PerHour(uint64_t now) const -> int64_t;
    };

    class gil_source
    {
    public:
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        static auto ReadBalance(const IInventory& inventory) -> std::optional<uint32_t>;
        auto HandlePacket(uint16_t id, std::span<const uint8_t> data) -> void;
        auto LoadSession(const settings_store& store) -> void;
        auto SaveSession(const settings_store& store) const -> void;
        auto Session(void) const -> const gil_session& { return this->session_; }
        auto ResetSession(uint64_t now) -> void { this->session_.Reset(now); }
        auto MenuOpen(void) const -> bool { return this->menu_open_; }

    private:
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
        gil_session session_{};
        uint64_t next_scan_{};
        bool syncing_{}, menu_open_{}, restored_{};
    };
}
