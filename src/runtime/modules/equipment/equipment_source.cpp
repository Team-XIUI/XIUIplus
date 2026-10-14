#include "equipment_source.hpp"
#include "chat/chat_encoding.hpp"

#include <utility>

namespace xiui
{
    auto equipment_source::ReadSlot(const IInventory& inventory, const uint32_t slot) -> equipped_item
    {
        if (slot >= equipment_slot_names.size()) return {};
        const auto* equipment = inventory.GetEquippedItem(slot);
        if (equipment == nullptr) return {};
        if (equipment->Index == 0) return {0, 0, 0, 0, true};
        const auto bag = static_cast<uint8_t>((equipment->Index >> 8) & 0xFF);
        const auto index = static_cast<uint8_t>(equipment->Index & 0xFF);
        if (bag > 16 || index == 0 || index > 80 || (equipment->Index & 0xFFFF0000u) != 0 ||
            (inventory.GetContainerUpdateFlags() & (1u << bag)) == 0 || index > inventory.GetContainerCountMax(bag)) return {};
        const auto* item = inventory.GetContainerItem(bag, index);
        if (item == nullptr || item->Id == 0 || item->Id == 0xFFFF || item->Count == 0) return {};
        return {item->Id, bag, index, item->Count, true};
    }

    auto equipment_source::Update(IAshitaCore* core, const inventory_state& state, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        if (!state.available || inventory == nullptr)
        {
            this->Reset();
            return;
        }
        if (now < this->next_scan_) return;
        auto* resources = core->GetResourceManager();
        for (uint32_t slot = 0; slot < this->items_.size(); ++slot)
        {
            auto next = ReadSlot(*inventory, slot);
            if (!next.ready) continue;
            if (next.id == this->items_[slot].id)
                next.name = std::move(this->items_[slot].name);
            else if (next.id != 0)
            {
                const auto* resource = resources != nullptr ? resources->GetItemById(next.id) : nullptr;
                const char* name = resource != nullptr ? (resource->Name[2] != nullptr ? resource->Name[2] : resource->Name[0]) : nullptr;
                next.name = name != nullptr && name[0] != '\0' ? FromGameText(name) : "Item #" + std::to_string(next.id);
            }
            this->items_[slot] = std::move(next);
        }
        this->available_ = true;
        this->next_scan_ = now + 100;
    }
}
