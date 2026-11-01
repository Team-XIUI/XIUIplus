#pragma once

#include "combat/skillchain_source.hpp"

namespace xiui
{
    enum class chains_group : uint8_t { weapon, spell, pet };
    enum class chains_pet : uint8_t { none, avatar, jug, frame };
    struct chains_action
    {
        chains_group group{};
        uint8_t category{};
        uint16_t id{};
        std::string name{};
        bool ready{true};
        chains_pet requirement{};
    };
    struct chains_row
    {
        chains_group group{};
        std::string name{};
        skillchain_property result{};
        bool ready{};
    };
    struct chains_filters
    {
        bool weapon{true}, spell{true}, pet{true}, require_buff{}, avatar{true}, jug{true}, frame{true};
    };

    auto ChainsLevel(skillchain_property property) -> unsigned;
    auto ChainsRows(const skillchain_source& source, const skillchain_window& window,
        std::span<const chains_action> actions, const chains_filters& filters) -> std::vector<chains_row>;

    class chains_source
    {
    public:
        auto Clear(void) -> void;
        auto Update(IAshitaCore* core, const skillchain_source& source, uint64_t now) -> void;
        auto HandlePacket(uint16_t id, std::span<const uint8_t> data) -> void;
        auto Actions(void) const -> std::span<const chains_action> { return this->actions_; }

    private:
        std::vector<chains_action> actions_{};
        uint32_t player_{}, zone_{};
        uint8_t job_{}, subjob_{}, frame_{};
        uint16_t melee_{}, ranged_{};
        bool pup_skills_{}, initialized_{};
        uint64_t next_update_{};
        uint32_t job_offset_{};
        auto ReadJobData(IAshitaCore* core, std::array<uint8_t, 20>& result) -> bool;
    };
}
