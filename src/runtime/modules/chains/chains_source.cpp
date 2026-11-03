#include "chains_source.hpp"
#include "ashita_compat.hpp"
#include "chat/chat_encoding.hpp"
#include "resources/chains_data.hpp"
#include "resources/skillchain_data.hpp"
#include "Memory.h"
#include "native_pattern.hpp"

#include <algorithm>
#include <cstring>
#include <tuple>

namespace
{
    template<typename T> auto Read(const uintptr_t address, T& result) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&result, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    auto SpellKnown(const xiui::ashita::player_view& player, const ISpell& spell) -> bool
    {
        if (!player.HasSpellData() || !player.HasSpell(spell.Index)) return false;
        const auto job = player.GetMainJob();
        if (job == 0 || job >= 24) return false;
        const auto required = spell.LevelRequired[job];
        if (required < 0) return false;
        if (spell.JobPointMask & (1u << job)) return player.GetMainJobLevel() >= 99 && player.GetJobPointsSpent(job) >= required;
        return required > 0 && required < 255 && player.GetMainJobLevel() >= required;
    }
}

namespace xiui
{
    auto ChainsLevel(const skillchain_property property) -> unsigned
    {
        if (property >= skillchain_property::light2) return 4;
        if (property >= skillchain_property::light) return 3;
        if (property >= skillchain_property::fusion) return 2;
        return property == skillchain_property::none ? 0 : 1;
    }
    auto ChainsRows(const skillchain_source& source, const skillchain_window& window,
        const std::span<const chains_action> actions, const chains_filters& filters) -> std::vector<chains_row>
    {
        std::vector<chains_row> rows;
        for (const auto& action : actions)
        {
            if ((action.group == chains_group::weapon && !filters.weapon) || (action.group == chains_group::spell && !filters.spell) ||
                (action.group == chains_group::pet && !filters.pet)) continue;
            if (!action.ready && ((action.requirement == chains_pet::avatar && filters.avatar) || (action.requirement == chains_pet::jug && filters.jug) ||
                (action.requirement == chains_pet::frame && filters.frame))) continue;
            const auto chain = source.Continuation(window, action.category, action.id, action.name, filters.require_buff, false);
            if (chain == skillchain_property::none) continue;
            const bool ready = action.ready && source.Continuation(window, action.category, action.id, action.name) != skillchain_property::none;
            rows.push_back({action.group, action.name, chain, ready});
        }
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return std::tuple{a.group, -static_cast<int>(ChainsLevel(a.result)), a.name} <
                std::tuple{b.group, -static_cast<int>(ChainsLevel(b.result)), b.name};
        });
        rows.erase(std::unique(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return a.group == b.group && a.name == b.name && a.result == b.result;
        }), rows.end());
        return rows;
    }
    auto chains_source::Clear(void) -> void
    {
        this->actions_.clear(); this->player_ = this->zone_ = 0; this->job_ = this->subjob_ = this->frame_ = 0;
        this->melee_ = this->ranged_ = 0; this->pup_skills_ = false; this->next_update_ = 0;
    }
    auto chains_source::ReadJobData(IAshitaCore* core, std::array<uint8_t, 20>& result) -> bool
    {
        if (!this->initialized_)
        {
            this->initialized_ = true;
            const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll"), size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
            if (base != 0 && size != 0)
            {
                const auto found = FindUniquePattern(base, size, "C1E1032BC8B0018D????????????B9????????F3A55F5E5B");
                if (found != 0) Read(found + 10, this->job_offset_);
                if (this->job_offset_ < 4 || this->job_offset_ > 0x100000) this->job_offset_ = 0;
            }
        }
        uintptr_t inventory{}, object{};
        const auto* pointers = core != nullptr ? core->GetPointerManager() : nullptr;
        return this->job_offset_ != 0 && pointers != nullptr && Read(pointers->Get("inventory"), inventory) && inventory != 0 &&
            Read(inventory, object) && object != 0 && Read(object + this->job_offset_ + 4, result);
    }
    auto chains_source::HandlePacket(const uint16_t id, const std::span<const uint8_t> data) -> void
    {
        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (id == 0x44 && data.size() >= 0x78 && data[4] == 18)
        {
            this->frame_ = data[9];
            std::memcpy(&this->melee_, data.data() + 0x70, 2); std::memcpy(&this->ranged_, data.data() + 0x74, 2);
            this->pup_skills_ = true;
        }
        if (id == 0x44 || id == 0xAC) this->next_update_ = 0;
    }
    auto chains_source::Update(IAshitaCore* core, const skillchain_source& source, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto entities = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        const auto* resources = core != nullptr ? core->GetResourceManager() : nullptr;
        if (player == nullptr || party == nullptr || entities == nullptr || resources == nullptr || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) == 0 || party->GetMemberHP(0) == 0) { this->Clear(); return; }
        const auto actor = party->GetMemberServerId(0); const auto zone = party->GetMemberZone(0);
        const auto job = player->GetMainJob(), subjob = player->GetSubJob();
        if (actor != this->player_ || zone != this->zone_ || job != this->job_ || subjob != this->subjob_)
        { this->Clear(); this->player_ = actor; this->zone_ = zone; this->job_ = job; this->subjob_ = subjob; }
        if (now < this->next_update_) return;
        this->next_update_ = now + 250; this->actions_.clear();
        const auto playerIndex = party->GetMemberTargetIndex(0);
        const uint32_t petIndex = playerIndex > 0 && playerIndex < entities->GetEntityMapSize() ? entities->GetPetTargetIndex(playerIndex) : 0;
        std::string pet;
        if (petIndex > 0 && petIndex < entities->GetEntityMapSize() && entities->GetHPPercent(petIndex) > 0 && entities->GetName(petIndex) != nullptr)
            pet = FromGameText(entities->GetName(petIndex));
        std::array<uint8_t, 20> jobData{};
        const bool hasJobData = (job == 16 || job == 18) && this->ReadJobData(core, jobData);
        if (job == 18 && hasJobData) this->frame_ = jobData[1];
        if (job == 18 && !this->pup_skills_)
        { this->melee_ = player->GetCombatSkill(22).GetSkill(); this->ranged_ = player->GetCombatSkill(23).GetSkill(); }
        for (const auto& skill : source.Skills())
        {
            if (skill.category == 3 && player->HasAbilityData() && player->HasWeaponSkill(skill.id))
                this->actions_.push_back({chains_group::weapon, 3, skill.id, std::string{skill.name}});
            else if (skill.category == 4 && ((job == 16 && hasJobData && skill.id > 512 && skill.id < 768 &&
                std::find(jobData.begin(), jobData.end(), skill.id - 512) != jobData.end()) || (job == 20 && player->GetMainJobLevel() >= 40)))
            {
                const auto* spell = resources->GetSpellById(skill.id);
                if (spell != nullptr && SpellKnown(*player, *spell)) this->actions_.push_back({chains_group::spell, 4, skill.id, std::string{skill.name}});
            }
            else if (skill.category == 13 && !skill.avatar.empty() && (job == 15 || subjob == 15))
            {
                const auto* ability = resources->GetAbilityByName(std::string{skill.name}.c_str(), 2);
                if (ability != nullptr && (player->HasAbility(ability->Id) || player->HasPetCommand(ability->Id)))
                    this->actions_.push_back({chains_group::pet, 13, skill.id, std::string{skill.name}, pet == skill.avatar, chains_pet::avatar});
            }
            else if ((skill.category == 11 || (skill.category == 13 && skill.avatar.empty())) && (job == 9 || subjob == 9))
            {
                bool known{}, current{};
                for (const auto& move : resources::chains_jug_moves) if (move.move == skill.name)
                { known = true; current |= move.pet == std::string_view{pet}.substr(0, 15); }
                if (known) this->actions_.push_back({chains_group::pet, skill.category, skill.id, std::string{skill.name}, current, chains_pet::jug});
            }
            else if (skill.category == 21 && (job == 18 || subjob == 18))
            {
                bool known{}, current{};
                for (const auto& move : resources::chains_frame_moves)
                    if (move.id == skill.id && (this->pup_skills_ || job == 18) && (move.ranged ? this->ranged_ : this->melee_) >= move.skill)
                    { known = true; current |= move.frame == this->frame_; }
                if (known) this->actions_.push_back({chains_group::pet, 21, skill.id, std::string{skill.name}, current && !pet.empty(), chains_pet::frame});
            }
        }
    }
}
