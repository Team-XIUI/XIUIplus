#include "combat_source.hpp"
#include "status_durations.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>

namespace xiui
{
    auto combat_cast::Active(const uint64_t now) const -> bool
    {
        return !this->name.empty() && now >= this->started && (this->interrupted != 0
            ? now >= this->interrupted && now - this->interrupted < 800
            : now - this->started < static_cast<uint64_t>((this->seconds + 1.5f) * 1000));
    }

    auto combat_cast::Ratio(const uint64_t now) const -> float
    {
        const auto until = this->interrupted != 0 ? this->interrupted : now;
        return until >= this->started ? std::clamp(static_cast<float>(until - this->started) / std::max(250.0f, this->seconds * 1000), 0.0f, 1.0f) : 0;
    }

    auto FindEntityIndex(IAshitaCore& core, const uint32_t id) -> uint32_t
    {
        const auto* memory = core.GetMemoryManager();
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        auto* resources = core.GetResourceManager();
        if (entity == nullptr || id == 0) return 0;
        const auto index = resources != nullptr ? resources->GetEntityIndexById(id) : 0;
        if (index != 0 && index < entity->GetEntityMapSize() && entity->GetServerId(index) == id) return index;
        for (uint32_t candidate = 1; candidate < entity->GetEntityMapSize(); ++candidate)
            if (entity->GetServerId(candidate) == id) return candidate;
        return 0;
    }

    auto IsStatusGain(const uint32_t message) -> bool
    {
        switch (message) {
        case 127: case 160: case 164: case 166: case 186: case 203: case 205: case 230: case 236: case 237:
        case 242: case 243: case 266: case 267: case 268: case 271: case 319: case 320: case 374: return true;
        default: return false;
        }
    }

    auto IsStatusLoss(const uint32_t message) -> bool
    {
        switch (message) {
        case 83: case 123: case 159: case 168: case 204: case 206: case 321: case 341: case 343: case 378: return true;
        default: return false;
        }
    }

    auto combat_source::Clear(void) -> void { this->actors_.clear(); this->player_ = this->zone_ = 0; this->next_update_ = 0; }

    auto combat_source::Actor(const uint32_t id, const uint64_t now) -> combat_actor&
    {
        if (!this->actors_.contains(id) && this->actors_.size() >= capacity)
        {
            const auto oldest = std::min_element(this->actors_.begin(), this->actors_.end(), [](const auto& a, const auto& b) { return a.second.updated < b.second.updated; });
            this->actors_.erase(oldest);
        }
        auto& actor = this->actors_[id];
        actor.id = id; actor.updated = now;
        return actor;
    }

    auto combat_source::Find(const uint32_t id) const -> const combat_actor*
    {
        const auto found = this->actors_.find(id);
        return found != this->actors_.end() ? &found->second : nullptr;
    }

    auto combat_source::ApplyStatus(const uint32_t target, const uint32_t status, const uint32_t spell, const uint32_t message,
        const uint64_t now, uint32_t seconds) -> void
    {
        if (target == 0 || status == 0 || status == 255 || status > 1023) return;
        if (IsStatusLoss(message))
        {
            if (auto found = this->actors_.find(target); found != this->actors_.end())
                std::erase_if(found->second.statuses, [status](const auto& value) { return value.id == status; });
            return;
        }
        if (!IsStatusGain(message)) return;
        if (seconds == 0) seconds = XIUIStatusSeconds(spell != 0 ? 4 : 0, spell, status, message);
        auto& statuses = this->Actor(target, now).statuses;
        auto next = combat_status{static_cast<uint16_t>(status), static_cast<uint16_t>(spell), now,
            seconds != 0 ? now + static_cast<uint64_t>(seconds) * 1000 : 0, false, seconds != 0};
        if (this->rules_ == server_rules::phoenix)
        {
            uint32_t minimum{}, maximum{};
            if (spell == 58 || spell == 80 || spell == 59) { minimum = 30; maximum = 120; }
            else if (spell == 254 || spell == 276) { minimum = 80; maximum = 300; }
            else if (spell == 220 || spell == 225) minimum = maximum = 30;
            if (maximum != 0)
            {
                next.expires = now + uint64_t{maximum} * 1000;
                next.earliest = minimum != maximum ? now + uint64_t{minimum} * 1000 : 0;
                next.estimated = true;
            }
        }
        const auto existing = std::find_if(statuses.begin(), statuses.end(), [status](const auto& value) { return value.id == status; });
        if (existing != statuses.end())
        {
            *existing = next;
            return;
        }
        if (statuses.size() == status_capacity) statuses.erase(statuses.begin());
        statuses.push_back(next);
    }

