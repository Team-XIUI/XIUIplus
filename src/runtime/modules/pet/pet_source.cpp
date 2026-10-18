#include "pet_source.hpp"
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
        if (beastmaster && (flags & 0x110) == 0x10) return pet_kind::charm;
        constexpr std::array avatars{"Carbuncle", "Ifrit", "Shiva", "Garuda", "Titan", "Ramuh", "Leviathan", "Fenrir", "Diabolos", "Atomos", "Odin", "Alexander", "Cait Sith", "Siren"};
        if ((mainJob == 15 || subJob == 15) && (name.ends_with(" Spirit") || std::find(avatars.begin(), avatars.end(), name) != avatars.end()))
            return pet_kind::avatar;
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
        const bool same = sample.owner == this->state_.owner && sample.zone == this->state_.zone && sample.main_job == this->state_.main_job &&
            sample.sub_job == this->state_.sub_job && sample.pet.id != 0 && sample.pet.id == this->state_.pet.id && sample.pet.index == this->state_.pet.index;
        sample.observed_since = same ? this->state_.observed_since : sample.pet.id != 0 ? now : 0;
        if (same) sample.estimated_expires = this->state_.estimated_expires;
        else if (sample.kind == pet_kind::jug && sample.pet.id != 0)
        {
            const auto duration = JugLifetimeSeconds(sample.pet.name, this->rules_);
            if (duration) sample.estimated_expires = now + uint64_t{duration} * 1000;
        }
        if (sample.kind == pet_kind::charm && sample.pet.id == this->charm_target_ && sample.owner == this->charm_owner_ && sample.estimated_expires == 0 && this->charm_started_ && now - this->charm_started_ < 30000)
        {
            if (this->charm_level_ == 0)
                if (const auto* mob = MobById(sample.pet.id); mob && mob->minimum_level == mob->maximum_level) this->charm_level_ = mob->minimum_level;
            const auto duration = CharmLifetimeSeconds(this->charm_player_level_, this->charm_level_, this->charm_charisma_, this->bonus_charisma_, this->charm_gear_);
            if (duration) sample.estimated_expires = this->charm_started_ + uint64_t{duration} * 1000;
        }
        this->state_ = std::move(sample);
    }

    auto pet_source::HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded) -> void
    {
        auto* memory = core ? core->GetMemoryManager() : nullptr; auto* player = memory ? memory->GetPlayer() : nullptr;
        const auto* party = memory ? memory->GetParty() : nullptr;
        if (!player || !party || !party->GetMemberServerId(0)) return;
        if (id == 0x29 && data.size() >= 26 && this->charm_started_ && now >= this->charm_started_ && now - this->charm_started_ < 30000)
        {
            uint32_t target{}, level{}, parameter{}; uint16_t message{};
            std::memcpy(&target, data.data() + 8, 4); std::memcpy(&level, data.data() + 12, 4);
            std::memcpy(&parameter, data.data() + 16, 4); std::memcpy(&message, data.data() + 24, 2); message &= 0x7FFF;
            if (target == this->charm_target_ && level > 0 && level <= 255 && ((message >= 0xAA && message <= 0xB2) || (parameter >= 0x40 && parameter <= 0x47))) this->charm_level_ = level;
            return;
        }
        if (id != 0x28 || !decoded || !decoded->Action()) return;
        const auto& action = *decoded->Action();
        if (action.actor != party->GetMemberServerId(0) || action.category != 6 || action.target_count == 0) return;
        auto ability = action.parameter & 0xFFFF; if (ability >= 512 && ability < 1024) ability -= 512;
        if (ability == 8 && this->state_.kind == pet_kind::charm && this->state_.estimated_expires)
        { this->state_.estimated_expires += 1500000; return; }
        if (ability != 52) return;
        this->charm_target_ = action.targets[0].id; this->charm_owner_ = action.actor; this->charm_started_ = now; this->charm_level_ = this->charm_gear_ = 0;
        this->charm_player_level_ = player->GetMainJobLevel(); this->charm_charisma_ = player->GetStat(6);
        if (const auto* inventory = memory->GetInventory())
            for (uint32_t slot = 0; slot < 16; ++slot)
            {
                const auto item = equipment_source::ReadSlot(*inventory, slot);
                for (const auto& bonus : charm_equipment_bonuses)
                    if (item.ready && item.id == bonus.id && (bonus.id != 13667 || (this->rules_ == server_rules::horizon && player->GetSubJob() == 9))) this->charm_gear_ += bonus.ranks;
            }
    }
    auto pet_source::Update(IAshitaCore* core, const combat_source& combat, const uint64_t now) -> void
    {
        if (now < this->next_update_) return;
        this->next_update_ = now + 100;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
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
