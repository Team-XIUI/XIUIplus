#pragma once

#include "combat/combat_source.hpp"
#include "entities/entity_name_colors.hpp"
#include <string_view>

namespace xiui
{
    enum class pet_kind : size_t { avatar, jug, charm, wyvern, automaton, count };
    inline constexpr size_t pet_kind_count = static_cast<size_t>(pet_kind::count);
    inline constexpr const char* pet_kind_names[]{"Avatar / spirit", "Jug pet", "Charmed pet", "Wyvern", "Automaton"};
    inline constexpr const char* pet_kind_keys[]{"avatar", "jug", "charm", "wyvern", "automaton"};
    auto PetKind(uint8_t mainJob, uint8_t subJob, uint32_t spawnFlags, std::string_view name) -> pet_kind;
    auto PetRecast(uint32_t timer, pet_kind kind) -> bool;

    struct pet_entity
    {
        uint32_t id{}, index{}, hp{};
        float distance{};
        bool distance_known{};
        std::string name{};
        D3DCOLOR color{0xFFFFFFFF};
    };
    struct pet_snapshot
    {
        pet_entity pet{}, target{};
        uint32_t owner{}, zone{}, tp{};
        uint8_t main_job{}, sub_job{}, mp_percent{};
        pet_kind kind{pet_kind::count};
        bool ready{}, mp_known{}, tp_known{};
        uint64_t observed_since{};
        std::array<combat_status, 32> statuses{};
        size_t status_count{};
    };
    class pet_source
    {
    public:
        auto Clear(void) -> void { this->state_ = {}; this->next_update_ = 0; }
        auto Update(IAshitaCore* core, const combat_source& combat, uint64_t now) -> void;
        auto State(void) const -> const pet_snapshot& { return this->state_; }
        auto Resources(void) const -> IResourceManager* { return this->resources_; }
    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct pet_hud_test;
#endif
        pet_snapshot state_{};
        uint64_t next_update_{};
        IResourceManager* resources_{};
        auto Publish(pet_snapshot sample, uint64_t now) -> void;
    };
}
