#include "enemy_source.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto enemy_source::Clear(void) -> void
    {
        this->entries_.clear();
        this->group_.clear();
        this->next_scan_ = 0;
        this->player_id_ = this->zone_id_ = 0;
    }

    auto enemy_source::RefreshGroup(IParty& party, IEntity& entity) -> void
    {
        this->group_.clear();
        for (uint32_t member = 0; member < 18; ++member)
        {
            if (member != 0 && party.GetMemberIsActive(member) == 0) continue;
            const auto id = party.GetMemberServerId(member);
            if (id != 0) this->group_.insert(id);
            const auto index = party.GetMemberTargetIndex(member);
            if (index == 0 || index >= entity.GetEntityMapSize()) continue;
            const auto pet = entity.GetPetTargetIndex(index);
            if (pet > 0 && pet < entity.GetEntityMapSize())
            {
                const auto petId = entity.GetServerId(pet);
                if (petId != 0) this->group_.insert(petId);
            }
        }
    }

    auto enemy_source::IsLiveMonster(IEntity& entity, const uint32_t index) -> bool
    {
        if (index == 0 || index >= entity.GetEntityMapSize() || (entity.GetSpawnFlags(index) & 0x110) != 0x10)
            return false;
        const auto flags = entity.GetRenderFlags0(index);
        return (flags & 0x200) != 0 && (flags & 0x4000) == 0 && entity.GetHPPercent(index) > 0;
    }

    auto enemy_source::FindIndex(IAshitaCore& core, const uint32_t id) -> uint32_t
    {
        auto* entity = core.GetMemoryManager()->GetEntity();
        if (id == 0 || entity == nullptr) return 0;
        auto* resources = core.GetResourceManager();
        const auto index = resources != nullptr ? resources->GetEntityIndexById(id) : 0;
        if (index > 0 && index < entity->GetEntityMapSize() && entity->GetServerId(index) == id) return index;
        for (uint32_t candidate = 1; candidate < entity->GetEntityMapSize(); ++candidate)
            if (entity->GetServerId(candidate) == id) return candidate;
        return 0;
    }

    auto enemy_source::Find(const uint32_t id) -> enemy_entry*
    {
        const auto found = std::find_if(this->entries_.begin(), this->entries_.end(), [id](const auto& entry) { return entry.id == id; });
        return found != this->entries_.end() ? &*found : nullptr;
    }

    auto enemy_source::Add(const uint32_t id, const uint32_t index, const uint64_t now) -> enemy_entry*
    {
        if (auto* existing = this->Find(id)) return existing;
        if (this->entries_.size() >= capacity) return nullptr;
        auto& entry = this->entries_.emplace_back();
        entry.id = id; entry.index = index; entry.evidence_at = now;
        return &entry;
    }

    auto enemy_source::RefreshSession(IParty& party, IPlayer& player, IEntity& entity) -> bool
    {
        const auto index = party.GetMemberTargetIndex(0);
        if (player.GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            index == 0 || index >= entity.GetEntityMapSize())
        {
            this->Clear();
            return false;
        }
        const auto id = party.GetMemberServerId(0), zone = entity.GetZoneId(index);
        if (this->player_id_ != id || this->zone_id_ != zone)
        {
            this->Clear();
            this->player_id_ = id; this->zone_id_ = zone;
        }
        return id != 0;
    }

    auto enemy_source::Update(IAshitaCore* core, const uint64_t now, const bool includeSelected) -> void
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (party == nullptr || player == nullptr || entity == nullptr)
        {
            this->Clear();
            return;
        }
        if (!this->RefreshSession(*party, *player, *entity)) return;
        const auto selected = target != nullptr ? target->GetTargetIndex(0) : 0;
        for (auto& entry : this->entries_) entry.selected = entry.index == selected;
        if (now < this->next_scan_) return;
        this->next_scan_ = now + 200;
        this->RefreshGroup(*party, *entity);
        std::erase_if(this->entries_, [&](const auto& entry) {
            return !IsLiveMonster(*entity, entry.index) || entity->GetServerId(entry.index) != entry.id;
        });
        for (uint32_t index = 1; index < entity->GetEntityMapSize(); ++index)
        {
            if (!IsLiveMonster(*entity, index)) continue;
            const auto id = entity->GetServerId(index);
            const bool claimed = IsGroupClaim(entity->GetClaimStatus(index), this->group_);
            auto* entry = this->Find(id);
            if (entry == nullptr && (claimed || (includeSelected && selected == index))) entry = this->Add(id, index, now);
            if (entry == nullptr) continue;
            if (claimed || (includeSelected && selected == index)) entry->evidence_at = now;
            const char* name = entity->GetName(index);
            entry->name = name != nullptr ? name : "Enemy";
            entry->name_color = ReadEntityNameColor(*party, *entity, index);
            entry->hp_percent = std::min<uint32_t>(100, entity->GetHPPercent(index));
            entry->distance = std::sqrt(std::max(0.0f, entity->GetDistance(index)));
            entry->claimed = claimed;
            entry->hostile = entry->hostile || claimed;
            entry->selected = index == selected;
            entry->target_name.clear();
            if (entry->last_target != 0 && now - entry->action_at <= 30000)
            {
                const auto victim = FindIndex(*core, entry->last_target);
                const char* victimName = victim != 0 ? entity->GetName(victim) : nullptr;
                if (victimName != nullptr) entry->target_name = victimName;
            }
        }
        std::erase_if(this->entries_, [now, includeSelected](const auto& entry) {
            return !entry.claimed && ((!entry.hostile && !(includeSelected && entry.selected)) || now - entry.evidence_at > 30000);
        });
    }

    auto enemy_source::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> void
    {
        const combat_action_packet local{data};

        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (id != 0x28 || core == nullptr) return;
        const auto& action = (decoded != nullptr ? *decoded : local).Action();
        if (!action) return;
        auto* memory = core->GetMemoryManager();
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        if (entity == nullptr || party == nullptr || player == nullptr || !this->RefreshSession(*party, *player, *entity)) return;
        if (action->category != 1 && action->category != 2 && action->category != 3 && action->category != 4 &&
            action->category != 6 && action->category != 7 && action->category != 8 && action->category != 11 && action->category != 12) return;
        const auto index = FindIndex(*core, action->actor);
        if (!IsLiveMonster(*entity, index)) return;
        this->RefreshGroup(*party, *entity);
        if (this->group_.contains(action->actor)) return;
        bool attacksGroup{};
        for (size_t i = 0; i < action->target_count; ++i)
            attacksGroup = attacksGroup || this->group_.contains(action->targets[i].id);
        auto* entry = this->Find(action->actor);
        if (entry == nullptr && (attacksGroup || IsGroupClaim(entity->GetClaimStatus(index), this->group_)))
            entry = this->Add(action->actor, index, now);
        if (entry == nullptr) return;
        if (attacksGroup) entry->evidence_at = now;
        entry->hostile = true;
        entry->last_target = action->targets[0].id;
        entry->action_at = now;
        entry->attacking_player = entry->last_target == party->GetMemberServerId(0);
        const char* enemyName = entity->GetName(index);
        entry->name = enemyName != nullptr ? enemyName : "Enemy";
        entry->name_color = ReadEntityNameColor(*party, *entity, index);
        entry->hp_percent = std::min<uint32_t>(100, entity->GetHPPercent(index));
        entry->distance = std::sqrt(std::max(0.0f, entity->GetDistance(index)));
        entry->claimed = IsGroupClaim(entity->GetClaimStatus(index), this->group_);

        const auto message = action->targets[0].message;
        const bool interrupted = message == 16 || message == 84 || message == 106 ||
            (action->category == 7 && message == 0) || (action->category == 8 && (action->parameter & 0xFFFF) == 0x7073);
        if (interrupted)
        {
            if (entry->HasCast(now)) entry->interrupted_at = now;
            return;
        }
        if (action->category == 4 || action->category == 11)
        {
            entry->cast_name.clear();
            return;
        }
        auto* resources = core->GetResourceManager();
        if (resources == nullptr || (action->category != 7 && action->category != 8)) return;
        const auto ability = action->targets[0].parameter;
        const char* name{};
        float seconds = 3.0f;
        if (action->category == 8)
        {
            const auto* spell = resources->GetSpellById(ability);
            if (spell != nullptr) { name = spell->Name[2] != nullptr ? spell->Name[2] : spell->Name[0]; seconds = spell->CastTime / 4.0f; }
        }
        else if (ability >= 256) name = resources->GetString("monsters.abilities", ability - 256);
        else if (const auto* resource = resources->GetAbilityById(ability); resource != nullptr)
            name = resource->Name[2] != nullptr ? resource->Name[2] : resource->Name[0];
        if (name == nullptr || name[0] == '\0') return;
        entry->cast_name = name;
        entry->cast_id = ability;
        entry->cast_started = now;
        entry->cast_seconds = std::clamp(seconds, 0.25f, 30.0f);
        entry->interrupted_at = 0;
    }
} // namespace xiui