    auto combat_source::Observe(const combat_action& action, IResourceManager* resources, const uint64_t now) -> void
    {
        if (action.actor == 0 || action.target_count == 0 || action.target_count > action.targets.size()) return;
        auto& actor = this->Actor(action.actor, now);
        actor.target = action.targets[0].id; actor.targeted = now;
        const auto message = action.targets[0].message;
        const bool interrupted = (action.parameter & 0xFFFF) == 0x7073 || message == 16 || message == 84 || message == 106;
        if (interrupted)
        { if (actor.cast.Active(now)) actor.cast.interrupted = now; }
        else if (action.category == 4 || action.category == 5 || action.category == 11) actor.cast = {};
        else if ((action.category == 7 || action.category == 8 || action.category == 9) && resources != nullptr)
        {
            const auto id = action.targets[0].parameter;
            const char* name{}; float seconds = 3;
            if (action.category == 8)
            {
                if (const auto* spell = resources->GetSpellById(id))
                { name = spell->Name[2] != nullptr ? spell->Name[2] : spell->Name[0]; seconds = spell->CastTime / 4.0f; }
            }
            else if (action.category == 9)
            {
                if (const auto* item = resources->GetItemById(id))
                { name = item->Name[2] != nullptr ? item->Name[2] : item->Name[0]; seconds = item->CastTime / 4.0f; }
            }
            else if (id >= 256) name = resources->GetString("monsters.abilities", id - 256);
            else if (const auto* ability = resources->GetAbilityById(id)) name = ability->Name[2] != nullptr ? ability->Name[2] : ability->Name[0];
            if (name != nullptr && *name) actor.cast = {FromGameText(name), now, 0, std::clamp(seconds, 0.25f, 60.0f)};
        }
        for (size_t i = 0; i < action.target_count; ++i)
        {
            const auto& target = action.targets[i];
            const auto spell = action.category == 4 ? action.parameter : 0;
            for (size_t j = 0; j < target.result_count; ++j)
            {
                const auto& result = target.results[j];
                auto seconds = XIUIStatusSeconds(action.category, action.parameter, result.parameter, result.message);
                if (action.actor == this->player_ && spell != 0) seconds = AdjustSongDuration(seconds, spell, this->songs_);
                this->ApplyStatus(target.id, result.parameter, spell, result.message, now, seconds);
                this->ApplyStatus(result.additional_message == 166 ? action.actor : target.id, result.additional_parameter, 0, result.additional_message, now);
                this->ApplyStatus(action.actor, result.spikes_parameter, 0, result.spikes_message, now);
                if (result.message == 6 || result.message == 20) this->actors_.erase(target.id);
            }
        }
    }

    auto combat_source::Prune(const uint64_t now) -> void
    {
        for (auto& [actorId, actor] : this->actors_)
        {
            (void)actorId;
            if (this->rules_ == server_rules::phoenix)
            {
                for (auto& status : actor.statuses)
                {
                    if (status.estimated && status.expires != 0 && now >= status.expires)
                    {
                        status.expires = 0;
                        status.uncertain = true;
                    }
                }
            }
            std::erase_if(actor.statuses, [now](const auto& status) { return status.expires != 0 && now >= status.expires; });
            if (!actor.cast.Active(now)) actor.cast = {};
            if (now >= actor.targeted && now - actor.targeted > 30000) actor.target = 0;
        }
        std::erase_if(this->actors_, [now](const auto& entry) { return entry.second.statuses.empty() && entry.second.cast.name.empty() && now - entry.second.updated > 300000; });
    }

    auto combat_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        if (player == nullptr || party == nullptr || entity == nullptr || player->GetIsZoning() || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) || party->GetMemberServerId(0) == 0)
        { this->Clear(); return; }
        const auto id = party->GetMemberServerId(0);
        const auto zone = party->GetMemberZone(0);
        if (this->player_ != id || this->zone_ != zone) { this->Clear(); this->player_ = id; this->zone_ = zone; }
        if (now < this->next_update_) return;
        this->next_update_ = now + 100;
        this->Prune(now);
        for (auto& [actorId, actor] : this->actors_)
        {
            (void)actorId;
            if (actor.index == 0) actor.index = FindEntityIndex(*core, actor.id);
        }
        std::erase_if(this->actors_, [&](const auto& entry) {
            const auto& actor = entry.second;
            return actor.index == 0 || actor.index >= entity->GetEntityMapSize() || entity->GetServerId(actor.index) != actor.id ||
                ((entity->GetSpawnFlags(actor.index) & 0x10) != 0 && entity->GetHPPercent(actor.index) == 0);
        });
    }

    auto combat_source::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> void
    {
        const combat_action_packet local{data};

        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (core == nullptr) return;
        this->Update(core, now);
        if (this->player_ == 0) return;
        if (id == 0x28)
        {
            if (const auto& action = (decoded != nullptr ? *decoded : local).Action()) this->Observe(*action, core->GetResourceManager(), now);
        }
        else if (id == 0x29 && data.size() >= 0x1C)
        {
            uint32_t actor{}, target{}, parameter{}; uint16_t message{};
            std::memcpy(&actor, data.data() + 4, 4); std::memcpy(&target, data.data() + 8, 4);
            std::memcpy(&parameter, data.data() + 12, 4); std::memcpy(&message, data.data() + 24, 2);
            message &= 0x7FFF;
            if (message == 6 || message == 20) this->actors_.erase(target);
            else this->ApplyStatus(target != 0 ? target : actor, parameter, 0, message, now);
        }
    }
}
