#pragma once

#include "ashita_host.hpp"

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace xiui::ashita
{
    static_assert(ASHITA_INTERFACE_VERSION == 4.30, "Review the Ashita compatibility maps when updating the SDK.");

    namespace detail
    {
        template<auto Method> struct legacy_method;
        template<typename Result, typename Interface, typename... Parameters, Result(Interface::*Method)(Parameters...) const>
        struct legacy_method<Method>
        {
            template<typename... Arguments>
            static auto Call(const Interface* object, const size_t slot, Arguments&&... arguments) -> Result
            {
                // Keep member-call conventions, including hidden small-struct return storage.
                using function = Result(Interface::*)(Parameters...) const;
                const auto table = *reinterpret_cast<void* const* const*>(object);
                function target{};
                static_assert(sizeof(target) == sizeof(void*));
                std::memcpy(&target, &table[slot], sizeof(target));
                return (object->*target)(std::forward<Arguments>(arguments)...);
            }
        };
    }

    template<typename Derived, typename Interface> class memory_view
    {
    public:
        memory_view(const Interface* native = nullptr) : memory_view(native, native != nullptr && LegacyInterface()) {}
        memory_view(const Interface* native, const bool legacy) : native_(native), legacy_(legacy) {}
        explicit operator bool(void) const { return this->native_ != nullptr; }
        auto operator==(std::nullptr_t) const -> bool { return this->native_ == nullptr; }
        auto operator!=(std::nullptr_t) const -> bool { return this->native_ != nullptr; }
        auto operator->(void) const -> const Derived* { return static_cast<const Derived*>(this); }
        auto operator*(void) const -> const Derived& { return *static_cast<const Derived*>(this); }
    protected:
        template<auto Method, size_t LegacySlot, typename... Arguments>
        auto Invoke(Arguments&&... arguments) const -> decltype(auto)
        {
            if (this->legacy_) return detail::legacy_method<Method>::Call(this->native_, LegacySlot, std::forward<Arguments>(arguments)...);
            return (this->native_->*Method)(std::forward<Arguments>(arguments)...);
        }
        const Interface* native_{};
        bool legacy_{};
    };

    // 4.16 slots: AshitaXI/Ashita-v4beta, SDK commit 21053e446a0374ed19c3f9bb04f67cc7279af58d.
    // Keep the maps here; modules use the same typed methods on both interfaces.
#define XIUI_ASHITA_METHOD(result, name, parameters, arguments, legacy_slot) \
    auto name parameters const -> result { return this->template Invoke<&interface_type::name, legacy_slot> arguments; }

    class party_view final : public memory_view<party_view, IParty>
    {
        using interface_type = IParty;
    public:
        using memory_view::memory_view;
        XIUI_ASHITA_METHOD(uint32_t, GetAllianceLeaderServerId, (void), (), 1)
        XIUI_ASHITA_METHOD(uint32_t, GetAlliancePartyLeaderServerId1, (void), (), 2)
        XIUI_ASHITA_METHOD(uint32_t, GetAlliancePartyLeaderServerId2, (void), (), 3)
        XIUI_ASHITA_METHOD(uint32_t, GetAlliancePartyLeaderServerId3, (void), (), 4)
        XIUI_ASHITA_METHOD(int8_t, GetAllianceInvited, (void), (), 11)
        XIUI_ASHITA_METHOD(const char*, GetMemberName, (uint32_t index), (index), 14)
        XIUI_ASHITA_METHOD(uint32_t, GetMemberServerId, (uint32_t index), (index), 15)
        XIUI_ASHITA_METHOD(uint32_t, GetMemberTargetIndex, (uint32_t index), (index), 16)
        XIUI_ASHITA_METHOD(uint32_t, GetMemberHP, (uint32_t index), (index), 18)
        XIUI_ASHITA_METHOD(uint32_t, GetMemberMP, (uint32_t index), (index), 19)
        XIUI_ASHITA_METHOD(uint32_t, GetMemberTP, (uint32_t index), (index), 20)
        XIUI_ASHITA_METHOD(uint8_t, GetMemberHPPercent, (uint32_t index), (index), 21)
        XIUI_ASHITA_METHOD(uint8_t, GetMemberMPPercent, (uint32_t index), (index), 22)
        XIUI_ASHITA_METHOD(uint16_t, GetMemberZone, (uint32_t index), (index), 23)
        XIUI_ASHITA_METHOD(uint32_t, GetMemberFlagMask, (uint32_t index), (index), 25)
        XIUI_ASHITA_METHOD(uint16_t, GetMemberTreasureLot, (uint32_t index, uint32_t slot), (index, slot), 26)
        XIUI_ASHITA_METHOD(uint8_t, GetMemberMainJob, (uint32_t index), (index), 31)
        XIUI_ASHITA_METHOD(uint8_t, GetMemberMainJobLevel, (uint32_t index), (index), 32)
        XIUI_ASHITA_METHOD(uint8_t, GetMemberSubJob, (uint32_t index), (index), 33)
        XIUI_ASHITA_METHOD(uint8_t, GetMemberSubJobLevel, (uint32_t index), (index), 34)
        auto GetMemberMasterLevel(const uint32_t index) const -> uint8_t { return this->legacy_ ? 0 : this->native_->GetMemberMasterLevel(index); }
        XIUI_ASHITA_METHOD(uint8_t, GetMemberIsActive, (uint32_t index), (index), 38)
        XIUI_ASHITA_METHOD(uint32_t, GetStatusIconsServerId, (uint32_t index), (index), 40)
        XIUI_ASHITA_METHOD(uint64_t, GetStatusIconsBitMask, (uint32_t index), (index), 42)
        XIUI_ASHITA_METHOD(uint8_t*, GetStatusIcons, (uint32_t index), (index), 43)
    };
    inline auto View(const IParty* native) -> party_view { return {native}; }

    class entity_view final : public memory_view<entity_view, IEntity>
    {
        using interface_type = IEntity;
    public:
        using memory_view::memory_view;
        XIUI_ASHITA_METHOD(Ashita::FFXI::entity_t*, GetRawEntity, (uint32_t index), (index), 0)
        XIUI_ASHITA_METHOD(uint32_t, GetEntityMapSize, (void), (), 1)
        XIUI_ASHITA_METHOD(float, GetLocalPositionX, (uint32_t index), (index), 2)
        XIUI_ASHITA_METHOD(float, GetLocalPositionZ, (uint32_t index), (index), 3)
        XIUI_ASHITA_METHOD(float, GetLocalPositionY, (uint32_t index), (index), 4)
        XIUI_ASHITA_METHOD(float, GetLocalPositionYaw, (uint32_t index), (index), 7)
        XIUI_ASHITA_METHOD(uint32_t, GetTargetIndex, (uint32_t index), (index), 27)
        XIUI_ASHITA_METHOD(uint32_t, GetServerId, (uint32_t index), (index), 28)
        XIUI_ASHITA_METHOD(const char*, GetName, (uint32_t index), (index), 29)
        XIUI_ASHITA_METHOD(uintptr_t, GetActorPointer, (uint32_t index), (index), 32)
        XIUI_ASHITA_METHOD(float, GetDistance, (uint32_t index), (index), 35)
        XIUI_ASHITA_METHOD(float, GetHeading, (uint32_t index), (index), 38)
        XIUI_ASHITA_METHOD(uint8_t, GetHPPercent, (uint32_t index), (index), 40)
        XIUI_ASHITA_METHOD(uint8_t, GetType, (uint32_t index), (index), 41)
        XIUI_ASHITA_METHOD(uint8_t, GetRace, (uint32_t index), (index), 42)
        XIUI_ASHITA_METHOD(uint32_t, GetRenderFlags0, (uint32_t index), (index), 58)
        XIUI_ASHITA_METHOD(uint32_t, GetStatusServer, (uint32_t index), (index), 86)
        XIUI_ASHITA_METHOD(uint32_t, GetStatus, (uint32_t index), (index), 87)
        XIUI_ASHITA_METHOD(uint32_t, GetClaimStatus, (uint32_t index), (index), 91)
        XIUI_ASHITA_METHOD(uint32_t, GetZoneId, (uint32_t index), (index), 92)
        XIUI_ASHITA_METHOD(uint32_t, GetSpawnFlags, (uint32_t index), (index), 101)
        XIUI_ASHITA_METHOD(uint16_t, GetNameColor, (uint32_t index), (index), 103)
        XIUI_ASHITA_METHOD(uint16_t, GetTargetedIndex, (uint32_t index), (index), 113)
        XIUI_ASHITA_METHOD(uint16_t, GetPetTargetIndex, (uint32_t index), (index), 114)
        XIUI_ASHITA_METHOD(float, GetModelSize, (uint32_t index), (index), 120)
        XIUI_ASHITA_METHOD(float, GetModelHitboxSize, (uint32_t index), (index), 121)
        XIUI_ASHITA_METHOD(uint16_t, GetMonstrosityFlag, (uint32_t index), (index), 123)
        XIUI_ASHITA_METHOD(void, SetType, (uint32_t index, uint8_t type), (index, type), 174)
    };
    inline auto View(const IEntity* native) -> entity_view { return {native}; }

    class player_view final : public memory_view<player_view, IPlayer>
    {
        using interface_type = IPlayer;
    public:
        using memory_view::memory_view;
        XIUI_ASHITA_METHOD(uint32_t, GetHPMax, (void), (), 1)
        XIUI_ASHITA_METHOD(uint32_t, GetMPMax, (void), (), 2)
        XIUI_ASHITA_METHOD(uint8_t, GetMainJob, (void), (), 3)
        XIUI_ASHITA_METHOD(uint8_t, GetMainJobLevel, (void), (), 4)
        XIUI_ASHITA_METHOD(uint8_t, GetSubJob, (void), (), 5)
        XIUI_ASHITA_METHOD(uint8_t, GetSubJobLevel, (void), (), 6)
        XIUI_ASHITA_METHOD(uint16_t, GetExpCurrent, (void), (), 7)
        XIUI_ASHITA_METHOD(uint16_t, GetExpNeeded, (void), (), 8)
        XIUI_ASHITA_METHOD(int16_t, GetStat, (uint32_t sid), (sid), 9)
        XIUI_ASHITA_METHOD(int16_t, GetStatModifier, (uint32_t sid), (sid), 10)
        XIUI_ASHITA_METHOD(uint8_t, GetMasteryJob, (void), (), 30)
        XIUI_ASHITA_METHOD(uint8_t, GetMasteryJobLevel, (void), (), 31)
        XIUI_ASHITA_METHOD(uint32_t, GetMasteryExp, (void), (), 34)
        XIUI_ASHITA_METHOD(uint32_t, GetMasteryExpNeeded, (void), (), 35)
        XIUI_ASHITA_METHOD(Ashita::FFXI::combatskill_t, GetCombatSkill, (uint32_t sid), (sid), 36)
        XIUI_ASHITA_METHOD(uint16_t, GetAbilityRecast, (uint32_t index), (index), 38)
        XIUI_ASHITA_METHOD(uint32_t, GetMountRecast, (void), (), 42)
        XIUI_ASHITA_METHOD(uint16_t, GetLimitPoints, (void), (), 45)
        XIUI_ASHITA_METHOD(uint8_t, GetMeritPoints, (void), (), 46)
        XIUI_ASHITA_METHOD(bool, GetIsExperiencePointsLocked, (void), (), 49)
        XIUI_ASHITA_METHOD(bool, GetIsLimitModeEnabled, (void), (), 50)
        XIUI_ASHITA_METHOD(uint8_t, GetMeritPointsMax, (void), (), 51)
        XIUI_ASHITA_METHOD(uint32_t, GetIsZoning, (void), (), 53)
        XIUI_ASHITA_METHOD(uint16_t, GetCapacityPoints, (uint32_t jobid), (jobid), 70)
        XIUI_ASHITA_METHOD(uint16_t, GetJobPoints, (uint32_t jobid), (jobid), 71)
        XIUI_ASHITA_METHOD(uint16_t, GetJobPointsSpent, (uint32_t jobid), (jobid), 72)
        XIUI_ASHITA_METHOD(int16_t*, GetStatusIcons, (void), (), 73)
        XIUI_ASHITA_METHOD(uint32_t*, GetStatusTimers, (void), (), 74)
        XIUI_ASHITA_METHOD(int16_t*, GetBuffs, (void), (), 75)
        XIUI_ASHITA_METHOD(uint8_t, GetPetMPPercent, (void), (), 76)
        XIUI_ASHITA_METHOD(uint32_t, GetPetTP, (void), (), 77)
        XIUI_ASHITA_METHOD(uint8_t, GetJobLevel, (uint32_t jobid), (jobid), 78)
        XIUI_ASHITA_METHOD(uint8_t, GetLoginStatus, (void), (), 81)
        XIUI_ASHITA_METHOD(bool, HasAbilityData, (void), (), 82)
        XIUI_ASHITA_METHOD(bool, HasSpellData, (void), (), 83)
        XIUI_ASHITA_METHOD(bool, HasAbility, (uint32_t id), (id), 84)
        XIUI_ASHITA_METHOD(bool, HasPetCommand, (uint32_t id), (id), 85)
        XIUI_ASHITA_METHOD(bool, HasSpell, (uint32_t id), (id), 86)
        XIUI_ASHITA_METHOD(bool, HasWeaponSkill, (uint32_t id), (id), 88)
        XIUI_ASHITA_METHOD(bool, HasKeyItem, (uint32_t id), (id), 89)
    };
    inline auto View(const IPlayer* native) -> player_view { return {native}; }

    class target_view final : public memory_view<target_view, ITarget>
    {
        using interface_type = ITarget;
    public:
        using memory_view::memory_view;
        XIUI_ASHITA_METHOD(uint32_t, GetTargetIndex, (uint32_t index), (index), 2)
        XIUI_ASHITA_METHOD(uint32_t, GetServerId, (uint32_t index), (index), 3)
        XIUI_ASHITA_METHOD(uintptr_t, GetActorPointer, (uint32_t index), (index), 5)
        XIUI_ASHITA_METHOD(uint8_t, GetIsSubTargetActive, (void), (), 16)
        XIUI_ASHITA_METHOD(uint8_t, GetActionTargetActive, (void), (), 26)
        XIUI_ASHITA_METHOD(uint8_t, GetIsMenuOpen, (void), (), 28)
        XIUI_ASHITA_METHOD(uint8_t, GetActionType, (void), (), 30)
        XIUI_ASHITA_METHOD(uint32_t, GetActionId, (void), (), 32)
        XIUI_ASHITA_METHOD(uintptr_t, GetActionCallback, (void), (), 41)
        XIUI_ASHITA_METHOD(uintptr_t, GetCancelCallback, (void), (), 42)
        XIUI_ASHITA_METHOD(void, SetTarget, (uint32_t index, bool force), (index, force), 69)
    };
    inline auto View(const ITarget* native) -> target_view { return {native}; }

    template<typename Native> requires (!std::is_same_v<std::remove_cv_t<Native>, IParty> &&
        !std::is_same_v<std::remove_cv_t<Native>, IEntity> && !std::is_same_v<std::remove_cv_t<Native>, IPlayer> &&
        !std::is_same_v<std::remove_cv_t<Native>, ITarget>)
    inline auto View(Native* native) -> Native* { return native; }

#undef XIUI_ASHITA_METHOD
}
