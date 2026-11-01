#include "party_source.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address < 0x10000) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto PartyCursor(IAshitaCore* core) -> int
    {
        auto* pointers = core != nullptr ? core->GetPointerManager() : nullptr;
        const auto root = pointers != nullptr ? pointers->Get("party") : 0;
        return xiui::ReadNativePartyCursor(root);
    }
}

namespace xiui
{
    auto ReadNativePartyCursor(const uintptr_t root) -> int
    {
        uintptr_t slot{}, window{}, callback{};
        int32_t member{-1};
        // The active selection field is a callback pointer, not a boolean.
        if (!Read(root, slot) || !Read(slot, window) || !Read(window + 0x54, callback) || callback == 0 ||
            !Read(window + 0x50, member) || member < 0 || member >= 18) return -1;
        return member;
    }

    auto party_cast::Active(const uint64_t now) const -> bool
    {
        return !this->name.empty() && now >= this->started &&
            (this->interrupted != 0 ? now >= this->interrupted && now - this->interrupted < 800 :
                now - this->started < static_cast<uint64_t>((this->seconds + 1.5f) * 1000));
    }

    auto LocalStatusSnapshotReady(const std::span<const int16_t> icons) -> bool
    {
        // Zero is KO; an entirely zero-filled buffer is not an initialized status list.
        return std::any_of(icons.begin(), icons.end(), [](const auto id) { return id != 0; }) &&
            std::all_of(icons.begin(), icons.end(), [](const auto id) { return id >= -1 && id <= 1023; });
    }

    auto PartyStatusSnapshotReady(const std::span<const uint8_t> icons, const uint64_t mask) -> bool
    {
        for (size_t i = 0; i < std::min<size_t>(32, icons.size()); ++i)
            if (icons[i] != 0 || ((mask >> (i * 2)) & 3) != 0) return true;
        return false;
    }

    auto DecodePartyBuffs(const std::span<const uint8_t> icons, const uint64_t mask, const std::span<uint16_t> output) -> size_t
    {
        if (!PartyStatusSnapshotReady(icons, mask)) return 0;
        size_t count{};
        for (size_t i = 0; i < std::min<size_t>(32, icons.size()) && count < output.size(); ++i)
        {
            const auto buff = static_cast<uint16_t>(icons[i] | (((mask >> (i * 2)) & 3) << 8));
            if (buff == 255) break;
            output[count++] = buff;
        }
        return count;
    }

    auto PartyResourceMaximum(const uint32_t value, const uint32_t percent) -> uint32_t
    {
        if (percent == 0 || percent > 100) return 0;
        return static_cast<uint32_t>((static_cast<uint64_t>(value) * 100 + percent / 2) / percent);
    }

    auto LocalResourceMaximum(const uint32_t value, const uint32_t percent, const uint32_t reported, const uint32_t previous) -> uint32_t
    {
        if (percent == 100) return value;
        const auto matches = [&](const uint32_t maximum) {
            return maximum >= value && maximum != 0 &&
                std::abs(static_cast<int>(value * 100ULL / maximum) - static_cast<int>(percent)) <= 1;
        };
        if (matches(reported)) return reported;
        return matches(previous) ? previous : 0;
    }

    auto FormatPartyResource(const uint32_t value, const uint32_t percent, const uint32_t maximum, const bool exact, const int mode) -> std::string
    {
        char text[64]{};
        switch (mode)
        {
        case 0: std::snprintf(text, sizeof(text), "%u", value); break;
        case 1: std::snprintf(text, sizeof(text), "%u%%", percent); break;
        case 2: std::snprintf(text, sizeof(text), "%u (%u%%)", value, percent); break;
        case 3: std::snprintf(text, sizeof(text), "%u%% (%u)", percent, value); break;
        case 4:
            if (maximum == 0) std::snprintf(text, sizeof(text), "%u/?", value);
            else std::snprintf(text, sizeof(text), "%u/%s%u", value, exact ? "" : "~", maximum);
            break;
        default: break;
        }
        return text;
    }

    auto PartyJobName(const uint32_t job) -> const char*
    {
        static constexpr const char* names[]{"", "WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG", "SAM", "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN", "MON", "SPC"};
        return job < std::size(names) ? names[job] : "";
    }

    auto PartyJobHasMP(const uint32_t job) -> bool
    {
        return job == 3 || job == 4 || job == 5 || job == 7 || job == 8 || job == 15 || job == 16 || job == 20 || job == 21 || job == 22;
    }

    auto party_source::Count(const size_t group) const -> size_t
    {
        if (group >= 3) return 0;
        return static_cast<size_t>(std::count_if(this->members_.begin() + group * 6, this->members_.begin() + (group + 1) * 6,
            [](const auto& member) { return member.active; }));
    }

    auto party_source::Clear(void) -> void
    {
        this->members_ = {};
        this->player_id_ = this->zone_ = 0;
        this->next_update_ = 0;
        this->cutscene_ = false;
    }

