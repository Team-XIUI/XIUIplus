#include "cast_cost_source.hpp"
#include "ashita_compat.hpp"
#include "chat/chat_encoding.hpp"
#include "combat/ability_recast.hpp"
#include "Memory.h"
#include "native_pattern.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace
{
    template<typename T>
    auto Read(uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ListItem(uintptr_t slot, uintptr_t method) noexcept -> int32_t
    {
        uintptr_t object{}; int32_t index{}, count{}, visible{};
        if (method == 0 || !Read(slot, object) || object == 0 || !Read(object + 0x30, index) ||
            !Read(object + 0x40, visible) || visible <= 0 || !Read(object + 0x44, count) || count <= 0 || count > 4096 || index < 0 || index >= count) return -1;
        using item_fn = int32_t(__thiscall*)(void*, int32_t);
        __try { return reinterpret_cast<item_fn>(method)(reinterpret_cast<void*>(object), index); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    }
}

namespace xiui
{
    auto FormatCastTime(float seconds) -> std::string
    {
        if (!std::isfinite(seconds) || seconds <= 0) return "0s";
        char text[32]{};
        if (seconds >= 60) std::snprintf(text, sizeof(text), "%u:%02u", static_cast<unsigned>(std::ceil(seconds)) / 60, static_cast<unsigned>(std::ceil(seconds)) % 60);
        else std::snprintf(text, sizeof(text), "%.1fs", seconds);
        return text;
    }

    auto cast_cost_source::Initialize(void) -> void
    {
        *this = {};
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll"), size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size == 0) return;
        const auto find = [&](const char* pattern, uint32_t occurrence = 0) { return Ashita::Memory::FindPattern(base, size, pattern, 0, occurrence); };
        const auto slot = [&](const char* pattern, size_t offset, uintptr_t& destination) {
            const auto match = FindUniquePattern(base, size, pattern);
            if (match != 0) Read(match + offset, destination);
            if (destination < base || destination - base + 4 > size) destination = 0;
        };
        slot("8B480C85C974??8B510885D274??3B05", 16, this->menu_slot_);
        slot("81EC80000000568B35????????8BCE8B463050E8", 9, this->ability_slot_);
        slot("81EC80000000568B35????????578BCE8B7E3057", 9, this->spell_slot_);
        slot("8B4424048B0D????????50E8????????8B0D????????C7411402000000C3", 6, this->mount_slot_);
        constexpr char getter[] = "8B44240485C07C??3B41447D??8B49208D04C185C075??83C8FFC204008B108B";
        this->ability_item_ = find(getter);
        this->spell_item_ = find(getter, 1);
        if (find(getter, 2) != 0) this->ability_item_ = this->spell_item_ = 0;
        if (this->spell_item_ == 0) this->spell_item_ = this->ability_item_;
        constexpr char mountGetter[] = "8B44240485C07C??3B41447D??8B49208D04C185C075??83C8FFC204008B00C20400";
        this->mount_item_ = FindUniquePattern(base, size, mountGetter);
    }

    auto cast_cost_source::AvailableMask(void) const -> uint32_t
    {
        if (this->menu_slot_ == 0) return 0;
        return (this->spell_slot_ != 0 && this->spell_item_ != 0 ? 1u : 0u) |
            (this->ability_slot_ != 0 && this->ability_item_ != 0 ? 2u : 0u) | (this->mount_slot_ != 0 && this->mount_item_ != 0 ? 4u : 0u);
    }

    auto cast_cost_source::Selection(void) const -> std::pair<action_cost_kind, int32_t>
    {
        uintptr_t primitive{}, header{}; std::array<char, 16> name{}; uint8_t closed{};
        if (!Read(this->menu_slot_, primitive) || primitive == 0 || !Read(primitive + 0x70, closed) || closed != 0 ||
            !Read(primitive + 4, header) || header == 0 || !Read(header + 0x46, name)) return {};
        const std::string_view menu{name.data(), name.size()};
        if (menu.substr(0, 8) != "menu    ") return {};
        const auto id = menu.substr(8);
        if (id == "magic   ") return {action_cost_kind::spell, ListItem(this->spell_slot_, this->spell_item_)};
        if (id == "ability ") return {action_cost_kind::ability, ListItem(this->ability_slot_, this->ability_item_)};
        if (id == "mount   ") return {action_cost_kind::mount, ListItem(this->mount_slot_, this->mount_item_)};
        return {};
    }

    auto cast_cost_source::ReadResource(IAshitaCore* core, action_cost_kind kind, uint32_t id) -> void
    {
        this->state_ = {};
        auto* resources = core != nullptr ? core->GetResourceManager() : nullptr;
        if (resources == nullptr) return;
        auto& state = this->state_;
        state.kind = kind; state.id = id;
        const char* name{};
        if (kind == action_cost_kind::spell && id < 2048)
        {
            if (const auto* spell = resources->GetSpellById(id))
            {
                name = spell->Name[2] != nullptr ? spell->Name[2] : spell->Name[0];
                state.mp_cost = spell->ManaCost; state.cast_seconds = spell->CastTime / 4.0f; state.base_recast = spell->RecastDelay / 4.0f;
            }
        }
        else if (kind == action_cost_kind::ability && id < 4096)
        {
            if (const auto* ability = resources->GetAbilityById(id))
            {
                name = ability->Name[2] != nullptr ? ability->Name[2] : ability->Name[0];
                if (id > 0 && id < 256) { state.kind = action_cost_kind::weapon_skill; state.tp_cost = 1000; }
                else
                {
                    state.tp_cost = std::clamp<int>(ability->TPCost, 0, 3000);
                    if (ability->RecastTimerId == 102 || ability->RecastTimerId == 195 || ability->RecastTimerId == 231)
                        state.charge_cost = std::clamp<uint32_t>(ability->ManaCost, 1, 3);
                    else state.mp_cost = ability->ManaCost;
                }
            }
        }
        else if (kind == action_cost_kind::mount && id < 256) { name = resources->GetString("mounts.names", id); state.base_recast = 60; }
        if (name != nullptr && *name) state.name = FromGameText(name);
        else state = {};
    }

    auto cast_cost_source::ReadRecast(IAshitaCore* core) -> void
    {
        auto* memory = core->GetMemoryManager();
        auto* recast = memory != nullptr ? memory->GetRecast() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto& state = this->state_;
        state.remaining = 0; state.recast_known = false;
        state.charges = state.max_charges = 0;
        if (state.kind == action_cost_kind::weapon_skill) { state.recast_known = true; return; }
        if (state.kind == action_cost_kind::mount)
        {
            if (player != nullptr) { state.remaining = player->GetMountRecast() / 60.0f; state.recast_known = true; state.total = 60; }
            return;
        }
        if (recast == nullptr) return;
        state.recast_known = true;
        if (state.kind == action_cost_kind::spell)
        { state.remaining = recast->GetSpellTimer(state.id) / 60.0f; state.total = std::max(state.base_recast, state.remaining); return; }
        const auto* ability = core->GetResourceManager()->GetAbilityById(state.id);
        if (ability == nullptr) { state.recast_known = false; return; }
        const auto timer = ability->RecastTimerId;
        uint32_t frames{}; int16_t modifier{};
        for (uint32_t i = 0; i < 32; ++i)
            if (recast->GetAbilityTimerId(i) == timer)
            {
                frames = recast->GetAbilityTimer(i);
                modifier = static_cast<int16_t>(recast->GetAbilityCalc2(i));
                state.base_recast = static_cast<float>(recast->GetAbilityRecast(i));
                break;
            }
        state.remaining = frames / 60.0f;
        state.total = std::max({state.total, state.base_recast, state.remaining});
        const auto pool = AbilityChargePool(core, timer);
        if (timer == 102 && pool.maximum == 0) state.charge_cost = 0;
        if (const auto charges = CalculateChargeRecast(frames, pool, modifier); charges.maximum != 0)
        {
            state.charges = charges.charges;
            state.max_charges = charges.maximum;
            state.remaining = charges.remaining;
            state.total = charges.duration;
        }
        if (state.charge_cost != 0 && pool.maximum == 0) state.recast_known = false;
    }

    auto cast_cost_source::Update(IAshitaCore* core, uint64_t now) -> void
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        if (player == nullptr || party == nullptr || core->GetResourceManager() == nullptr || player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) == 0 || party->GetMemberZone(0) == 0)
        { this->Clear(); return; }
        if (this->player_ != party->GetMemberServerId(0) || this->zone_ != party->GetMemberZone(0) ||
            this->main_job_ != player->GetMainJob() || this->sub_job_ != player->GetSubJob() ||
            this->main_level_ != player->GetMainJobLevel() || this->sub_level_ != player->GetSubJobLevel())
        {
            this->Clear(); this->player_ = party->GetMemberServerId(0); this->zone_ = party->GetMemberZone(0);
            this->main_job_ = player->GetMainJob(); this->sub_job_ = player->GetSubJob();
            this->main_level_ = player->GetMainJobLevel(); this->sub_level_ = player->GetSubJobLevel();
        }
        const auto [kind, id] = this->Selection();
        if (kind == action_cost_kind::none || id < 0)
        {
            this->state_ = {};
            this->mp_preview_ = {(this->AvailableMask() & 1) != 0, false, this->player_, 0, now};
            return;
        }
        const auto previousKind = this->state_.kind == action_cost_kind::weapon_skill ? action_cost_kind::ability : this->state_.kind;
        const bool changed = id != static_cast<int32_t>(this->state_.id) || kind != previousKind;
        if (!changed && now < this->next_update_) return;
        this->next_update_ = now + 50;
        this->mp_preview_ = {(this->AvailableMask() & 1) != 0, false, this->player_, 0, now};
        if (changed) this->ReadResource(core, kind, static_cast<uint32_t>(id));
        if (!this->state_.Valid()) return;
        this->state_.player_mp = party->GetMemberMP(0); this->state_.player_tp = std::min<uint32_t>(3000, party->GetMemberTP(0));
        if (this->state_.kind == action_cost_kind::spell)
        { this->mp_preview_.selected = true; this->mp_preview_.cost = this->state_.mp_cost; }
        this->ReadRecast(core);
    }
}
