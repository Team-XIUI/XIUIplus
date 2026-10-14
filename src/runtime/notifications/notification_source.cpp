#include "notification_source.hpp"
#include "resources/skill_names.hpp"
#include "modules/gil/gil_source.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace xiui
{
    auto notification_source::ResetBalances(void) -> void
    { this->balances_ready_ = false; this->gil_ = 0; this->key_items_ = {}; this->pool_ = {}; this->party_invite_received_at_.reset(); }

    auto notification_source::Reset(const uint64_t now) -> void
    {
        this->inventory_.Reset(now);
        this->player_id_ = this->zone_id_ = 0;
        this->ResetBalances();
    }

    auto notification_source::HandlePacket(IAshitaCore* core, notification_center& center, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (id == 0x000A || id == 0x000B)
        {
            this->inventory_.BeginSync(now);
            center.Clear();
            this->ResetBalances();
            return;
        }
        if (id == 0x001D)
        {
            this->inventory_.HandleSyncPacket(data, now);
            return;
        }
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        if (party == nullptr || player == nullptr || player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
            return;
        const auto number = [data](size_t at, size_t bytes) -> uint32_t {
            uint32_t value{}; if (at <= data.size() && bytes <= data.size() - at) std::memcpy(&value, data.data() + at, bytes); return value;
        };
        const auto invite = DecodeInvitation(id, data);
        if (invite && id == 0xDC)
        {
            center.Post({notification_kind::party_invite, invite->sender, 0,
                std::string{invite->alliance ? "Alliance Invitation: " : "Party Invitation: "} + FromGameText(invite->name)}, now);
            this->party_invite_received_at_ = now;
            return;
        }
        if (invite && id == 0x21)
        {
            const auto* entity = memory->GetEntity();
            const auto index = invite->index, server = invite->sender;
            if (entity != nullptr && index != 0 && index < entity->GetEntityMapSize() && entity->GetServerId(index) == server)
            {
                const auto* trader = entity->GetName(index);
                if (trader != nullptr) center.Post({notification_kind::trade_invite, server, 0, "Trade Request: " + FromGameText(trader)}, now);
            }
            return;
        }
        if (id == 0xD2 && data.size() >= 0x1C)
        {
            const auto slot = data[0x14];
            const auto item = number(0x10, 2), count = number(4, 4);
            if (slot < this->pool_.size()) this->pool_[slot] = data[0x15] == 0 && item != 0 && count > 0 ? pool_notice{item, count, now + 250} : pool_notice{};
            return;
        }
        if (id == 0xD3 && data.size() >= 0x16 && data[0x14] < this->pool_.size() && data[0x15] != 0)
        { this->pool_[data[0x14]] = {}; return; }
        if (id == 0x55 && data.size() >= 0x88)
        {
            const auto table = number(0x84, 2);
            auto* resources = core->GetResourceManager();
            for (const auto key : this->key_items_.Apply(table, data.subspan(4, 64), this->balances_ready_))
                if (const auto* item = resources != nullptr ? resources->GetString("keyitems.names", key) : nullptr; item != nullptr && *item)
                    center.Post({notification_kind::key_item, key, 0, "Key Item: " + FromGameText(item)}, now);
            return;
        }
        auto event = DecodeRewardPacket(id, data, party->GetMemberServerId(0));
        if (!event)
            return;
        if (event->kind == notification_kind::skill)
            event->subject = resources::SkillName(event->key);
        center.Post(std::move(*event), now);
    }

    auto notification_source::ItemTotals(IInventory& inventory) -> item_totals
    {
        item_totals result;
        const auto lastBag = static_cast<uint32_t>(Ashita::FFXI::Enums::Container::Wardrobe8);
        for (uint32_t bag = 0; bag <= lastBag; ++bag)
        {
            const auto capacity = std::min<uint32_t>(80, inventory.GetContainerCountMax(bag));
            for (uint32_t slot = 1; slot <= capacity; ++slot)
            {
                const auto* item = inventory.GetContainerItem(bag, slot);
                if (item != nullptr && item->Id > 0 && item->Id != 0xFFFF && item->Count > 0)
                    result[item->Id] += item->Count;
            }
        }
        return result;
    }

    auto notification_source::UpdatePool(IAshitaCore& core, notification_center& center, const uint64_t now) -> void
    {
        const auto* inventory = core.GetMemoryManager()->GetInventory();
        auto* resources = core.GetResourceManager();
        if (inventory == nullptr || resources == nullptr) return;
        for (uint32_t slot = 0; slot < this->pool_.size(); ++slot)
        {
            auto& pending = this->pool_[slot];
            if (pending.item == 0 || now < pending.due) continue;
            const auto* item = inventory->GetTreasurePoolItem(slot);
            if (item != nullptr && item->ItemId == pending.item && item->Count != 0)
            {
                const auto* resource = resources->GetItemById(pending.item);
                const auto* label = resource != nullptr ? (resource->Name[2] != nullptr ? resource->Name[2] : resource->Name[0]) : nullptr;
                if (label != nullptr) center.Post({notification_kind::treasure, pending.item, static_cast<double>(pending.count), "Treasure: " + FromGameText(label)}, now);
            }
            pending = {};
        }
    }

    auto notification_source::Update(IAshitaCore* core, notification_center& center, const uint64_t now) -> void
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto endSession = [&]()
        {
            if (this->player_id_ != 0)
            {
                this->inventory_.Invalidate(now);
                this->player_id_ = this->zone_id_ = 0;
                center.Clear();
                this->ResetBalances();
            }
        };
        if (party == nullptr || inventory == nullptr || player == nullptr || entity == nullptr || player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        {
            endSession();
            return;
        }
        const auto playerId = party->GetMemberServerId(0);
        const auto index = party->GetMemberTargetIndex(0);
        if (playerId == 0 || index == 0 || index >= entity->GetEntityMapSize())
        {
            endSession();
            return;
        }
        const auto zone = entity->GetZoneId(index);
        if (playerId != this->player_id_ || zone != this->zone_id_)
        {
            this->inventory_.Invalidate(now);
            if (this->player_id_ != 0)
                center.Clear();
            this->player_id_ = playerId;
            this->zone_id_ = zone;
            this->ResetBalances();
        }
        if (this->party_invite_received_at_)
        {
            const auto elapsed = now - *this->party_invite_received_at_;
            // Incoming callbacks run before the client sets its pending-invite flag.
            if (elapsed >= notification_entry::party_invite_lifetime || (elapsed >= 250 && party->GetAllianceInvited() == 0))
            {
                center.Dismiss(notification_kind::party_invite);
                this->party_invite_received_at_.reset();
            }
        }
        this->UpdatePool(*core, center, now);
        uint32_t requiredFlags{};
        for (uint32_t bag = 0; bag <= static_cast<uint32_t>(Ashita::FFXI::Enums::Container::Wardrobe8); ++bag)
            if (inventory->GetContainerCountMax(bag) > 0)
                requiredFlags |= 1u << bag;
        if (!this->inventory_.CanSample(now, inventory->GetContainerUpdateCounter(), inventory->GetContainerUpdateFlags(), requiredFlags))
            return;
        auto* resources = core->GetResourceManager();
        const auto gil = gil_source::ReadBalance(*inventory);
        if (gil)
        {
            if (this->balances_ready_ && *gil > this->gil_) center.Post({notification_kind::gil, 65535, static_cast<double>(*gil - this->gil_), "gil"}, now);
            this->gil_ = *gil;
            if (!this->balances_ready_)
            {
                for (uint32_t key = 0; key < 4096; ++key)
                    if (player->HasKeyItem(key)) this->key_items_.Seed(key);
                this->balances_ready_ = true;
            }
        }
        for (const auto& [id, count] : this->inventory_.Sample(ItemTotals(*inventory), now))
        {
            const auto* item = resources != nullptr ? resources->GetItemById(id) : nullptr;
            const char* name = item != nullptr ? (item->Name[2] != nullptr ? item->Name[2] : item->Name[0]) : nullptr;
            if (name != nullptr && name[0] != '\0')
                center.Post({notification_kind::item, id, static_cast<double>(count), name}, now);
        }
    }
} // namespace xiui
