#include "pet_source.hpp"
#include "ashita_compat.hpp"
#include "chat/chat_encoding.hpp"
#include "pet_lifetimes.hpp"
#include "modules/equipment/equipment_source.hpp"
#include "modules/mobinfo/mob_database.hpp"
#include <cstring>

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto PetKind(const uint8_t mainJob, const uint8_t subJob, const uint32_t flags, const std::string_view name) -> pet_kind
    {
        const bool beastmaster = mainJob == 9 || subJob == 9;
        if (beastmaster && JugLifetimeSeconds(name, server_rules::retail) != 0) return pet_kind::jug;
        if (beastmaster && (flags & 0x10) != 0) return pet_kind::charm;
        constexpr std::array avatars{"Carbuncle", "Ifrit", "Shiva", "Garuda", "Titan", "Ramuh", "Leviathan", "Fenrir", "Diabolos", "Atomos", "Odin", "Alexander", "Cait Sith", "Siren"};
        if ((mainJob == 15 || subJob == 15) && (name.ends_with(" Spirit") || std::find(avatars.begin(), avatars.end(), name) != avatars.end()))
            return pet_kind::avatar;
        if (beastmaster && !name.empty() && mainJob != 14 && mainJob != 15 && mainJob != 18) return pet_kind::charm;
        if (mainJob == 18) return pet_kind::automaton;
        if (mainJob == 14) return pet_kind::wyvern;
        if (mainJob == 15) return pet_kind::avatar;
        if (mainJob == 9) return pet_kind::jug;
        if (subJob == 15) return pet_kind::avatar;
        if (subJob == 9) return pet_kind::jug;
        return pet_kind::count;
    }

    auto PetRecast(const uint32_t timer, const pet_kind kind) -> bool
    {
        switch (kind)
        {
        case pet_kind::avatar: return timer == 173 || timer == 174 || timer == 108 || timer == 71;
        case pet_kind::jug: return timer == 102 || timer == 103 || timer == 104;
        case pet_kind::charm: return timer == 102 || timer == 103;
        case pet_kind::wyvern: return timer == 163 || timer == 162 || timer == 164 || timer == 70;
        case pet_kind::automaton: return (timer >= 205 && timer <= 210) || timer == 115;
        default: return false;
        }
    }

    auto pet_source::Publish(pet_snapshot sample, const uint64_t now) -> void
    {
        if ((this->charm_.owner && (sample.owner != this->charm_.owner || sample.zone != this->charm_.zone ||
            sample.main_job != this->charm_.main_job || sample.sub_job != this->charm_.sub_job)) ||
            (sample.pet.id == this->charm_.target && sample.pet.id != 0 && sample.pet.index != this->charm_.index) ||
            (this->state_.pet.id && sample.pet.id != this->state_.pet.id && sample.pet.id != this->charm_.target)) this->charm_ = {};
        const bool charmed = sample.pet.id != 0 && sample.pet.id == this->charm_.target && sample.pet.index == this->charm_.index && sample.owner == this->charm_.owner &&
            sample.zone == this->charm_.zone && this->charm_.started != 0;
        if (charmed) { sample.kind = pet_kind::charm; sample.mp_known = false; sample.mp_percent = 0; }
        const bool same = sample.owner == this->state_.owner && sample.zone == this->state_.zone && sample.main_job == this->state_.main_job &&
            sample.sub_job == this->state_.sub_job && sample.kind == this->state_.kind && sample.pet.id != 0 && sample.pet.id == this->state_.pet.id && sample.pet.index == this->state_.pet.index;
        sample.observed_since = same ? this->state_.observed_since : sample.pet.id != 0 ? now : 0;
        sample.estimated_expires = 0;
        if (same) sample.estimated_expires = this->state_.estimated_expires;
        else if (sample.kind == pet_kind::jug && sample.pet.id != 0)
        {
            const auto duration = JugLifetimeSeconds(sample.pet.name, this->rules_);
            if (duration) sample.estimated_expires = now + uint64_t{duration} * 1000;
        }
        if (charmed && sample.estimated_expires == 0)
        {
            if (this->charm_.level == 0)
                if (const auto* mob = MobById(sample.pet.id); mob && mob->minimum_level == mob->maximum_level) this->charm_.level = mob->minimum_level;
            const auto duration = CharmLifetimeSeconds(this->charm_.player_level, this->charm_.level, this->charm_.charisma, this->charm_.bonus_charisma, this->charm_.gear);
            if (duration) sample.estimated_expires = this->charm_.started + uint64_t{duration} * 1000 + this->charm_.familiar_bonus;
        }
        this->state_ = std::move(sample);
    }

    auto pet_source::BeginCharm(IAshitaCore* core, const uint32_t target, const uint32_t index, const uint64_t now) -> bool
    {
        auto* memory = core ? core->GetMemoryManager() : nullptr;
        const auto player = memory ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto party = memory ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto entity = memory ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        const auto* packets = core ? core->GetPacketManager() : nullptr;
        if (!player || !party || !entity || !packets || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            (player->GetMainJob() != 9 && player->GetSubJob() != 9)) return false;
        const auto owner = party->GetMemberServerId(0);
        const auto zone = party->GetMemberZone(0);
        const auto ownerIndex = party->GetMemberTargetIndex(0);
        if (!owner || !zone || !ownerIndex || ownerIndex >= entity->GetEntityMapSize() || entity->GetServerId(ownerIndex) != owner ||
            !target || !index || index >= entity->GetEntityMapSize() || entity->GetServerId(index) != target) return false;
        const auto petIndex = entity->GetPetTargetIndex(ownerIndex);
        if (petIndex != 0 && petIndex != ownerIndex && petIndex < entity->GetEntityMapSize() && entity->GetServerId(petIndex) != 0) return false;
        this->charm_ = {};
        this->charm_.target = target; this->charm_.index = index; this->charm_.owner = owner; this->charm_.zone = zone; this->charm_.attempted = now;
        this->charm_.main_job = player->GetMainJob(); this->charm_.sub_job = player->GetSubJob();
        this->charm_.player_level = player->GetMainJobLevel(); this->charm_.charisma = player->GetStat(6); this->charm_.bonus_charisma = this->bonus_charisma_;
        if (const auto* inventory = memory->GetInventory())
            for (uint32_t slot = 0; slot < 16; ++slot)
            {
                const auto item = equipment_source::ReadSlot(*inventory, slot);
                for (const auto& bonus : charm_equipment_bonuses)
                    if (item.ready && item.id == bonus.id && (bonus.id != 13667 || (this->rules_ == server_rules::horizon && player->GetSubJob() == 9))) this->charm_.gear += bonus.ranks;
            }
        // Only send the level check while a Charm estimate would be displayed.
        if (!this->charm_checks_) return true;
        std::array<uint8_t, 16> check{};
        std::memcpy(check.data() + 4, &target, 4); std::memcpy(check.data() + 8, &index, 4);
        this->charm_.check_pending = this->charm_.check_owned = true;
        core->GetPacketManager()->AddOutgoingPacket(0x00DD, static_cast<uint32_t>(check.size()), check.data());
        return true;
    }

    auto pet_source::HandleOutgoingPacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const bool injected) -> void
    {
        if (id == 0x00DD && !injected) { this->charm_.check_owned = false; return; }
        if (id != 0x001A || data.size() < 16) return;
        uint16_t category{}, ability{}, index{}; uint32_t target{};
        std::memcpy(&category, data.data() + 10, 2); std::memcpy(&ability, data.data() + 12, 2);
        if (category != 9 || ability != 52) return;
        std::memcpy(&target, data.data() + 4, 4); std::memcpy(&index, data.data() + 8, 2);
        this->BeginCharm(core, target, index, now);
    }

    auto pet_source::ObserveCheck(const std::span<const uint8_t> data, const uint64_t now) -> bool
    {
        if (!this->charm_.check_pending || data.size() < 28 || now < this->charm_.attempted || now - this->charm_.attempted >= 30000) return false;
        uint32_t actor{}, target{}, level{}, parameter{}; uint16_t message{};
        std::memcpy(&actor, data.data() + 4, 4); std::memcpy(&target, data.data() + 8, 4); std::memcpy(&level, data.data() + 12, 4);
        std::memcpy(&parameter, data.data() + 16, 4); std::memcpy(&message, data.data() + 24, 2); message &= 0x7FFF;
        if (actor != this->charm_.owner || target != this->charm_.target ||
            !((message >= 0xAA && message <= 0xB2 && parameter >= 0x40 && parameter <= 0x47) || message == 0xF9)) return false;
        this->charm_.check_pending = false;
        if (level > 0 && level <= 255 && message != 0xF9) this->charm_.level = level;
        return this->charm_.check_owned;
    }

    auto pet_source::ObserveAction(const combat_action& action, const uint64_t now) -> void
    {
        if (action.actor != this->charm_.owner || action.category != 6 || action.target_count == 0) return;
        auto ability = action.parameter & 0xFFFF; if (ability >= 512 && ability < 1024) ability -= 512;
        if (ability == 24 && this->charm_.started && action.targets[0].id == this->charm_.target && action.targets[0].message == 108)
        {
            this->charm_.familiar_bonus += 1500000;
            if (this->state_.kind == pet_kind::charm && this->state_.pet.id == this->charm_.target && this->state_.estimated_expires)
                this->state_.estimated_expires += 1500000;
        }
        if (ability != 52 || action.targets[0].id != this->charm_.target || now < this->charm_.attempted || now - this->charm_.attempted >= 30000) return;
        this->charm_.started = action.targets[0].message == 136 ? now : 0;
        if (this->charm_.started && this->state_.pet.id == this->charm_.target)
        { this->state_.estimated_expires = 0; this->state_.observed_since = now; }
    }

    auto pet_source::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> bool
    {
        if (id == 0x000A || id == 0x000B) { this->Clear(); return false; }
        const auto* memory = core ? core->GetMemoryManager() : nullptr;
        const auto party = memory ? xiui::ashita::View(memory->GetParty()) : nullptr;
        if (!party || party->GetMemberServerId(0) != this->charm_.owner || party->GetMemberZone(0) != this->charm_.zone) return false;
        if (id == 0x29) return this->ObserveCheck(data, now);
        if (id == 0x28 && decoded && decoded->Action()) this->ObserveAction(*decoded->Action(), now);
        return false;
    }
    auto pet_source::Update(IAshitaCore* core, const combat_source& combat, const uint64_t now) -> void
    {
        if (now < this->next_update_) return;
        this->next_update_ = now + 100;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        auto entity = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        this->resources_ = core != nullptr ? core->GetResourceManager() : nullptr;
        if (player == nullptr || party == nullptr || entity == nullptr || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        { this->Clear(); this->next_update_ = now + 100; return; }
        pet_snapshot sample;
        sample.owner = party->GetMemberServerId(0); sample.zone = party->GetMemberZone(0);
        sample.main_job = player->GetMainJob(); sample.sub_job = player->GetSubJob();
        const auto ownerIndex = party->GetMemberTargetIndex(0);
        const auto size = entity->GetEntityMapSize();
        if (sample.owner == 0 || sample.zone == 0 || ownerIndex == 0 || ownerIndex >= size || entity->GetServerId(ownerIndex) != sample.owner)
        { this->Clear(); return; }
        sample.ready = true; sample.kind = PetKind(sample.main_job, sample.sub_job, 0, "");
        const auto read = [&](const uint32_t index) -> pet_entity {
            if (index == 0 || index >= size || entity->GetServerId(index) == 0) return {};
            pet_entity value;
            value.id = entity->GetServerId(index); value.index = index; value.hp = std::min<uint32_t>(100, entity->GetHPPercent(index));
            const auto* name = entity->GetName(index); value.name = name != nullptr ? FromGameText(std::string_view{name, strnlen(name, 24)}) : "Pet";
            const auto distance = entity->GetDistance(index);
            value.distance_known = std::isfinite(distance) && distance >= 0;
            if (value.distance_known) value.distance = std::sqrt(distance);
            value.color = ReadEntityNameColor(*party, *entity, index);
            return value;
        };
        const auto petIndex = entity->GetPetTargetIndex(ownerIndex);
        if (petIndex != ownerIndex) sample.pet = read(petIndex);
        if (sample.pet.id != 0)
        {
            sample.kind = PetKind(sample.main_job, sample.sub_job, entity->GetSpawnFlags(petIndex), sample.pet.name);
            const auto mp = player->GetPetMPPercent();
            const auto tp = player->GetPetTP();
            sample.mp_known = (sample.kind == pet_kind::avatar || sample.kind == pet_kind::automaton) && mp <= 100;
            sample.mp_percent = mp; sample.tp_known = tp <= 3000; sample.tp = sample.tp_known ? tp : 0;
            auto targetIndex = entity->GetTargetedIndex(petIndex);
            const bool same = sample.owner == this->state_.owner && sample.zone == this->state_.zone && sample.main_job == this->state_.main_job &&
                sample.sub_job == this->state_.sub_job && sample.pet.id == this->state_.pet.id && sample.pet.index == this->state_.pet.index;
            const auto since = same ? this->state_.observed_since : now;
            if (const auto* actor = combat.Find(sample.pet.id); actor != nullptr && targetIndex == 0 && actor->targeted >= since)
                targetIndex = static_cast<uint16_t>(FindEntityIndex(*core, actor->target));
            if (targetIndex != petIndex) sample.target = read(targetIndex);
        }
        this->Publish(std::move(sample), now);
        if (const auto* actor = combat.Find(this->state_.pet.id); actor != nullptr)
            for (const auto& status : actor->statuses)
                if (status.observed >= this->state_.observed_since && this->state_.status_count < this->state_.statuses.size())
                    this->state_.statuses[this->state_.status_count++] = status;
    }
}
