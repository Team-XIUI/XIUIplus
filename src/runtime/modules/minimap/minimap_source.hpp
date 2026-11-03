#pragma once

#include "Ashita.h"
#include "minimap_model.hpp"

namespace xiui
{
    class minimap_source
    {
    public:
        auto Initialize(void) -> void;
        auto Clear(void) -> void;
        auto Update(IAshitaCore* core, uint64_t now, bool npcs = true, bool distantParty = false, bool enemies = true) -> void;
        auto HandlePartyPosition(std::span<const uint8_t> packet, uint64_t now) -> void;
        auto State(void) const -> const minimap_state& { return this->state_; }
        auto Available(void) const -> bool { return !this->records_.empty(); }
    private:
        friend struct minimap_checks;
        friend struct phoenix_integration_test;
        std::vector<native_map_record> records_{};
        std::vector<native_map_record> zone_records_{};
        const native_map_record* map_{}, *single_map_{};
        uintptr_t floor_function_{}, terrain_pointer_{};
        minimap_state state_{};
        uint64_t next_markers_{};
        uint64_t next_floor_{};
        uint16_t zone_{};
        struct party_position { uint32_t id{}; uint16_t zone{}; float x{}, y{}, height{}; uint64_t at{}; };
        std::array<party_position, 18> party_positions_{};
        uint64_t next_party_request_{};
        std::optional<int> floor_{};
        auto InitializeFloorLookup(uintptr_t base, size_t size) -> void;
        auto Floor(float x, float height, float y) const -> std::optional<int>;
        auto SelectMap(uint16_t zone, uint64_t now, float x, float height, float y) -> const native_map_record*;
    };
}
