#include "satchel_actions.hpp"
#include "Ashita.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
    auto Wardrobe(const uint8_t bag) -> bool { return bag == 8 || (bag >= 10 && bag <= 16); }
    auto Gear(const uint16_t type) -> bool { return type == 4 || type == 5; }
    auto SameItem(const xiui::satchel_item& a, const xiui::satchel_item& b) -> bool
    {
        return a.id == b.id && a.extra == b.extra;
    }
    auto ActionName(const xiui::satchel_action action) -> const char*
    {
        switch (action)
        {
        case xiui::satchel_action::move: return "Move";
        case xiui::satchel_action::split: return "Split";
        case xiui::satchel_action::use: return "Use";
        case xiui::satchel_action::equip: return "Equip";
        case xiui::satchel_action::drop: return "Drop";
        case xiui::satchel_action::bazaar: return "Bazaar price";
        case xiui::satchel_action::sort: return "Sort";
        }
        return "Action";
    }
    auto LiveGuard(IAshitaCore* core, const xiui::satchel_snapshot& snapshot) -> std::string
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (player == nullptr || party == nullptr || entity == nullptr || target == nullptr ||
            memory->GetInventory() == nullptr || core->GetPacketManager() == nullptr)
            return "Inventory services are unavailable.";
        if (player->GetIsZoning() || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
            return "Wait until your character has finished loading.";
        const auto index = party->GetMemberTargetIndex(0);
        if (snapshot.character_id == 0 || party->GetMemberServerId(0) != snapshot.character_id ||
            snapshot.zone == 0 || party->GetMemberZone(0) != snapshot.zone || index == 0 ||
            index >= entity->GetEntityMapSize() || entity->GetServerId(index) != snapshot.character_id)
            return "Your character or zone changed; select the item again.";
        if (target->GetIsMenuOpen() || target->GetActionTargetActive() || target->GetIsSubTargetActive() || entity->GetStatus(index) == 4)
            return "Close the native menu or finish the current action first.";
        return {};
    }
    auto Equipped(const IInventory& inventory, const uint8_t bag, const uint8_t slot) -> bool
    {
        const auto index = static_cast<uint32_t>(bag) << 8 | slot;
        for (uint32_t i = 0; i < 16; ++i)
        {
            const auto* entry = inventory.GetEquippedItem(i);
            if (entry != nullptr && entry->Index == index) return true;
        }
        return false;
    }
    auto ReadBag(const IInventory& inventory, xiui::satchel_snapshot& snapshot, const uint8_t bag) -> bool
    {
        if (bag >= snapshot.bags.size()) return false;
        auto& result = snapshot.bags[bag];
        const auto capacity = inventory.GetContainerCountMax(bag);
        if ((inventory.GetContainerUpdateFlags() & (1u << bag)) == 0 || capacity == 0 || capacity > 80)
            return false;
        auto previous = std::move(result.items);
        result.items.clear();
        result.capacity = capacity;
        for (uint32_t slot = 1; slot <= capacity; ++slot)
        {
            const auto* native = inventory.GetContainerItem(bag, slot);
            if (native == nullptr) return false;
            if (native->Id == 0 || native->Id == 0xFFFF || native->Count == 0) continue;
            if (native->Index != slot) return false;
            xiui::satchel_item item{};
            item.id = native->Id; item.bag = bag; item.slot = static_cast<uint8_t>(slot);
            item.flags = native->Flags; item.count = native->Count; item.price = native->Price;
            item.equipped = Equipped(inventory, bag, item.slot);
            std::copy_n(native->Extra, item.extra.size(), item.extra.begin());
            for (const auto& cached : previous)
                if (cached.slot == item.slot) { item.generation = cached.generation; break; }
            result.items.push_back(item);
        }
        result.ready = true;
        return true;
    }
    auto Total(const xiui::satchel_bag& bag, const xiui::satchel_item& item) -> uint64_t
    {
        uint64_t total{};
        for (const auto& candidate : bag.items) if (SameItem(candidate, item)) total += candidate.count;
        return total;
    }
    template<size_t N> auto Send(IPacketManager& packets, const uint16_t id, std::array<uint8_t, N> bytes) -> void
    {
        packets.AddOutgoingPacket(id, static_cast<uint32_t>(bytes.size()), bytes.data());
    }
    template<size_t N> auto Put32(std::array<uint8_t, N>& bytes, const size_t offset, const uint32_t value) -> void
    {
        for (size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

namespace xiui
{
    auto satchel_actions::SnapshotEligibility(const satchel_snapshot& snapshot, const satchel_action_request& request) -> std::string
    {
        if (snapshot.read_only || request.item.read_only) return "Offline inventories and stored slip contents are read only.";
        if (!snapshot.ready || snapshot.identity.empty()) return "Live inventory is still loading.";
        if (request.identity != snapshot.identity) return "Your character changed; select the item again.";
        if (request.item.bag >= snapshot.bags.size()) return "That bag is unavailable.";
        const auto& bag = snapshot.bags[request.item.bag];
        if (!bag.ready || !bag.accessible || bag.capacity == 0 || bag.capacity > 80) return "That bag is not accessible here.";
        if (request.action == satchel_action::sort) return {};
        if (request.item.id == 0 || request.item.id == 0xFFFF || request.item.slot == 0 || request.item.slot > bag.capacity)
            return "Select an occupied inventory slot.";
        const auto* current = SatchelFind(snapshot, request.item.bag, request.item.slot);
        if (current == nullptr || *current != request.item) return "That inventory slot changed; select the item again.";
        return {};
    }

    auto satchel_actions::Plan(const satchel_snapshot& snapshot, const satchel_action_request& request, const resource& item, plan& result) -> std::string
    {
        if (const auto reason = SnapshotEligibility(snapshot, request); !reason.empty()) return reason;
        result = {};
        result.quantity = request.quantity != 0 ? request.quantity : request.item.count;
        if (request.action == satchel_action::sort) return {};
        const auto& source = request.item;
        const bool inBazaar = source.price != 0 || source.flags == 2;
        const bool locked = source.flags != 0;
        if (request.action != satchel_action::bazaar && inBazaar) return "Unlist this item from your bazaar first.";
        if (request.action != satchel_action::use && source.equipped) return "Unequip this item first.";
        if (locked && !(request.action == satchel_action::use && source.equipped && source.flags == 1) &&
            !(request.action == satchel_action::bazaar && inBazaar && source.flags == 2))
            return "This item is locked or reserved by the game.";
        switch (request.action)
        {
        case satchel_action::move:
        case satchel_action::split:
        {
            const auto destination = request.action == satchel_action::split ? source.bag : request.destination;
            if (destination >= snapshot.bags.size()) return "Select a destination bag.";
            const auto& target = snapshot.bags[destination];
            if (source.bag == 3 || destination == 3) return "Temporary items cannot be transferred.";
            if (!target.ready || !target.accessible || target.capacity == 0 || target.capacity > 80) return "The destination is not accessible here.";
            if (Wardrobe(destination) && !Gear(item.type)) return "Only weapons and armor can enter a wardrobe.";
            if (result.quantity == 0 || result.quantity > source.count) return "Choose a quantity within the current stack.";
            if (request.action == satchel_action::split && result.quantity >= source.count) return "A split must leave items in the original stack.";
            const bool splitting = result.quantity < source.count;
            if (!splitting && request.destination_slot != 0x52)
            {
                const auto* candidate = SatchelFind(snapshot, destination, request.destination_slot);
                if (candidate != nullptr)
                {
                    if ((source.bag == destination && source.slot == candidate->slot) || !SameItem(source, *candidate) ||
                        candidate->read_only || candidate->flags != 0 || candidate->price != 0 || candidate->equipped ||
                        item.stack <= 1 || static_cast<uint64_t>(candidate->count) + result.quantity > item.stack)
                        return "These stacks are not compatible or the destination stack is full.";
                    result.destination_slot = candidate->slot;
                    return {};
                }
                if (request.destination_slot == 0 || request.destination_slot > target.capacity)
                    return "That destination slot is unavailable.";
            }
            if (!splitting && request.destination_slot == 0x52)
            {
                for (const auto& candidate : target.items)
                {
                    if (candidate.slot == source.slot && destination == source.bag) continue;
                    if (SameItem(source, candidate) && !candidate.read_only && !candidate.equipped && candidate.flags == 0 && candidate.price == 0 &&
                        item.stack > 1 && static_cast<uint64_t>(candidate.count) + result.quantity <= item.stack)
                    {
                        result.destination_slot = candidate.slot;
                        return {};
                    }
                }
            }
            if (target.items.size() >= target.capacity) return "The destination needs an empty slot.";
            if (source.bag == destination && request.action == satchel_action::move && !splitting)
                return "Select another stack to combine, or use Split.";
            return {};
        }
        case satchel_action::use:
            if (source.bag != 0 && source.bag != 3 && !Wardrobe(source.bag)) return "Move this item to Inventory before using it.";
            if (Gear(item.type))
            {
                if (!source.equipped || item.charges == 0 || source.extra[0] != 1) return "Equip an enchanted item before using it.";
                if (item.charges != 255 && source.extra[1] == 0) return "This item has no charges remaining.";
            }
            else if (item.type != 7 || (item.flags & 0x0200) == 0) return "This item cannot be used.";
            if ((item.targets & 1) == 0) return "This item requires a target; use the native item menu.";
            return {};
        case satchel_action::equip:
            if (source.bag != 0 && !Wardrobe(source.bag)) return "Move this equipment to Inventory or a wardrobe first.";
            if (!Gear(item.type) || item.slots == 0) return "This item cannot be equipped.";
            if (item.job == 0 || item.job > 22 || (item.jobs & (1u << item.job)) == 0 || item.job_level < item.level)
                return "Your current job or level cannot equip this item.";
            result.equipment_slot = request.equipment_slot;
            if (result.equipment_slot < 0)
                for (int slot = 0; slot < 16; ++slot)
                    if ((item.slots & (1u << slot)) != 0) { result.equipment_slot = slot; break; }
            if (result.equipment_slot < 0 || result.equipment_slot > 15 || (item.slots & (1u << result.equipment_slot)) == 0)
                return "This item cannot be equipped in that slot.";
            return {};
        case satchel_action::drop:
            if (source.bag != 0) return "Move this item to Inventory before dropping it.";
            if (source.id >= 29312 && source.id <= 29339 &&
                std::any_of(source.extra.begin(), source.extra.end(), [](const uint8_t byte) { return byte != 0; }))
                return "A storage slip with stored items cannot be dropped.";
            if (result.quantity == 0 || result.quantity > source.count || result.quantity > 99) return "Choose a valid drop quantity.";
            return {};
        case satchel_action::bazaar:
            if (source.bag != 0) return "Move this item to Inventory before listing it.";
            if ((item.flags & 0x4000) != 0 || (Gear(item.type) && (source.extra[0] == 2 || source.extra[0] == 3)))
                return "Exclusive and augmented equipment cannot be sold in a bazaar.";
            if (item.type == 9) return "Currency cannot be listed in a bazaar.";
            if (request.price > 99999999) return "The maximum bazaar price is 99,999,999 gil.";
            if (source.price == request.price) return "This item already has that bazaar price.";
            return {};
        case satchel_action::sort: return {};
        }
        return "That action is unavailable.";
    }

    auto satchel_actions::Prepare(IAshitaCore* core, const satchel_snapshot& snapshot, const satchel_action_request& request,
        satchel_snapshot& live, plan& result) const -> std::string
    {
        if (this->restricted_) return "Item actions are unavailable while this interface is suspended.";
        if (this->busy_) return "Wait for the current item action to finish.";
        if (const auto reason = SnapshotEligibility(snapshot, request); !reason.empty()) return reason;
        if (const auto reason = LiveGuard(core, snapshot); !reason.empty()) return reason;
        const auto* memory = core->GetMemoryManager();
        const auto* inventory = memory->GetInventory();
        live = snapshot;
        if (!ReadBag(*inventory, live, request.item.bag)) return "The source bag is still loading.";
        if (request.action == satchel_action::move && request.destination != request.item.bag &&
            !ReadBag(*inventory, live, request.destination)) return "The destination bag is still loading.";
        resource item{};
        if (request.action != satchel_action::sort)
        {
            auto* resources = core->GetResourceManager();
            const auto* native = resources != nullptr ? resources->GetItemById(request.item.id) : nullptr;
            if (native == nullptr) return "Item information is unavailable.";
            item.type = native->Type; item.flags = native->Flags; item.stack = std::max<uint16_t>(1, native->StackSize);
            item.slots = native->Slots; item.targets = native->Targets; item.level = native->Level; item.jobs = native->Jobs;
            item.charges = native->MaxCharges; item.job = memory->GetPlayer()->GetMainJob(); item.job_level = memory->GetPlayer()->GetMainJobLevel();
        }
        return Plan(live, request, item, result);
    }

    auto satchel_actions::Eligibility(IAshitaCore* core, const satchel_snapshot& snapshot, const satchel_action_request& request) const -> std::string
    {
        satchel_snapshot live{}; plan result{};
        return this->Prepare(core, snapshot, request, live, result);
    }

    auto satchel_actions::Submit(IAshitaCore* core, const satchel_snapshot& snapshot, satchel_action_request request, const uint64_t now) -> bool
    {
        satchel_snapshot live{}; plan result{};
        if (const auto reason = this->Prepare(core, snapshot, request, live, result); !reason.empty())
        {
            this->status_ = reason;
            this->outcome_ = satchel_action_outcome::rejected;
            return false;
        }
        if (request.action == satchel_action::sort && now < this->next_sort_[request.item.bag])
        {
            this->status_ = "Wait before sorting this bag again.";
            this->outcome_ = satchel_action_outcome::rejected;
            return false;
        }
        if (request.action == satchel_action::split) request.destination = request.item.bag;
        Dispatch(*core->GetPacketManager(), request, result, snapshot.character_id, core->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0));
        if (request.action == satchel_action::sort) this->next_sort_[request.item.bag] = now + 1500;
        this->before_ = std::move(live);
        this->pending_ = std::move(request);
        this->plan_ = result;
        this->busy_ = true;
        this->outcome_ = satchel_action_outcome::pending;
        this->deadline_ = now + (this->pending_.action == satchel_action::use ? 15000 : 5000);
        this->status_ = std::string{ActionName(this->pending_.action)} + " submitted; waiting for an inventory update.";
        return true;
    }

    auto satchel_actions::Dispatch(IPacketManager& packets, const satchel_action_request& request, const plan& result,
        const uint32_t character_id, const uint32_t character_index) -> void
    {
        // Layouts follow Phoenix's c2s packet definitions; Ashita fills the header.
        switch (request.action)
        {
        case satchel_action::move:
        case satchel_action::split:
        {
            std::array<uint8_t, 12> bytes{};
            Put32(bytes, 4, result.quantity);
            bytes[8] = request.item.bag; bytes[9] = request.destination; bytes[10] = request.item.slot; bytes[11] = result.destination_slot;
            Send(packets, 0x029, bytes);
            break;
        }
        case satchel_action::use:
        {
            std::array<uint8_t, 20> bytes{};
            Put32(bytes, 4, character_id);
            bytes[12] = static_cast<uint8_t>(character_index); bytes[13] = static_cast<uint8_t>(character_index >> 8); bytes[14] = request.item.slot;
            Put32(bytes, 16, request.item.bag);
            Send(packets, 0x037, bytes);
            break;
        }
        case satchel_action::equip:
        {
            std::array<uint8_t, 8> bytes{};
            bytes[4] = request.item.slot; bytes[5] = static_cast<uint8_t>(result.equipment_slot); bytes[6] = request.item.bag;
            Send(packets, 0x050, bytes);
            break;
        }
        case satchel_action::drop:
        {
            std::array<uint8_t, 12> bytes{};
            Put32(bytes, 4, result.quantity); bytes[8] = request.item.bag; bytes[9] = request.item.slot;
            Send(packets, 0x028, bytes);
            break;
        }
        case satchel_action::bazaar:
        {
            std::array<uint8_t, 12> bytes{};
            bytes[4] = request.item.slot; Put32(bytes, 8, request.price);
            Send(packets, 0x10B, std::array<uint8_t, 8>{});
            Send(packets, 0x10A, bytes);
            Send(packets, 0x109, std::array<uint8_t, 4>{});
            break;
        }
        case satchel_action::sort:
        {
            std::array<uint8_t, 8> bytes{}; bytes[4] = request.item.bag;
            Send(packets, 0x03A, bytes);
            break;
        }
        }
    }

    auto satchel_actions::Observe(const satchel_snapshot& snapshot, const uint64_t now, const bool equipment_ack) -> void
    {
        if (!this->busy_) return;
        if (!snapshot.ready || snapshot.read_only || snapshot.identity != this->before_.identity ||
            snapshot.character_id != this->before_.character_id || snapshot.zone != this->before_.zone)
        {
            this->Cancel();
            return;
        }
        const auto& request = this->pending_;
        const auto* current = SatchelFind(snapshot, request.item.bag, request.item.slot);
        bool confirmed{};
        const auto reduced = [&] {
            const auto remaining = request.item.count - this->plan_.quantity;
            return remaining == 0 ? current == nullptr : current != nullptr && SameItem(*current, request.item) && current->count == remaining;
        };
        switch (request.action)
        {
        case satchel_action::move:
        case satchel_action::split:
            if (request.destination == request.item.bag)
            {
                if (reduced())
                {
                    const auto* oldTarget = SatchelFind(this->before_, request.destination, this->plan_.destination_slot);
                    const auto* newTarget = SatchelFind(snapshot, request.destination, this->plan_.destination_slot);
                    if (this->plan_.destination_slot < 0x52)
                        confirmed = oldTarget != nullptr && newTarget != nullptr && SameItem(*newTarget, request.item) &&
                            newTarget->count == oldTarget->count + this->plan_.quantity;
                    else
                        for (const auto& candidate : snapshot.bags[request.destination].items)
                            if (candidate.slot != request.item.slot && SatchelFind(this->before_, request.destination, candidate.slot) == nullptr &&
                                SameItem(candidate, request.item) && candidate.count == this->plan_.quantity) { confirmed = true; break; }
                }
            }
            else
                confirmed = reduced() && Total(snapshot.bags[request.destination], request.item) ==
                    Total(this->before_.bags[request.destination], request.item) + this->plan_.quantity;
            break;
        case satchel_action::drop: confirmed = reduced(); break;
        case satchel_action::use:
            confirmed = current == nullptr || (current->id == request.item.id &&
                (current->count < request.item.count || current->extra != request.item.extra));
            break;
        case satchel_action::equip:
            confirmed = equipment_ack && current != nullptr && SameItem(*current, request.item) && current->equipped;
            break;
        case satchel_action::bazaar:
            confirmed = current != nullptr && SameItem(*current, request.item) && current->count == request.item.count && current->price == request.price;
            break;
        case satchel_action::sort:
        {
            const auto& old = this->before_.bags[request.item.bag].items;
            const auto& updated = snapshot.bags[request.item.bag].items;
            if (old.size() != updated.size()) confirmed = true;
            else
                for (const auto& previous : old)
                {
                    const auto* next = SatchelFind(snapshot, request.item.bag, previous.slot);
                    if (next == nullptr || !SameItem(*next, previous) || next->count != previous.count) { confirmed = true; break; }
                }
            break;
        }
        }
        if (confirmed)
        {
            this->busy_ = false;
            this->outcome_ = satchel_action_outcome::observed;
            this->status_ = request.action == satchel_action::sort || request.action == satchel_action::use
                ? std::string{ActionName(request.action)} + " submitted; a live inventory change was observed."
                : std::string{ActionName(request.action)} + " observed in live inventory.";
        }
        else if (now >= this->deadline_)
        {
            this->busy_ = false;
            this->outcome_ = satchel_action_outcome::unconfirmed;
            this->status_ = std::string{ActionName(request.action)} + " submitted; no confirming inventory change was observed.";
        }
    }

    auto satchel_actions::Update(IAshitaCore* core, const satchel_snapshot& snapshot, const uint64_t now) -> void
    {
        if (!this->busy_) return;
        if (this->restricted_ || !LiveGuard(core, this->before_).empty()) { this->Cancel(); return; }
        auto live = snapshot;
        const auto* inventory = core->GetMemoryManager()->GetInventory();
        if (!ReadBag(*inventory, live, this->pending_.item.bag) ||
            ((this->pending_.action == satchel_action::move || this->pending_.action == satchel_action::split) &&
                this->pending_.destination != this->pending_.item.bag && !ReadBag(*inventory, live, this->pending_.destination)))
        {
            this->Cancel();
            return;
        }
        const auto* equipment = this->pending_.action == satchel_action::equip
            ? inventory->GetEquippedItem(static_cast<uint32_t>(this->plan_.equipment_slot)) : nullptr;
        const auto expectedIndex = static_cast<uint32_t>(this->pending_.item.bag) << 8 | this->pending_.item.slot;
        this->Observe(live, now, equipment != nullptr && equipment->Index == expectedIndex);
    }

    auto satchel_actions::Cancel(void) -> void
    {
        if (this->busy_)
        {
            this->status_ = "Stopped waiting for the submitted action; check live inventory before retrying.";
            this->outcome_ = satchel_action_outcome::cancelled;
        }
        this->busy_ = false;
        this->pending_ = {};
        this->before_ = {};
        this->deadline_ = 0;
    }

    auto satchel_actions::SetRestricted(const bool restricted) -> void
    {
        this->restricted_ = restricted;
        if (restricted) this->Cancel();
    }
}
