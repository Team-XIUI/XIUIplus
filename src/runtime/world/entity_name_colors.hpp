#pragma once

#include "ashita_compat.hpp"

#include "Ashita.h"

#include <algorithm>
#include <array>

namespace xiui
{
    inline constexpr D3DCOLOR unclaimed_name_color = 0xFFFFFFC2;
    inline constexpr D3DCOLOR group_claim_name_color = 0xFFFF8282;
    inline constexpr D3DCOLOR other_claim_name_color = 0xFFFF82FF;
    enum class entity_class : size_t { group_player, player, npc, unclaimed_mob, group_mob, other_mob, other, count };
    inline constexpr size_t entity_class_count = static_cast<size_t>(entity_class::count);
    inline constexpr const char* entity_class_names[]{"Group player", "Other player", "NPC", "Unclaimed monster", "Group-claimed monster", "Other-claimed monster", "Other entity"};
    inline constexpr const char* entity_class_keys[]{"group_player", "player", "npc", "unclaimed_mob", "group_mob", "other_mob", "other"};
    struct entity_name_palette
    {
        bool enabled{};
        std::array<D3DCOLOR, entity_class_count> colors{0xFFC2FFFF, 0xFFFFFFFF, 0xFFC2FFC2,
            unclaimed_name_color, group_claim_name_color, other_claim_name_color, 0xFFFFFFFF};
    };
    inline entity_name_palette shared_entity_name_palette{};

    inline auto ClassifyEntity(const uint32_t spawnFlags, const uint32_t claim, const bool groupClaim, const bool groupMember) -> entity_class
    {
        if ((spawnFlags & 0x01) != 0) return groupMember ? entity_class::group_player : entity_class::player;
        if ((spawnFlags & 0x102) != 0) return entity_class::npc;
        if ((spawnFlags & 0x10) == 0) return entity_class::other;
        if ((claim & 0xFFFF) == 0) return entity_class::unclaimed_mob;
        return groupClaim ? entity_class::group_mob : entity_class::other_mob;
    }

    inline auto IsMonster(const entity_class kind) -> bool
    { return kind == entity_class::unclaimed_mob || kind == entity_class::group_mob || kind == entity_class::other_mob; }

    inline auto EntityHasHealth(uint32_t spawnFlags) -> bool
    {
        // Pets can carry Ally without either the Player or Monster flag.
        return (spawnFlags & 0x02) == 0 && (spawnFlags & 0x111) != 0;
    }

    template<typename Group>
    auto IsGroupClaim(uint32_t claim, const Group& group) -> bool
    {
        const auto claimer = claim & 0xFFFF;
        return claimer != 0 && std::any_of(group.begin(), group.end(), [claimer](uint32_t id) {
            return (id & 0xFFFF) == claimer;
        });
    }

    inline auto EntityNameColor(uint32_t spawnFlags, uint32_t claim, bool groupClaim, bool groupMember,
        uint16_t nativeColor = 23) -> D3DCOLOR
    {
        if (shared_entity_name_palette.enabled)
            return shared_entity_name_palette.colors[static_cast<size_t>(ClassifyEntity(spawnFlags, claim, groupClaim, groupMember))];
        // Native entries use half-intensity RGB; entry 23 selects colors from entity state.
        constexpr std::array<uint32_t, 23> palette{
            0x808080, 0x618080, 0x616180, 0x416180, 0x618061, 0x808061, 0x804141, 0x804180,
            0x806141, 0x414141, 0x808080, 0x808080, 0x617180, 0x592121, 0x592121, 0x592121,
            0x806169, 0x494980, 0x802141, 0x214180, 0x616121, 0x013D5A, 0x2E4D20};
        if (nativeColor < palette.size())
        {
            const auto color = palette[nativeColor];
            const auto channel = [color](uint32_t shift) { return std::min(255u, ((color >> shift) & 0xFF) * 2); };
            return D3DCOLOR_XRGB(channel(16), channel(8), channel(0));
        }
        if ((spawnFlags & 0x01) != 0)
            return groupMember ? 0xFFC2FFFF : 0xFFFFFFFF;
        if ((spawnFlags & 0x102) != 0)
            return 0xFFC2FFC2;
        if ((spawnFlags & 0x10) == 0)
            return 0xFFFFFFFF;
        if ((claim & 0xFFFF) == 0)
            return unclaimed_name_color;
        return groupClaim ? group_claim_name_color : other_claim_name_color;
    }

    struct entity_name_state { entity_class kind{entity_class::other}; D3DCOLOR color{0xFFFFFFFF}; };

    inline auto ReadEntityNameState(const xiui::ashita::party_view& party, const xiui::ashita::entity_view& entity, uint32_t index) -> entity_name_state
    {
        if (index == 0 || index >= entity.GetEntityMapSize())
            return {};
        const auto claim = entity.GetClaimStatus(index);
        std::array<uint32_t, 18> group{};
        bool groupMember{};
        for (uint32_t member = 0; member < group.size(); ++member)
        {
            if (member != 0 && party.GetMemberIsActive(member) == 0)
                continue;
            group[member] = party.GetMemberServerId(member);
            groupMember = groupMember || (group[member] != 0 && party.GetMemberTargetIndex(member) == index);
        }
        const auto flags = entity.GetSpawnFlags(index);
        const auto groupClaim = IsGroupClaim(claim, group);
        return {ClassifyEntity(flags, claim, groupClaim, groupMember), EntityNameColor(flags, claim, groupClaim, groupMember, entity.GetNameColor(index))};
    }
    inline auto ReadEntityNameColor(const xiui::ashita::party_view& party, const xiui::ashita::entity_view& entity, const uint32_t index) -> D3DCOLOR
    { return ReadEntityNameState(party, entity, index).color; }
} // namespace xiui
