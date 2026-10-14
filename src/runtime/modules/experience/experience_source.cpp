#include "experience_source.hpp"
#include "combat/action_packet.hpp"
#include "Memory.h"

#include <algorithm>
#include <ctime>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& result) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&result, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui
{
    auto experience_source::InitializeClock(void) -> void
    {
        this->initialized_ = true;
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll"), size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size == 0) return;
        constexpr char pattern[] = "8B0D????????8B410C8B49108D04808D04808D04808D04C1C3";
        const auto found = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
        if (found != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0) Read(found + 2, this->time_slot_);
        if (this->time_slot_ < base || this->time_slot_ - base + 4 > size) this->time_slot_ = 0;
    }

    auto experience_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto index = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
        const auto id = party != nullptr ? party->GetMemberServerId(0) : 0;
        if (player == nullptr || id == 0 || player->GetIsZoning() || player->GetMainJob() == 0 || player->GetMainJob() > 22 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            entity == nullptr || index == 0 || index >= entity->GetEntityMapSize() || entity->GetServerId(index) != id)
        {
            this->ready = false; this->next_scan_ = 0; this->tracker.Discontinuity();
            this->food = {}; this->dedication = {}; this->ring = {}; this->report_.reset();
            this->food_consumed_ = this->dedication_consumed_ = false;
            this->sync_penalty.reset(); this->dedication_pool.reset(); return;
        }
        if (now < this->next_scan_) return;
        this->next_scan_ = now + 250;
        if (!this->initialized_) this->InitializeClock();
        uintptr_t time{}; uint32_t utc{};
        this->utc_ = Read(this->time_slot_, time) && Read(time + 12, utc) && utc > 1009810800 ? utc : 0;
        const auto job = player->GetMainJob();
        auto level = player->GetJobLevel(job);
        if (level == 0 || player->GetExpNeeded() == 0) { this->ready = false; this->tracker.Discontinuity(); return; }
        experience_sample sample{id, party->GetMemberZone(0), player->GetMainJobLevel()};
        sample.progress = {job, level, player->GetExpCurrent(), player->GetExpNeeded(), player->GetLimitPoints(),
            player->GetMeritPoints(), player->GetMeritPointsMax(), player->GetCapacityPoints(job), player->GetJobPoints(job), player->GetJobPointsSpent(job),
            player->GetMasteryJob(), player->GetMasteryExp(), player->GetMasteryExpNeeded(), player->GetMasteryJobLevel(),
            player->GetIsLimitModeEnabled(), player->GetIsExperiencePointsLocked(), player->HasKeyItem(3262)};
        const auto status = entity->GetStatusServer(index); sample.dead = status == 2 || status == 3;
        sample.sub_job = player->GetSubJob(); sample.effective_sub_level = player->GetSubJobLevel();
        if (sample.sub_job > 0 && sample.sub_job <= 22)
        {
            const auto masteryBonus = sample.progress.mastery_job == job ? sample.progress.master_level / 5 : 0;
            sample.sub_level = sample.effective_level == level ? sample.effective_sub_level :
                std::min<uint32_t>(player->GetJobLevel(sample.sub_job), level / 2 + masteryBonus);
        }
        const auto& previous = this->tracker.Sample();
        if (previous.player != sample.player || previous.zone != sample.zone || previous.progress.job != job)
        {
            this->report_.reset(); this->last_status_sequence_ = this->last_chain_sequence_ = 0;
            this->no_exp_reason.clear(); this->food_ended = this->dedication_ended = false;
        }
        this->LoadCharacter(id);
        this->tracker.Observe(sample, now, this->utc_ != 0 ? this->utc_ : static_cast<uint32_t>(std::time(nullptr)));
        if (this->restored_chain_)
        {
            if (this->restored_chain_zone_ == sample.zone && this->restored_chain_->deadline > now) this->tracker.Chain() = *this->restored_chain_;
            this->restored_chain_.reset();
        }
        this->ready = true;
        this->food = {}; this->dedication = {}; this->sync_active = false;
        std::array<int16_t, 32> icons{}; std::array<uint32_t, 32> timers{};
        const auto* nativeIcons = player->GetStatusIcons();
        const auto* nativeTimers = player->GetStatusTimers();
        bool timed = nativeIcons != nullptr && Read(reinterpret_cast<uintptr_t>(nativeIcons), icons);
        if (!timed) Read(reinterpret_cast<uintptr_t>(player->GetBuffs()), icons);
        timed = timed && Read(reinterpret_cast<uintptr_t>(nativeTimers), timers) && this->utc_ != 0;
        for (size_t i = 0; i < icons.size(); ++i)
        {
            if (icons[i] == 269) this->sync_active = true;
            auto* effect = icons[i] == 251 ? &this->food : icons[i] == 249 ? &this->dedication : nullptr;
            if (effect == nullptr) continue;
            effect->active = true;
            if (!timed || timers[i] == 0 || timers[i] == 0x7FFFFFFF) continue;
            const auto ticks = static_cast<int32_t>(timers[i] - (this->utc_ - 0x3C307D70u) * 60u);
            if (ticks > 0 && ticks <= 604800 * 60) effect->deadline = now + static_cast<uint64_t>(ticks) * 1000 / 60;
        }
        this->sync_penalty.reset(); this->dedication_pool.reset();
        if (!this->food.active) this->food_consumed_ = false;
        if (!this->dedication.active) this->dedication_consumed_ = false;
        if (this->report_ && now >= this->report_at_ && now - this->report_at_ <= 12000)
        {
            const auto& v = this->report_->values;
            if (v[0] == id && v[1] == sample.zone && v[2] == job && v[3] == level && v[4] == sample.effective_level &&
                (v[7] != 0) == this->sync_active && (v[10] != 0) == this->dedication.active && v[14] != 0)
            {
                this->sync_penalty = static_cast<uint32_t>(v[8]);
                if (this->dedication.active && v[13] * 1000 > now - this->report_at_)
                {
                    if (!this->pool_invalidated_) this->dedication_pool = static_cast<uint32_t>(v[11]);
                    this->dedication.deadline = this->report_at_ + v[13] * 1000;
                }
            }
        }
        if (now >= this->reason_until_) this->no_exp_reason.clear();
        const auto* inventory = memory->GetInventory();
        if (inventory != nullptr && now >= this->next_ring_) { this->ScanRings(*inventory); this->next_ring_ = now + 1000; }
    }

    auto experience_source::ScanRings(const IInventory& inventory) -> void
    {
        this->ring = {};
        for (uint32_t bag = 0; bag <= 16; ++bag)
        {
            if ((inventory.GetContainerUpdateFlags() & (1u << bag)) == 0) continue;
            const auto maximum = std::min(80u, inventory.GetContainerCountMax(bag));
            for (uint32_t slot = 1; slot <= maximum; ++slot)
            {
                const auto* item = inventory.GetContainerItem(bag, slot);
                if (item == nullptr || item->Count == 0) continue;
                experience_ring candidate; candidate.id = item->Id;
                unsigned maximumCharges{};
                switch (item->Id)
                {
                case 15761: candidate.name = "Chariot Band"; maximumCharges = 7; break;
                case 15762: candidate.name = "Empress Band"; maximumCharges = 7; break;
                case 15763: candidate.name = "Emperor Band"; maximumCharges = 3; break;
                case 15793: candidate.name = "Anniversary Ring"; maximumCharges = 10; break;
                default: continue;
                }
                if (item->Extra[0] == 1 && item->Extra[1] <= maximumCharges)
                {
                    candidate.charges = item->Extra[1];
                    if ((item->Extra[3] & 0x40) != 0) candidate.recast = 0;
                    else if (this->utc_ != 0)
                    {
                        uint32_t target{}; std::memcpy(&target, item->Extra + 4, 4);
                        if (target != 0 && target <= UINT32_MAX - 1009810800u)
                            candidate.recast = target + 1009810800 > this->utc_ ? target + 1009810800 - this->utc_ : 0;
                    }
                    if (item->Flags == 5 && this->utc_ != 0 && candidate.recast)
                    {
                        uint32_t target{}; std::memcpy(&target, item->Extra + 8, 4);
                        if (target != 0 && target <= UINT32_MAX - 1009810800u && target + 1009810800 > this->utc_)
                            candidate.recast = std::max(*candidate.recast, target + 1009810800 - this->utc_);
                    }
                }
                if (this->ring.id == 0 || (candidate.charges > 0 && this->ring.charges <= 0) ||
                    (candidate.charges > 0 && this->ring.charges > 0 && candidate.recast.value_or(UINT32_MAX) < this->ring.recast.value_or(UINT32_MAX))) this->ring = candidate;
            }
        }
    }

    auto experience_source::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (id == 0xA || id == 0xB)
        {
            this->tracker.Discontinuity(); this->ready = false; this->next_scan_ = 0; this->report_.reset();
            this->last_status_sequence_ = this->last_chain_sequence_ = 0;
            this->food_ended = this->dedication_ended = false; return;
        }
        if (!this->ready) this->Update(core, now);
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto& sample = this->tracker.Sample();
        if (!this->ready || party == nullptr || party->GetMemberServerId(0) != sample.player) return;
        const auto utc = this->utc_ != 0 ? this->utc_ : static_cast<uint32_t>(std::time(nullptr));
        if (id == 0x2D)
        {
            const auto award = DecodeExperienceAward(data, sample.player);
            if (award)
            {
                this->tracker.Award(award->kind, award->amount, award->chain, true, now, utc);
                if (award->kind == progression_kind::experience) { this->pool_invalidated_ = true; this->dedication_pool.reset(); }
            }
        }
        else if (id == 0x28 && data.size() >= 24 && ExperiencePacketValue<uint32_t>(data, 5) == sample.player && ((data[10] >> 2) & 0xF) == 5)
        {
            const auto action = DecodeCombatAction(data);
            if (action && action->actor == sample.player && action->category == 5)
                for (size_t target = 0; target < action->target_count; ++target)
                    if (action->targets[target].id == sample.player)
                        for (size_t result = 0; result < action->targets[target].result_count; ++result)
                        {
                            const auto& value = action->targets[target].results[result];
                            if (value.message == 105)
                            {
                                this->tracker.Award(progression_kind::experience, value.parameter, 0, false, now, utc);
                                this->pool_invalidated_ = true; this->dedication_pool.reset();
                            }
                        }
        }
        else if (id == 0x29 && data.size() >= 28 && ExperiencePacketValue<uint32_t>(data, 4) == sample.player && ExperiencePacketValue<uint32_t>(data, 8) == sample.player)
        {
            const auto message = ExperiencePacketValue<uint16_t>(data, 24) & 0x7FFF;
            const auto effect = ExperiencePacketValue<uint32_t>(data, 12);
            if (message == 206 && effect == 251 && this->food.active && !this->food_consumed_)
            { this->food_ended = this->food_consumed_ = true; this->food = {}; }
            if (message == 206 && effect == 249 && this->dedication.active && !this->dedication_consumed_)
            { this->dedication_ended = this->dedication_consumed_ = true; this->dedication = {}; this->dedication_pool.reset(); this->report_.reset(); }
            if (message == 37 || message == 545)
            { this->no_exp_reason = message == 37 ? "Too far from battle" : "Sync target unavailable"; this->reason_until_ = now + 15000; }
        }
        else if (id == 0x17)
        {
            const auto report = DecodeMomentumReport(data, sample);
            if (!report) return;
            const auto& v = report->values;
            auto& sequence = report->chain ? this->last_chain_sequence_ : this->last_status_sequence_;
            if (v[5] <= sequence) return;
            sequence = v[5];
            if (this->utc_ != 0 && (v[6] > this->utc_ + 15ull || v[6] + 15ull < this->utc_)) return;
            if (report->chain)
            {
                auto& chain = this->tracker.Chain();
                if (v[7] && v[8])
                {
                    chain.count = static_cast<uint32_t>(v[9]); chain.deadline = v[11] != 0 ? now + v[11] - v[10] : 0;
                    chain.estimated = false;
                    if (!this->tracker.Paused()) chain.best = std::max(chain.best, chain.count);
                }
                else chain.Clear();
            }
            else { this->report_ = report; this->report_at_ = now; this->pool_invalidated_ = false; }
        }
    }
}
