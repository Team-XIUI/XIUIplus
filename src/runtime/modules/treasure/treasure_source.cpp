#include "treasure_source.hpp"
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
}

namespace xiui
{
    auto treasure_source::Clear(void) -> void
    {
        this->entries_ = {}; this->drops_ = {}; this->applied_ = {}; this->resolved_ = {};
        this->player_ = 0; this->ready_ = this->clock_known_ = false; this->next_update_ = 0; this->error_.clear();
    }
    auto treasure_source::Count(void) const -> size_t
    { return static_cast<size_t>(std::count_if(this->entries_.begin(), this->entries_.end(), [](const auto& item) { return item.item != 0; })); }

    auto treasure_source::Reconcile(const std::array<treasure_snapshot, capacity>& slots, const bool ready, const uint32_t player, const uint64_t now) -> void
    {
        if (player != 0 && this->player_ != 0 && player != this->player_) this->Clear();
        this->player_ = player; this->ready_ = ready && player != 0;
        if (!this->ready_) return;
        for (size_t slot = 0; slot < capacity; ++slot)
        {
            const auto& sample = slots[slot]; auto& entry = this->entries_[slot]; const auto& drop = this->drops_[slot];
            if (sample.item == 0 || sample.item >= 65535) { entry = {}; this->resolved_[slot] = {}; continue; }
            if (this->resolved_[slot] == std::pair{sample.item, sample.drop}) { entry = {}; continue; }
            if (sample.item != entry.item || sample.drop != entry.drop || (drop.item == sample.item && drop.serial != this->applied_[slot]))
            {
                entry = {}; entry.generation = ++this->sequence_;
                if (drop.item == sample.item && (sample.drop == 0 || drop.start == sample.drop)) entry.expires = drop.expires;
                this->applied_[slot] = drop.serial;
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
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        if (player == nullptr || inventory == nullptr || party == nullptr || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        { this->ready_ = false; return; }
        const auto status = inventory->GetTreasurePoolStatus();
        std::array<treasure_snapshot, capacity> samples{};
        if (status == 1)
            for (size_t slot = 0; slot < capacity; ++slot)
                if (const auto* item = inventory->GetTreasurePoolItem(static_cast<uint32_t>(slot)); item != nullptr && item->ItemId > 0 && item->ItemId < 65535)
                {
                    auto& sample = samples[slot]; sample.item = item->ItemId; sample.count = std::max(1u, item->Count); sample.drop = item->DropTime;
                    sample.lot = item->Lot; sample.winning_lot = item->WinningLot <= 999 ? item->WinningLot : 0;
                    const auto* name = reinterpret_cast<const char*>(item->WinningEntityName);
                    sample.winner = FromGameText({name, strnlen(name, 16)});
                }
        this->Reconcile(samples, status == 1 || status == 2, party->GetMemberServerId(0), now);
    }

    auto treasure_source::HandlePacket(const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (id == 0xB) { this->Clear(); return; }
        if (id == 0xA) { this->BeginZone(); return; }
        if (id == 0xD2)
        {
            if (data.size() < 28) return;
            const auto item = Field<uint16_t>(data, 16); const auto slot = data[20]; const auto start = Field<uint32_t>(data, 24);
            if (slot >= capacity || item == 0 || item == 65535) return;
            const bool fresh = data[21] == 0;
            auto& event = this->drops_[slot];
            if (fresh && start != 0 && event.item == item && event.start == start) return;
            if (fresh && start != 0) { this->clock_known_ = true; this->clock_start_ = start; this->clock_at_ = now; }
            uint64_t expires{};
            if (fresh) expires = now + 300000;
            else if (this->clock_known_ && start != 0 && now >= this->clock_at_)
            {
                const uint32_t age = this->clock_start_ + static_cast<uint32_t>(now - this->clock_at_) - start;
                if (age < 300000) expires = now + 300000 - age;
            }
            if (event.item == item && event.start == start && start != 0) { if (event.expires == 0) event.expires = expires; return; }
            event = {item, start, ++this->sequence_, expires}; this->resolved_[slot] = {};
            return;
        }
        if (id != 0xD3 || data.size() < 54) return;
        const auto slot = data[20], resolution = data[21];
        if (slot >= capacity || resolution > 3 || this->entries_[slot].item == 0) return;
        auto& entry = this->entries_[slot];
        if (resolution != 0)
        { this->resolved_[slot] = {entry.item, entry.drop}; entry = {}; return; }
        const auto actor = Field<uint32_t>(data, 8);
        const auto lot = Field<uint16_t>(data, 18), winning = Field<uint16_t>(data, 14);
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
        if (item.item == 0 || item.generation != generation || item.pending_until > now || item.lot == 65535 || (!pass && item.lot != 0)) return {};
        return treasure_action{static_cast<uint8_t>(slot), item.item, item.drop, generation, pass};
    }
    auto treasure_source::Queue(const treasure_action& action, const uint64_t now, const std::function<void(const treasure_action&)>& send) -> bool
    {
        const auto current = this->Plan(action.slot, action.generation, action.pass, now);
        if (!current || current->item != action.item || current->drop != action.drop || !send) return false;
        this->entries_[action.slot].pending_until = now + 2000;
        send(action); return true;
    }
    auto treasure_source::Act(IAshitaCore* core, const size_t slot, const uint64_t generation, const bool pass, const uint64_t now) -> bool
    {
        this->error_.clear(); this->Update(core, now, true);
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        auto* chat = core != nullptr ? core->GetChatManager() : nullptr;
        auto* packets = core != nullptr ? core->GetPacketManager() : nullptr;
        auto* properties = core != nullptr ? core->GetProperties() : nullptr;
        if (player == nullptr || player->GetIsZoning() || chat == nullptr || chat->IsInputOpen() || packets == nullptr ||
            (target != nullptr && (target->GetIsMenuOpen() || target->GetActionTargetActive() || target->GetIsSubTargetActive())) ||
            (properties != nullptr && ::GetForegroundWindow() != properties->GetFinalFantasyHwnd()))
        { this->error_ = "Finish the current game interaction first."; return false; }
        const auto action = this->Plan(slot, generation, pass, now);
        if (!action) { this->error_ = "That item changed, has a decision, or is awaiting a response."; return false; }
        return this->Queue(*action, now, [&](const auto& request) {
            std::array<uint8_t, 8> bytes{}; bytes[4] = request.slot;
            packets->AddOutgoingPacket(request.pass ? 0x42 : 0x41, static_cast<uint32_t>(bytes.size()), bytes.data());
        });
    }
}