    auto party_source::UpdateSelection(const uint32_t main, const uint32_t secondary, const bool subtarget, const int partyCursor) -> void
    {
        for (auto& member : this->members_)
        {
            const bool entityTarget = member.active && member.entity_valid && member.index != 0;
            if (partyCursor >= 0)
            {
                member.subtarget = member.active && static_cast<int>(member.slot) == partyCursor;
                member.selected = member.subtarget || (entityTarget && member.index == main);
            }
            else
            {
                member.subtarget = entityTarget && (subtarget || (main == 0 && secondary != 0)) && member.index == (main != 0 ? main : secondary);
                member.selected = entityTarget && (member.index == main || (subtarget && member.index == secondary) || member.subtarget);
            }
        }
    }

    auto party_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        this->resources_ = core != nullptr ? core->GetResourceManager() : nullptr;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (party == nullptr || player == nullptr || entity == nullptr ||
            player->GetIsZoning() || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        { this->Clear(); return; }
        const auto playerId = party->GetMemberServerId(0), playerIndex = party->GetMemberTargetIndex(0);
        const auto mapSize = entity->GetEntityMapSize();
        if (playerId == 0 || playerIndex == 0 || playerIndex >= mapSize || entity->GetServerId(playerIndex) != playerId)
        { this->Clear(); return; }
        const auto zone = party->GetMemberZone(0);
        if (zone == 0) { this->Clear(); return; }
        if (this->player_id_ != playerId || this->zone_ != zone)
        { this->Clear(); this->player_id_ = playerId; this->zone_ = zone; }
        this->cutscene_ = entity->GetStatus(playerIndex) == 4;
        const auto updateSelection = [&] {
            this->UpdateSelection(target != nullptr ? target->GetTargetIndex(0) : 0, target != nullptr ? target->GetTargetIndex(1) : 0,
                target != nullptr && target->GetIsSubTargetActive() != 0, PartyCursor(core));
        };
        updateSelection();
        if (now < this->next_update_) return;
        this->next_update_ = now + 100;
        const std::array leaders{party->GetAlliancePartyLeaderServerId1(), party->GetAlliancePartyLeaderServerId2(), party->GetAlliancePartyLeaderServerId3()};
        auto* resources = core->GetResourceManager();
        for (uint32_t slot = 0; slot < this->members_.size(); ++slot)
        {
            auto& member = this->members_[slot];
            const auto id = party->GetMemberServerId(slot);
            if (party->GetMemberIsActive(slot) == 0 || id == 0)
            { member = {}; continue; }
            if (id != member.id) member = {};
            const bool sameJob = slot == 0 && member.main_job == player->GetMainJob() && member.main_level == player->GetMainJobLevel() &&
                member.sub_job == player->GetSubJob() && member.sub_level == player->GetSubJobLevel();
            const auto previousHP = sameJob && member.hp_max_exact ? member.hp_max : 0;
            const auto previousMP = sameJob && member.mp_max_exact ? member.mp_max : 0;
            member.active = true; member.id = id; member.slot = slot;
            member.zone = party->GetMemberZone(slot);
            member.in_zone = member.zone != 0 && member.zone == zone;
            const auto* name = party->GetMemberName(slot);
            member.name = name != nullptr ? FromGameText(std::string_view{name, strnlen(name, 18)}) : "";
            const auto* zoneName = resources != nullptr ? resources->GetString("zones.names", member.zone) : nullptr;
            member.zone_name = zoneName != nullptr ? FromGameText(zoneName) : "Zone " + std::to_string(member.zone);
            member.leader = leaders[slot / 6] != 0 && leaders[slot / 6] == id;
            member.alliance_leader = party->GetAllianceLeaderServerId() != 0 && party->GetAllianceLeaderServerId() == id;
            member.index = member.in_zone ? party->GetMemberTargetIndex(slot) : 0;
            member.entity_valid = member.index > 0 && member.index < mapSize && entity->GetServerId(member.index) == id;
            member.status_known = member.entity_valid && (entity->GetRenderFlags0(member.index) & 0x200) != 0;
            member.status = member.status_known ? entity->GetStatus(member.index) : 0;
            member.distance_known = false;
            if (member.entity_valid)
            {
                const auto squared = entity->GetDistance(member.index);
                member.distance_known = std::isfinite(squared) && squared >= 0;
                member.distance = member.distance_known ? std::sqrt(squared) : 0;
            }
            member.buff_count = 0;
            member.buff_expires = {};
            member.buffs_known = false;
            if (!member.in_zone)
            {
                member.hp = member.mp = member.tp = member.hp_max = member.mp_max = 0;
                member.hp_percent = member.mp_percent = 0;
                member.main_job = member.main_level = member.sub_job = member.sub_level = member.master_level = 0;
                member.sync = member.hp_max_exact = member.mp_max_exact = false;
                member.cast = {};
                continue;
            }
            member.hp = party->GetMemberHP(slot); member.mp = party->GetMemberMP(slot);
            member.tp = std::min<uint32_t>(3000, party->GetMemberTP(slot));
            member.hp_percent = std::min<uint8_t>(100, party->GetMemberHPPercent(slot));
            member.mp_percent = std::min<uint8_t>(100, party->GetMemberMPPercent(slot));
            member.hp_max = PartyResourceMaximum(member.hp, member.hp_percent);
            member.mp_max = PartyResourceMaximum(member.mp, member.mp_percent);
            member.hp_max_exact = member.hp_percent == 100;
            member.mp_max_exact = member.mp_percent == 100;
            member.main_job = party->GetMemberMainJob(slot); member.main_level = party->GetMemberMainJobLevel(slot);
            member.sub_job = party->GetMemberSubJob(slot); member.sub_level = party->GetMemberSubJobLevel(slot);
            member.master_level = party->GetMemberMasterLevel(slot);
            member.sync = (party->GetMemberFlagMask(slot) & 0x100) != 0;
            if (slot == 0)
            {
                if (const auto maximum = LocalResourceMaximum(member.hp, member.hp_percent, player->GetHPMax(), previousHP))
                { member.hp_max = maximum; member.hp_max_exact = true; }
                if (const auto maximum = LocalResourceMaximum(member.mp, member.mp_percent, player->GetMPMax(), previousMP))
                { member.mp_max = maximum; member.mp_max_exact = true; }
                member.main_job = player->GetMainJob(); member.main_level = player->GetMainJobLevel();
                member.sub_job = player->GetSubJob(); member.sub_level = player->GetSubJobLevel();
                std::array<int16_t, 32> icons{};
                std::array<uint32_t, 32> timers{};
                bool timed = Read(reinterpret_cast<uintptr_t>(player->GetStatusIcons()), icons) && LocalStatusSnapshotReady(icons);
                member.buffs_known = timed || (Read(reinterpret_cast<uintptr_t>(player->GetBuffs()), icons) && LocalStatusSnapshotReady(icons));
                timed = timed && Read(reinterpret_cast<uintptr_t>(player->GetStatusTimers()), timers) && this->utc_ != 0 &&
                    now >= this->utc_sampled_at_ && now - this->utc_sampled_at_ <= 2000;
                for (size_t i = 0; member.buffs_known && i < icons.size(); ++i)
                {
                    if (icons[i] < 0 || icons[i] == 255 || icons[i] > 1023) continue;
                    const auto output = member.buff_count++;
                    member.buffs[output] = static_cast<uint16_t>(icons[i]);
                    const auto* info = this->resources_ != nullptr ? this->resources_->GetStatusIconByIndex(member.buffs[output]) : nullptr;
                    if (!timed || timers[i] == 0 || timers[i] == 0x7FFFFFFF || (info != nullptr && info->HideTimer != 0)) continue;
                    member.buff_expires[output] = NativeStatusDeadline(timers[i], this->utc_, now);
                }
            }
            else if (slot < 6)
            {
                for (uint32_t entry = 0; entry < 5; ++entry)
                    if (party->GetStatusIconsServerId(entry) == id)
                    {
                        if (const auto* icons = party->GetStatusIcons(entry))
                        {
                            const auto mask = party->GetStatusIconsBitMask(entry);
                            member.buffs_known = PartyStatusSnapshotReady({icons, 32}, mask);
                            if (member.buffs_known) member.buff_count = DecodePartyBuffs({icons, 32}, mask, member.buffs);
                        }
                        break;
                    }
            }
            if (!member.cast.Active(now) || member.hp == 0) member.cast = {};
        }
        updateSelection();
    }

