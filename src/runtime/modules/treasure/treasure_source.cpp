#include "treasure_source.hpp"
#include "ashita_compat.hpp"
#include "chat/chat_encoding.hpp"
#include <algorithm>
#include <cstring>

namespace
{
    template<typename T> auto Field(std::span<const uint8_t> bytes, size_t offset) -> T
    { T value{}; if (offset + sizeof(T) <= bytes.size()) std::memcpy(&value, bytes.data() + offset, sizeof(T)); return value; }
    auto Name(std::span<const uint8_t> bytes, size_t offset) -> std::string
    {
        if (offset + 16 > bytes.size()) return {};
        const auto* value = reinterpret_cast<const char*>(bytes.data() + offset);
        return xiui::FromGameText({value, strnlen(value, 16)});
    }
    auto SendDecision(IAshitaCore* core, const xiui::treasure_action& request) -> void
    {
        std::array<uint8_t, 8> bytes{};
        bytes[4] = request.slot;
        core->GetPacketManager()->AddOutgoingPacket(request.pass ? 0x42 : 0x41, static_cast<uint32_t>(bytes.size()), bytes.data());
    }
}

namespace xiui
{
    auto treasure_source::Clear(void) -> void
    {
        this->entries_ = {}; this->drops_ = {}; this->applied_ = {}; this->missing_until_ = {};
        this->player_ = 0; this->ready_ = this->clock_known_ = false; this->next_update_ = 0; this->error_.clear();
    }
    auto treasure_source::Count(void) const -> size_t
    { return static_cast<size_t>(std::count_if(this->entries_.begin(), this->entries_.end(), [](const auto& item) { return item.item != 0; })); }

    auto treasure_source::MemberLot(const size_t slot, const uint64_t generation, const uint32_t member) const -> std::optional<uint16_t>
    {
        if (!this->ready_ || slot >= capacity || member == 0 || this->missing_until_[slot] != 0) return {};
        const auto& entry = this->entries_[slot];
        if (entry.item == 0 || entry.generation != generation) return {};
        for (const auto& value : entry.members)
            if (value.id == member && (value.lot <= 999 || value.lot == 65535)) return value.lot;
        return {};
    }

    auto treasure_source::Reconcile(const std::array<treasure_snapshot, capacity>& slots, const bool ready, const uint32_t player, const uint64_t now) -> void
    {
        if (player != 0 && this->player_ != 0 && player != this->player_) this->Clear();
        if (player != 0) this->player_ = player;
        this->ready_ = ready && player != 0;
        if (!this->ready_) return;
        for (size_t slot = 0; slot < capacity; ++slot)
        {
            const auto& sample = slots[slot]; auto& entry = this->entries_[slot]; auto& drop = this->drops_[slot];
            if (sample.item == 0 || sample.item >= 65535)
            {
                if (entry.item == 0) continue;
                // Preserve presentation through a short native refresh, but suspend decisions immediately.
                auto& missing = this->missing_until_[slot];
                if (missing == 0) missing = now + 300;
                if (now >= missing)
                {
                    entry = {}; missing = 0;
                    if (drop.serial == this->applied_[slot]) this->drops_[slot] = {};
                    this->applied_[slot] = 0;
                }
                continue;
            }
            this->missing_until_[slot] = 0;
            const bool matchingDrop = drop.item == sample.item && (sample.drop == 0 || drop.start == sample.drop);
            if (!matchingDrop && drop.serial == this->applied_[slot] && (sample.item != entry.item || sample.drop != entry.drop)) drop = {};
            if (sample.item != entry.item || sample.drop != entry.drop || (matchingDrop && drop.serial != this->applied_[slot]))
            {
                entry = {}; entry.generation = ++this->sequence_;
                if (matchingDrop) entry.expires = drop.expires;
                this->applied_[slot] = matchingDrop ? drop.serial : 0;
            }
            if (entry.lot != sample.lot) entry.pending_until = 0;
            static_cast<treasure_snapshot&>(entry) = sample;
            if (entry.expires != 0 && now >= entry.expires) entry.expires = 0;
        }
    }

