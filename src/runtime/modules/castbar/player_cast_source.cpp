#include "player_cast_source.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>

namespace xiui
{
    auto player_cast_state::Begin(std::string label, uint32_t actionId, bool isItem, uint64_t now, native_cast_sample baseline) -> void
    {
        this->Clear();
        this->name = std::move(label); this->id = actionId; this->item = isItem;
        this->started = this->changed = now; this->previous = baseline;
    }

    auto player_cast_state::Update(const native_cast_sample sample, const uint64_t now) -> void
    {
        if (!this->Active()) return;
        if (this->interrupted != 0)
        {
            if (now - this->interrupted >= 700) this->Clear();
            return;
        }
        if (now - this->started > 120000) { this->Clear(); return; }
        const bool valid = sample.Valid() && sample.type == (this->item ? 1u : 0u);
        const bool moved = valid && (sample.percent != this->previous.percent || sample.count != this->previous.count || sample.maximum != this->previous.maximum);
        if (valid && !this->sampled && sample.count > 0 && sample.percent < 1 && (moved || sample.percent == 0)) this->sampled = true;
        if (this->sampled && valid)
        {
            if (sample.percent >= 1 || sample.count <= 0) { this->Clear(); return; }
            this->progress = sample.percent;
            if (moved) this->changed = now;
        }
        this->previous = sample;
        if (now - this->changed >= 1500) this->Clear();
    }

    auto player_cast_source::Update(IAshitaCore* core, uint64_t now) -> void
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* bar = memory != nullptr ? memory->GetCastBar() : nullptr;
        if (player == nullptr || party == nullptr || player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) == 0 || party->GetMemberZone(0) == 0 || party->GetMemberHP(0) == 0)
        { this->Clear(); return; }
        const auto id = party->GetMemberServerId(0);
        const auto zone = party->GetMemberZone(0);
        if (id != this->player_ || zone != this->zone_) { this->Clear(); this->player_ = id; this->zone_ = zone; }
        this->available_ = bar != nullptr;
        this->native_ = bar != nullptr ? native_cast_sample{bar->GetPercent(), bar->GetMax(), bar->GetCount(), bar->GetCastType()} : native_cast_sample{};
        this->state_.Update(this->native_, now);
    }

    auto player_cast_source::Observe(const combat_action& action, uint32_t player, IResourceManager* resources, uint64_t now) -> void
    {
        if (player == 0 || action.actor != player || action.target_count == 0 || action.target_count > action.targets.size()) return;
        if (action.category != 8 && action.category != 9 && action.category != 4 && action.category != 5) return;
        const auto message = action.targets[0].message;
        if ((action.parameter & 0xFFFF) == 0x7073 || message == 16 || message == 84 || message == 106)
        {
            if (this->state_.Active()) this->state_.interrupted = now;
            return;
        }
        if (action.category == 4 || action.category == 5) { this->state_.Clear(); return; }
        if ((action.parameter & 0xFFFF) != 0x6163) return;
        const auto id = action.targets[0].parameter;
        const bool item = action.category == 9;
        const char* name{};
        if (resources != nullptr)
        {
            if (item)
            {
                if (const auto* resource = resources->GetItemById(id)) name = resource->Name[2] != nullptr ? resource->Name[2] : resource->Name[0];
            }
            else if (const auto* resource = resources->GetSpellById(id)) name = resource->Name[2] != nullptr ? resource->Name[2] : resource->Name[0];
        }
        this->state_.Begin(name != nullptr ? FromGameText(name) : std::string{item ? "Item " : "Spell "} + std::to_string(id), id, item, now, this->native_);
        this->state_.target_id = action.targets[0].id;
    }

    auto player_cast_source::HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void
    {
        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (id != 0x28) return;
        const auto action = DecodeCombatAction(data);
        if (!action) return;
        this->Update(core, now);
        this->Observe(*action, this->player_, core != nullptr ? core->GetResourceManager() : nullptr, now);
    }
}
