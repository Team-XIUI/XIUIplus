#include "inventory_source.hpp"
#include "ashita_compat.hpp"

namespace xiui
{
    auto InventoryTotal(const inventory_state& state, const inventory_group& group) -> inventory_usage
    {
        inventory_usage result{0, 0, state.available};
        for (size_t i = 0; i < group.count; ++i)
        {
            const auto& bag = state.bags[group.bags[i]];
            if (bag.capacity == 0) continue;
            result.used += bag.used;
            result.capacity += bag.capacity;
            result.ready = result.ready && bag.ready;
        }
        result.ready = result.ready && result.capacity > 0;
        return result;
    }

    auto inventory_source::ReadBags(const IInventory& inventory) -> std::array<inventory_usage, 17>
    {
        std::array<inventory_usage, 17> result{};
        const auto flags = inventory.GetContainerUpdateFlags();
        for (uint32_t bag = 0; bag < result.size(); ++bag)
        {
            const auto capacity = inventory.GetContainerCountMax(bag);
            if (capacity == 0 || capacity > 80) continue;
            const auto used = inventory.GetContainerCount(bag);
            result[bag] = {used <= capacity ? used : 0, capacity, used <= capacity && (flags & (1u << bag)) != 0};
        }
        return result;
    }

    auto inventory_source::HandlePacket(const uint16_t id, const std::span<const uint8_t> data) -> void
    {
        if (id == 0x000A || id == 0x000B)
        {
            this->state_ = {};
            this->syncing_ = true;
            this->next_scan_ = 0;
        }
        else if (id == 0x001D && data.size() >= 12 && data[4] <= 1)
        {
            this->syncing_ = data[4] == 0;
            this->next_scan_ = 0;
        }
    }

    auto inventory_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        if (player == nullptr || party == nullptr || inventory == nullptr || player->GetIsZoning() != 0 || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) || party->GetMemberServerId(0) == 0)
        {
            this->state_ = {};
            this->next_scan_ = 0;
            return;
        }
        const auto playerId = party->GetMemberServerId(0);
        const auto zone = party->GetMemberZone(0);
        if (playerId != this->player_id_ || zone != this->zone_)
        {
            this->state_ = {};
            this->player_id_ = playerId;
            this->zone_ = zone;
            this->next_scan_ = 0;
        }
        const auto target = xiui::ashita::View(memory->GetTarget());
        this->state_.menu_open = target != nullptr && target->GetIsMenuOpen() != 0;
        if (this->syncing_ || now < this->next_scan_) return;
        this->state_.bags = ReadBags(*inventory);
        this->state_.available = true;
        this->next_scan_ = now + 500;
    }
}