    auto treasure_source::Update(IAshitaCore* core, const uint64_t now, const bool force) -> void
    {
        if (!force && now < this->next_update_) return;
        this->next_update_ = now + 150;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        if (player == nullptr || inventory == nullptr || party == nullptr || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        { this->ready_ = false; return; }
        std::array<treasure_snapshot, capacity> samples{};
        std::array<uint32_t, 18> members{};
        const auto localId = party->GetMemberServerId(0);
        const auto localZone = party->GetMemberZone(0);
        const auto entities = xiui::ashita::View(memory->GetEntity());
        for (uint32_t member = 0; member < members.size(); ++member)
        {
            const auto id = party->GetMemberServerId(member);
            if (!id || !party->GetMemberIsActive(member) || party->GetMemberZone(member) != localZone) continue;
            const auto index = party->GetMemberTargetIndex(member);
            if (member != 0 && (entities == nullptr || index == 0 || index >= entities->GetEntityMapSize() ||
                entities->GetServerId(index) != id || (entities->GetSpawnFlags(index) & 1) == 0)) continue;
            members[member] = id;
        }
        // Native pool status can lag individual slots during drops and distribution.
        for (size_t slot = 0; slot < capacity; ++slot)
            if (const auto* item = inventory->GetTreasurePoolItem(static_cast<uint32_t>(slot)); item != nullptr && item->ItemId > 0 && item->ItemId < 65535)
            {
                auto& sample = samples[slot]; sample.item = item->ItemId; sample.count = std::max(1u, item->Count); sample.drop = item->DropTime;
                sample.lot = item->Lot; sample.winning_lot = item->WinningLot <= 999 ? item->WinningLot : 0;
                const auto* name = reinterpret_cast<const char*>(item->WinningEntityName);
                sample.winner = FromGameText({name, strnlen(name, 16)});
                for (uint32_t member = 0; member < members.size(); ++member)
                    if (members[member] != 0)
                        sample.members[member] = {members[member], member == 0 ? sample.lot : party->GetMemberTreasureLot(member, static_cast<uint32_t>(slot))};
            }
        this->Reconcile(samples, true, localId, now);
    }

    auto treasure_source::HandlePacket(const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (id == 0xB) { this->Clear(); return; }
        if (id == 0xA) { this->BeginZone(); return; }
        if (id == 0xD2)
        {
            if (data.size() < 28) return;
            const auto item = Field<uint16_t>(data, 16); const auto slot = data[20]; const auto start = Field<uint32_t>(data, 24);
            if (slot >= capacity) return;
            this->next_update_ = 0;
            if (item == 0 || item == 65535) { this->drops_[slot] = {}; return; }
            const bool fresh = data[21] == 0;
            auto& event = this->drops_[slot];
            if (fresh && event.item == item && event.start == start) return;
            if (fresh && start != 0) { this->clock_known_ = true; this->clock_start_ = start; this->clock_at_ = now; }
            uint64_t expires{};
            if (fresh) expires = now + 300000;
            else if (this->clock_known_ && start != 0 && now >= this->clock_at_)
            {
                const uint32_t age = this->clock_start_ + static_cast<uint32_t>(now - this->clock_at_) - start;
                if (age < 300000) expires = now + 300000 - age;
            }
            if (event.item == item && event.start == start) { if (event.expires == 0) event.expires = expires; return; }
            event = {item, start, ++this->sequence_, expires};
            return;
        }
        if (id != 0xD3 || data.size() < 54) return;
        const auto slot = data[20], resolution = data[21];
        if (slot >= capacity || resolution > 3) return;
        this->next_update_ = 0;
        // Outcomes have no item identity. Let native slots confirm removal or replacement.
        if (resolution != 0) { this->drops_[slot] = {}; return; }
        auto& entry = this->entries_[slot];
        if (entry.item == 0 || (this->drops_[slot].serial != 0 && this->drops_[slot].serial != this->applied_[slot])) return;
        const auto actor = Field<uint32_t>(data, 8);
        const auto lot = Field<uint16_t>(data, 18), winning = Field<uint16_t>(data, 14);
        for (auto& member : entry.members)
            if (member.id != 0 && member.id == actor) { member.lot = lot; break; }
        if (actor == this->player_) { entry.lot = lot; entry.pending_until = 0; }
        entry.winning_lot = winning <= 999 ? winning : 0; entry.winner = Name(data, 22);
        const auto name = Name(data, 38);
        if (!name.empty())
        {
            entry.history.push_back(name + (lot == 65535 ? ": passed" : ": " + std::to_string(lot)));
            if (entry.history.size() > 18) entry.history.erase(entry.history.begin());
        }
    }

    auto treasure_source::Plan(const size_t slot, const uint64_t generation, const bool pass, const uint64_t now) const -> std::optional<treasure_action>
    {
        if (!this->ready_ || slot >= capacity) return {};
        const auto& item = this->entries_[slot];
        if (item.item == 0 || item.generation != generation || this->missing_until_[slot] != 0 ||
            (this->drops_[slot].serial != 0 && this->drops_[slot].serial != this->applied_[slot]) ||
            item.pending_until > now || item.lot == 65535 || (!pass && item.lot != 0)) return {};
        return treasure_action{static_cast<uint8_t>(slot), item.item, item.drop, generation, pass};
    }
    auto treasure_source::CaptureBatch(const bool pass, const uint64_t now) const -> std::vector<treasure_action>
    {
        std::vector<treasure_action> actions;
        actions.reserve(capacity);
        for (size_t slot = 0; slot < capacity; ++slot)
        {
            if (const auto action = this->Plan(slot, this->entries_[slot].generation, pass, now))
                actions.push_back(*action);
        }
        return actions;
    }
    auto treasure_source::Queue(const treasure_action& action, const uint64_t now, const std::function<void(const treasure_action&)>& send) -> bool
    {
        const auto current = this->Plan(action.slot, action.generation, action.pass, now);
        if (!current || current->item != action.item || current->drop != action.drop || !send) return false;
        this->entries_[action.slot].pending_until = now + 2000;
        send(action);
        return true;
    }
    auto treasure_source::QueueBatch(const std::span<const treasure_action> actions, const uint64_t now,
        const std::function<void(const treasure_action&)>& send) -> size_t
    {
        size_t sent{};
        for (const auto& action : actions)
            sent += this->Queue(action, now, send) ? 1 : 0;
        return sent;
    }
    auto treasure_source::PrepareActions(IAshitaCore* core, const uint64_t now) -> bool
    {
        this->error_.clear();
        this->Update(core, now, true);
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        auto* chat = core != nullptr ? core->GetChatManager() : nullptr;
        auto* packets = core != nullptr ? core->GetPacketManager() : nullptr;
        auto* properties = core != nullptr ? core->GetProperties() : nullptr;
        if (player == nullptr || player->GetIsZoning() || chat == nullptr || chat->IsInputOpen() || packets == nullptr ||
            (target != nullptr && (target->GetIsMenuOpen() || target->GetActionTargetActive() || target->GetIsSubTargetActive())) ||
            (properties != nullptr && ::GetForegroundWindow() != properties->GetFinalFantasyHwnd()))
        {
            this->error_ = "Finish the current game interaction first.";
            return false;
        }
        return true;
    }
    auto treasure_source::Act(IAshitaCore* core, const size_t slot, const uint64_t generation, const bool pass, const uint64_t now) -> bool
    {
        if (!this->PrepareActions(core, now)) return false;
        const auto action = this->Plan(slot, generation, pass, now);
        if (!action)
        {
            this->error_ = "That item changed, has a decision, or is awaiting a response.";
            return false;
        }
        return this->Queue(*action, now, [&](const auto& request) { SendDecision(core, request); });
    }
    auto treasure_source::ActBatch(IAshitaCore* core, const std::span<const treasure_action> actions, const uint64_t now) -> size_t
    {
        if (!this->PrepareActions(core, now)) return 0;
        return this->QueueBatch(actions, now, [&](const auto& request) { SendDecision(core, request); });
    }
}
