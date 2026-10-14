#include "gil_source.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    namespace
    {
        auto ProcessStarted(void) -> FILETIME
        {
            FILETIME created{}, exited{}, kernel{}, user{};
            ::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel, &user);
            return created;
        }
    }

    auto gil_session::Observe(const uint32_t playerId, const uint32_t amount, const uint64_t now) -> void
    {
        if (playerId == 0) { this->ready = false; return; }
        if (!this->initialized || this->player != playerId || now < this->started)
        {
            this->baseline = amount;
            this->started = now;
            this->initialized = true;
        }
        this->player = playerId;
        this->balance = amount;
        this->ready = true;
    }

    auto gil_session::Reset(const uint64_t now) -> void
    {
        if (!this->ready) { *this = {}; return; }
        this->baseline = this->balance;
        this->started = now;
    }

    auto gil_session::PerHour(const uint64_t now) const -> int64_t
    {
        if (!this->initialized || now <= this->started) return 0;
        return static_cast<int64_t>(std::llround(static_cast<double>(this->Net()) * 3600000.0 / (now - this->started)));
    }

    auto gil_source::HandlePacket(const uint16_t id, const std::span<const uint8_t> data) -> void
    {
        if (id == 0x000A || id == 0x000B)
        {
            this->session_.ready = false;
            this->syncing_ = true;
            this->next_scan_ = 0;
        }
        else if (id == 0x001D && data.size() >= 12 && data[4] <= 1)
        {
            this->syncing_ = data[4] == 0;
            this->next_scan_ = 0;
        }
    }

    auto gil_source::ReadBalance(const IInventory& inventory) -> std::optional<uint32_t>
    {
        const auto* gil = inventory.GetContainerItem(0, 0);
        if ((inventory.GetContainerUpdateFlags() & 1) == 0 || gil == nullptr || gil->Id != 65535) return std::nullopt;
        return gil->Count;
    }

    auto gil_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr;
        if (player != nullptr && player->GetLoginStatus() == static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoginScreen) && !player->GetIsZoning())
            this->session_ = {};
        if (player == nullptr || party == nullptr || inventory == nullptr || player->GetIsZoning() || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) || party->GetMemberServerId(0) == 0)
        {
            this->session_.ready = false;
            this->next_scan_ = 0;
            return;
        }
        const auto* target = memory->GetTarget();
        this->menu_open_ = target != nullptr && target->GetIsMenuOpen() != 0;
        if (this->session_.player != party->GetMemberServerId(0)) { this->session_.ready = false; this->next_scan_ = 0; }
        if (this->syncing_ || now < this->next_scan_) return;
        this->next_scan_ = now + 500;
        // Slot zero is the native gil record, separate from occupied inventory slots.
        const auto balance = ReadBalance(*inventory);
        if (!balance)
        {
            this->session_.ready = false;
            return;
        }
        this->session_.Observe(party->GetMemberServerId(0), *balance, now);
    }

    auto gil_source::LoadSession(const settings_store& store) -> void
    {
        if (this->restored_ || this->session_.initialized) return;
        this->restored_ = true;
        const auto created = ProcessStarted();
        if (static_cast<uint32_t>(store.ReadInt("session.gil", "process", 0)) != ::GetCurrentProcessId() ||
            static_cast<uint32_t>(store.ReadInt("session.gil", "created_low", 0)) != created.dwLowDateTime ||
            static_cast<uint32_t>(store.ReadInt("session.gil", "created_high", 0)) != created.dwHighDateTime) return;
        this->session_.initialized = store.ReadBool("session.gil", "initialized", false);
        this->session_.player = static_cast<uint32_t>(store.ReadInt("session.gil", "player", 0));
        this->session_.baseline = static_cast<uint32_t>(store.ReadInt("session.gil", "baseline", 0));
        this->session_.started = static_cast<uint32_t>(store.ReadInt("session.gil", "started_low", 0)) |
            (static_cast<uint64_t>(static_cast<uint32_t>(store.ReadInt("session.gil", "started_high", 0))) << 32);
        this->session_.balance = this->session_.baseline;
    }

    auto gil_source::SaveSession(const settings_store& store) const -> void
    {
        const auto created = ProcessStarted();
        store.WriteInt("session.gil", "process", static_cast<int>(::GetCurrentProcessId()));
        store.WriteInt("session.gil", "created_low", static_cast<int>(created.dwLowDateTime));
        store.WriteInt("session.gil", "created_high", static_cast<int>(created.dwHighDateTime));
        store.WriteBool("session.gil", "initialized", this->session_.initialized);
        store.WriteInt("session.gil", "player", static_cast<int>(this->session_.player));
        store.WriteInt("session.gil", "baseline", static_cast<int>(this->session_.baseline));
        store.WriteInt("session.gil", "started_low", static_cast<int>(this->session_.started));
        store.WriteInt("session.gil", "started_high", static_cast<int>(this->session_.started >> 32));
    }
}
