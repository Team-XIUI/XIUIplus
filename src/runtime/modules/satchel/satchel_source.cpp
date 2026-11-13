#include "satchel_source.hpp"
#include "ashita_compat.hpp"
#include "satchel_identity.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace xiui
{
    namespace
    {
        auto SameItem(const satchel_item& left, const satchel_item& right) -> bool
        {
            return left.id == right.id && left.bag == right.bag && left.slot == right.slot && left.flags == right.flags &&
                left.count == right.count && left.price == right.price && left.equipped == right.equipped &&
                left.read_only == right.read_only && left.extra == right.extra;
        }

        auto SameBag(const satchel_bag& left, const satchel_bag& right) -> bool
        {
            return left.capacity == right.capacity && left.ready == right.ready && left.accessible == right.accessible && left.items == right.items;
        }
    }

    auto SatchelBagAccessible(const uint8_t bag, const bool inMogHouse) -> bool
    {
        if (bag >= satchel_bag_names.size()) return false;
        return inMogHouse || (bag != 1 && bag != 2 && bag != 4 && bag != 9);
    }

    auto satchel_source::ReadBag(const IInventory& inventory, const uint8_t bag, const bool ready, const bool accessible) -> satchel_bag
    {
        satchel_bag result;
        if (bag >= satchel_bag_names.size()) return result;
        const auto capacity = inventory.GetContainerCountMax(bag);
        if (capacity == 0 || capacity > 80) return result;
        result.capacity = capacity;
        result.ready = ready && (inventory.GetContainerUpdateFlags() & (1u << bag)) != 0;
        result.accessible = result.ready && accessible;
        if (!result.ready) return result;
        for (uint32_t slot = 1; slot <= capacity; ++slot)
        {
            const auto* item = inventory.GetContainerItem(bag, slot);
            if (item == nullptr) { result.ready = result.accessible = false; result.items.clear(); return result; }
            if (item->Id == 0 || item->Id == 0xFFFF || item->Count == 0) continue;
            satchel_item next;
            next.id = item->Id; next.bag = bag; next.slot = static_cast<uint8_t>(slot);
            next.flags = item->Flags; next.count = item->Count; next.price = item->Price;
            static_assert(sizeof(item->Extra) == sizeof(next.extra));
            std::memcpy(next.extra.data(), item->Extra, next.extra.size());
            const auto index = (static_cast<uint32_t>(bag) << 8) | slot;
            for (uint32_t equip = 0; equip < 16; ++equip)
            {
                const auto* entry = inventory.GetEquippedItem(equip);
                if (entry != nullptr && entry->Index == index) { next.equipped = true; break; }
            }
            result.items.push_back(next);
        }
        if (result.items.size() != inventory.GetContainerCount(bag))
        {
            result.ready = result.accessible = false;
            result.items.clear();
        }
        return result;
    }

    auto satchel_source::Publish(satchel_snapshot next) -> void
    {
        bool changed = next.identity != this->snapshot_.identity || next.name != this->snapshot_.name ||
            next.character_id != this->snapshot_.character_id || next.gil != this->snapshot_.gil || next.zone != this->snapshot_.zone ||
            next.ready != this->snapshot_.ready || next.read_only != this->snapshot_.read_only;
        for (size_t bag = 0; bag < next.bags.size(); ++bag)
        {
            for (auto& item : next.bags[bag].items)
            {
                const auto* previous = SatchelFind(this->snapshot_, item.bag, item.slot);
                item.generation = previous != nullptr && SameItem(item, *previous) ? previous->generation : ++this->generation_;
            }
            changed = changed || !SameBag(next.bags[bag], this->snapshot_.bags[bag]);
        }
        next.generation = changed ? ++this->generation_ : this->snapshot_.generation;
        this->snapshot_ = std::move(next);
    }

    auto satchel_source::Invalidate(void) -> void
    {
        auto next = this->snapshot_;
        next.ready = false;
        for (auto& bag : next.bags) { bag.ready = false; bag.accessible = false; }
        this->Publish(std::move(next));
        this->next_scan_ = 0;
    }

    auto satchel_source::Reset(void) -> void
    {
        this->Publish({});
        this->next_scan_ = 0;
        this->syncing_ = false;
        this->in_mog_house_ = false;
        this->next_identity_scan_ = 0;
        this->server_character_ = 0;
        this->server_identity_.reset();
    }

    auto satchel_source::HandlePacket(const uint16_t id, const std::span<const uint8_t> data) -> void
    {
        if (id == 0x000A || id == 0x000B)
        {
            this->Reset();
            this->syncing_ = true;
            if (id == 0x000A && data.size() > 0x80) this->in_mog_house_ = data[0x80] == 1;
        }
        else if (id == 0x001D && data.size() >= 12 && data[4] <= 1)
        {
            this->syncing_ = data[4] == 0;
            if (this->syncing_) this->Invalidate();
            this->next_scan_ = 0;
        }
        else if ((id == 0x0096 || id == 0x0097) && data.size() >= 4)
        {
            this->in_mog_house_ = id == 0x0096;
            this->next_scan_ = 0;
        }
        else if (id == 0x001E || id == 0x001F || id == 0x0020 || id == 0x0050)
            this->next_scan_ = 0;
    }

    auto satchel_source::Update(IAshitaCore* core, const inventory_state& inventoryState, const std::optional<std::string>& identity, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        if (inventory == nullptr || player == nullptr || party == nullptr || player->GetIsZoning() || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) == 0 || party->GetMemberZone(0) == 0)
        {
            this->Publish({});
            this->next_scan_ = 0;
            return;
        }
        const auto characterId = party->GetMemberServerId(0);
        const auto zone = party->GetMemberZone(0);
        auto resolved = identity.value_or("");
        if (!SatchelStableIdentity(resolved, characterId))
        {
            // The connection cannot change within a zone; Reset() re-resolves it after zoning.
            if (this->server_character_ != characterId || (!this->server_identity_ && now >= this->next_identity_scan_))
            {
                if (this->server_character_ != characterId) this->identity_retry_ = 1000;
                this->server_identity_ = ReadSatchelServerIdentity(characterId);
                this->server_character_ = characterId;
                this->next_identity_scan_ = now + this->identity_retry_;
                this->identity_retry_ = std::min<uint64_t>(this->identity_retry_ * 2, 30000);
            }
            if (this->server_identity_) resolved = *this->server_identity_;
        }
        if (this->snapshot_.character_id != 0 && (characterId != this->snapshot_.character_id || zone != this->snapshot_.zone ||
            resolved != this->snapshot_.identity))
        {
            this->Publish({});
            this->next_scan_ = 0;
        }
        if (this->syncing_ || !inventoryState.available)
        {
            this->Invalidate();
            return;
        }
        if (now < this->next_scan_) return;
        satchel_snapshot next;
        next.identity = std::move(resolved);
        const auto* name = party->GetMemberName(0);
        if (name != nullptr) next.name = FromGameText(std::string{name, strnlen_s(name, 16)});
        next.character_id = characterId; next.zone = zone; next.observed = now;
        next.ready = true;
        for (uint8_t bag = 0; bag < next.bags.size(); ++bag)
        {
            next.bags[bag] = ReadBag(*inventory, bag, inventoryState.bags[bag].ready, SatchelBagAccessible(bag, this->in_mog_house_));
            if (next.bags[bag].ready) SatchelApplyNativeOrder(next.bags[bag].items, this->native_order_.Read(bag));
            const auto nativeCapacity = inventory->GetContainerCountMax(bag);
            if (bag != 3 && nativeCapacity != 0) next.ready = next.ready && next.bags[bag].ready;
            if (!next.bags[bag].ready && this->snapshot_.character_id == characterId)
                next.bags[bag].items = this->snapshot_.bags[bag].items;
        }
        const auto* gil = inventory->GetContainerItem(0, 0);
        const bool gilReady = next.bags[0].ready && gil != nullptr && gil->Id == 0xFFFF;
        next.ready = next.ready && gilReady;
        next.gil = gilReady ? gil->Count : this->snapshot_.gil;
        this->Publish(std::move(next));
        this->next_scan_ = now + 100;
    }
}