    auto party_source::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> void
    {
        const combat_action_packet local{data};

        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (id != 0x28 || core == nullptr) return;
        const auto& action = (decoded != nullptr ? *decoded : local).Action();
        if (!action || action->target_count == 0) return;
        this->Update(core, now);
        auto found = std::find_if(this->members_.begin(), this->members_.end(), [&](const auto& member) { return member.active && member.in_zone && member.id == action->actor; });
        if (found == this->members_.end()) return;
        auto& cast = found->cast;
        const auto message = action->targets[0].message;
        if ((action->category == 8 && (action->parameter & 0xFFFF) == 0x7073) || message == 16 || message == 84 || message == 106)
        { if (cast.Active(now)) cast.interrupted = now; return; }
        if (action->category == 4 || action->category == 11) { cast = {}; return; }
        auto* resources = core->GetResourceManager();
        if (action->category != 8 || resources == nullptr) return;
        const auto spellId = action->targets[0].parameter;
        const auto* spell = resources->GetSpellById(spellId);
        if (spell == nullptr) return;
        const auto* name = spell->Name[2] != nullptr ? spell->Name[2] : spell->Name[0];
        if (name == nullptr) return;
        cast = {FromGameText(name), spellId, now, 0, std::clamp(spell->CastTime / 4.0f, 0.25f, 30.0f)};
    }
}
