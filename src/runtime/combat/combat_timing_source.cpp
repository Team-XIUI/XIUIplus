#include "combat_timing_source.hpp"
#include "ashita_compat.hpp"
#include "modules/equipment/equipment_source.hpp"
#include "resources/timing_rules.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace
{
    auto Hash(uint64_t& key, const uint64_t value) -> void { key = (key ^ value) * 1099511628211ull; }
    auto Disabled(const int id) -> bool { return id == 2 || id == 7 || id == 10 || id == 19 || id == 28 || id == 159 || id == 193; }
    auto Dual(const unsigned skill) -> bool { return skill == 2 || skill == 3 || skill == 5 || skill == 9 || skill == 11; }
    auto TwoHanded(const unsigned skill) -> bool { return skill == 4 || skill == 6 || skill == 7 || skill == 8 || skill == 10 || skill == 12; }
    auto BaseTP(const double delay) -> double
    {
        if (delay <= 180) return std::floor(50 + (delay - 180) * 15 / 180);
        if (delay <= 450) return std::floor(50 + (delay - 180) * 65 / 270);
        if (delay <= 480) return std::floor(115 + (delay - 450) * 15 / 30);
        if (delay <= 530) return std::floor(130 + (delay - 480) * 15 / 50);
        return std::floor(145 + (delay - 530) * 35 / 470);
    }
}

namespace xiui
{
    auto combat_timing_source::Clear(void) -> void { *this = {}; }

    auto combat_timing_source::ReadLoadout(IAshitaCore& core) -> void
    {
        const auto* memory = core.GetMemoryManager();
        const auto player = xiui::ashita::View(memory->GetPlayer());
        const auto* inventory = memory->GetInventory();
        auto* resourceManager = core.GetResourceManager();
        std::array<int, 1024> mods{};
        uint64_t key = 1469598103934665603ull;
        const auto mainJob = player->GetMainJob(), subJob = player->GetSubJob();
        const auto mainLevel = player->GetMainJobLevel(), subLevel = player->GetSubJobLevel();
        Hash(key, mainJob); Hash(key, mainLevel); Hash(key, subJob); Hash(key, subLevel);
        std::map<std::pair<unsigned, unsigned>, int> traits;
        for (const auto& trait : resources::timing_traits)
            if ((trait.job == mainJob && trait.level <= mainLevel) || (trait.job == subJob && trait.level <= subLevel))
                traits[{trait.trait, trait.mod}] = std::max(traits[{trait.trait, trait.mod}], static_cast<int>(trait.value));
        for (const auto& [type, amount] : traits) mods[type.second] += amount;
        const IItem* main{}; const IItem* sub{}; bool occupiedSub{};
        if (inventory != nullptr && resourceManager != nullptr)
            for (uint32_t slot = 0; slot < 16; ++slot)
            {
                const auto item = equipment_source::ReadSlot(*inventory, slot);
                Hash(key, item.id); Hash(key, item.ready);
                const auto* info = item.id != 0 ? resourceManager->GetItemById(item.id) : nullptr;
                if (slot == 0 && info != nullptr && info->Delay > 0) main = info;
                if (slot == 1) { occupiedSub = item.id != 0; if (info != nullptr && info->Delay > 0) sub = info; }
                if (info == nullptr || info->Level > mainLevel) continue;
                auto rule = std::lower_bound(resources::timing_items.begin(), resources::timing_items.end(), item.id,
                    [](const auto& row, uint32_t id) { return row.item < id; });
                for (; rule != resources::timing_items.end() && rule->item == item.id; ++rule) mods[rule->mod] += rule->value;
            }
        const auto skill = main != nullptr ? main->Skill : 1;
        double weapon = main != nullptr ? main->Delay + (skill == 1 ? 240 : 0) : 480;
        const bool dual = Dual(skill) && sub != nullptr && Dual(sub->Skill);
        if (dual) weapon += sub->Delay;
        double magic{}, ability{}; bool fists{}, healingBlocked{};
        const auto* buffs = player->GetBuffs();
        bool disabled{};
        if (buffs != nullptr)
            for (size_t i = 0; i < 32; ++i)
            {
                const auto buff = buffs[i]; if (buff <= 0 || buff == 255) continue;
                Hash(key, static_cast<uint16_t>(buff));
                disabled = disabled || Disabled(buff);
                healingBlocked = healingBlocked || buff == 8 || buff == 20 || buff == 31;
                if (buff == 33) magic += 0.1465;
                if (buff == 214) magic += 0.06;
                if (buff == 126) magic += 0.25;
                if (buff == 13) magic -= 0.18;
                if (buff == 194) magic -= 0.25;
                if (buff == 1) magic -= 1;
                if (buff == 353 && TwoHanded(skill)) ability += 0.10;
                if (buff == 46) fists = true;
            }
        const double martial = skill == 1 ? mods[173] : 0, dw = dual ? mods[259] : 0;
        const double weaponMS = std::floor(weapon * 1000 / 60) + mods[171];
        double delay = std::floor((weaponMS - std::floor(martial * 1000 / 60)) * (1 - dw / 100));
        magic = std::clamp(magic + mods[167] / 10000.0, -1.0, 0.4375);
        ability = std::clamp(ability + (mods[383] + (TwoHanded(skill) ? mods[217] : 0)) / 10000.0, -0.25, 0.25);
        delay = std::floor(delay * std::clamp(1 - magic - ability - std::min(0.25, mods[384] / 10000.0), 0.07, 2.0));
        delay = std::clamp(std::floor(delay * (1 + mods[380] / 100.0)), std::floor(weaponMS * 0.2), weaponMS * 2);
        if (fists) delay = std::floor(std::clamp(weaponMS - martial * 1000 / 60, 1600.0, 8000.0) * 0.25);
        this->estimate_ = std::clamp(delay / 1000, 0.4, 30.0);
        double tpDelay = skill == 1 ? std::max(96.0, weapon - martial) / 2 : dual ? weapon * (1 - dw / 100) / 2 : weapon;
        const bool twoFists = skill == 1 && (main != nullptr || (!occupiedSub && (mainJob == 1 || mainJob == 2 || mainJob == 6 || mainJob == 13 || mainJob == 18 || mainJob == 19 || subJob == 2 || subJob == 18)));
        if (skill == 1 && !twoFists) tpDelay *= 2;
        this->tp_.Reset(static_cast<float>(std::floor(BaseTP(tpDelay) * (1 + mods[73] / 100.0))), dual || twoFists ? 2.0f : 1.0f, key);
        this->key_ = key; this->attack_.Change(this->estimate_, key);
        const double now = this->previous_scan_ / 1000.0;
        if (disabled) this->attack_.Hold(now, 8); else this->attack_.Release(now, 8);
        this->sample_.hp_base = static_cast<float>(10 + mods[72]); this->sample_.hp_step = 1;
        this->sample_.mp_base = static_cast<float>(12 + mods[71]); this->sample_.mp_step = static_cast<float>(1 + mods[295]);
        this->sample_.blocked = healingBlocked; this->sample_.key = key;
    }

    auto combat_timing_source::Update(IAshitaCore* core, const combat_source& combat, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto entities = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        const auto index = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
        if (player == nullptr || entities == nullptr || index == 0 || index >= entities->GetEntityMapSize() || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) == 0 || entities->GetServerId(index) != party->GetMemberServerId(0) || party->GetMemberHP(0) == 0)
        { this->Clear(); return; }
        if (this->player_ != party->GetMemberServerId(0) || this->zone_ != party->GetMemberZone(0) ||
            (this->previous_scan_ && (now < this->previous_scan_ || now - this->previous_scan_ > 5000)))
        { this->Clear(); this->player_ = party->GetMemberServerId(0); this->zone_ = party->GetMemberZone(0); }
        if (now < this->next_scan_) return;
        this->next_scan_ = now + 100; this->previous_scan_ = now;
        const double clock = now / 1000.0;
        const bool engaged = entities->GetStatus(index) == 1;
        if (engaged && !this->engaged_) this->attack_.Begin(clock, this->estimate_, !this->initialized_);
        if (!engaged && this->engaged_) { this->attack_.Stop(); this->tp_ = {}; }
        this->engaged_ = engaged;
        this->ReadLoadout(*core);
        if (this->vitals_at_ == 0 || now - this->vitals_at_ >= 250)
        {
            this->sample_.hp = party->GetMemberHP(0); this->sample_.mp = party->GetMemberMP(0);
            this->sample_.tp = std::min<uint32_t>(3000, party->GetMemberTP(0));
        }
        this->sample_.hp_max = player->GetHPMax(); this->sample_.mp_max = player->GetMPMax();
        this->sample_.resting = entities->GetStatus(index) == 33;
        this->sample_.joined_late = !this->initialized_;
        this->recovery_.Update(this->sample_, clock); this->tp_.Update(clock, this->sample_.tp);
        this->initialized_ = true;
        std::erase_if(this->enemies_, [&](const auto& pair) {
            const auto& enemy = pair.second;
            return clock - enemy.clock.LastSeen() > 30 || enemy.index == 0 || enemy.index >= entities->GetEntityMapSize() ||
                entities->GetServerId(enemy.index) != pair.first || entities->GetHPPercent(enemy.index) == 0;
        });
        for (auto& [id, enemy] : this->enemies_)
        {
            uint64_t effects{}; bool disabled{}; double modifier = 1;
            const auto* actor = combat.Find(id);
            if (actor != nullptr)
                for (const auto& status : actor->statuses)
                {
                    if (Disabled(status.id)) { disabled = true; effects |= 1ull << 4; }
                    if (status.id == 13) { modifier += 0.18; effects |= 1; }
                    if (status.id == 194) { modifier += 0.25; effects |= 2; }
                    if (status.id == 33) { modifier -= 0.1465; effects |= 4; }
                    if (status.id == 46) effects |= 8;
                }
            if (effects & 8) modifier = 0.25;
            if (effects != enemy.effect_key)
            {
                enemy.clock.Change(enemy.base * std::clamp(modifier, 0.25, 2.0), effects, true);
                enemy.effect_key = effects;
            }
            if (disabled) enemy.clock.Hold(clock, 8); else enemy.clock.Release(clock, 8);
        }
    }

    auto combat_timing_source::Observe(IAshitaCore& core, const combat_action& action, const double now) -> void
    {
        const bool own = action.actor == this->player_;
        for (size_t i = 0; i < action.target_count; ++i)
            if (action.targets[i].id == this->player_)
            {
                if (action.category != 1) this->recovery_.Contaminate(now);
                if (!own) this->tp_.Contaminate(now);
            }
        attack_clock* timer{}; enemy_clock* enemy{};
        if (own) timer = &this->attack_;
        else
        {
            const auto index = FindEntityIndex(core, action.actor);
            const auto entities = xiui::ashita::View(core.GetMemoryManager()->GetEntity());
            if (index == 0 || (entities->GetSpawnFlags(index) & 0x10) == 0) return;
            if (!this->enemies_.contains(action.actor) && this->enemies_.size() >= 64)
            {
                const auto oldest = std::min_element(this->enemies_.begin(), this->enemies_.end(), [](const auto& a, const auto& b) { return a.second.clock.LastSeen() < b.second.clock.LastSeen(); });
                this->enemies_.erase(oldest);
            }
            enemy = &this->enemies_[action.actor]; enemy->index = index; timer = &enemy->clock;
            if (!timer->Predict(now).active) timer->Begin(now, enemy->base, true);
        }
        if (action.category == 1)
        {
            timer->Round(now, own ? this->estimate_ : enemy->base, own ? this->key_ : enemy->effect_key, !own);
            if (own)
            {
                unsigned hits{}; bool contaminated{};
                const auto& target = action.targets[0];
                for (size_t i = 0; i < target.result_count; ++i)
                {
                    const auto& result = target.results[i];
                    hits += result.parameter > 0 && (result.message == 1 || result.message == 67);
                    contaminated = contaminated || result.spikes_message != 0;
                }
                this->tp_.Round(now, hits, this->sample_.tp, contaminated);
            }
            else if (enemy->effect_key == 0 && timer->Predict(now).learned) enemy->base = static_cast<float>(timer->Interval());
        }
        else if (action.category == 3 || action.category == 6)
        {
            timer->AddHold(now, 2);
            if (own) this->tp_.Contaminate(now, action.parameter == 63 ? 24 : 0.5);
        }
        else if (action.category == 7 || action.category == 8 || action.category == 9 || action.category == 12)
        {
            const unsigned reason = action.category == 12 ? 2 : action.category == 9 ? 4 : 1;
            if ((action.parameter & 0xFFFF) == 0x7073) timer->Release(now, reason, !own ? 2 : reason == 2 ? 0.8 : reason == 4 ? 0.4 : 2);
            else timer->Hold(now, reason);
            if (own) this->tp_.Contaminate(now);
        }
        else if (action.category == 2 || action.category == 4 || action.category == 5 || action.category == 11)
        {
            const unsigned reason = action.category == 2 ? 2 : action.category == 5 ? 4 : 1;
            timer->Release(now, reason, !own ? 2 : reason == 2 ? 0.8 : reason == 4 ? 0.4 : 2);
            if (own) this->tp_.Contaminate(now);
        }
        for (size_t i = 0; i < action.target_count; ++i)
            if (action.targets[i].id == this->player_ && action.category != 1)
            { this->recovery_.Contaminate(now); if (!own) this->tp_.Contaminate(now); }
    }

    auto combat_timing_source::HandlePacket(IAshitaCore* core, const combat_source& combat, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> void
    {
        const combat_action_packet local{data};

        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (core == nullptr) return;
        this->Update(core, combat, now);
        if (!this->player_) return;
        const auto clock = now / 1000.0;
        if (id == 0x28) { if (const auto& action = (decoded != nullptr ? *decoded : local).Action()) this->Observe(*core, *action, clock); }
        else if (id == 0x29 && data.size() >= 26)
        {
            uint32_t actor{}; uint16_t message{};
            std::memcpy(&actor, data.data() + 4, 4); std::memcpy(&message, data.data() + 24, 2);
            if (actor == this->player_)
            {
                if (message == 4) this->attack_.Stall("Too far");
                if (message == 5) this->attack_.Stall("Not facing");
                if (message == 84 || message == 106) this->attack_.AddHold(clock, this->attack_.Predict(clock).seconds <= 0.5f ? this->estimate_ : 2);
            }
        }
        else if (id == 0xDF && data.size() >= 20)
        {
            uint32_t member{}; std::memcpy(&member, data.data() + 4, 4);
            if (member != this->player_) return;
            std::memcpy(&this->sample_.hp, data.data() + 8, 4); std::memcpy(&this->sample_.mp, data.data() + 12, 4);
            std::memcpy(&this->sample_.tp, data.data() + 16, 4);
            this->vitals_at_ = now;
            this->sample_.tp = std::min<uint32_t>(3000, this->sample_.tp);
            this->recovery_.Update(this->sample_, clock); this->tp_.Update(clock, this->sample_.tp);
        }
    }

    auto combat_timing_source::Enemy(const uint32_t id, const double now) const -> attack_prediction
    {
        const auto found = this->enemies_.find(id);
        if (found == this->enemies_.end() || now - found->second.clock.LastSeen() > 30) return {};
        return found->second.clock.Predict(now);
    }

    auto combat_timing_source::TP(const uint32_t goal, const double now) const -> tp_prediction
    {
        const auto attack = this->attack_.Predict(now);
        auto result = this->tp_.Predict(this->sample_.tp, goal, attack.interval, attack.seconds);
        if (!attack.known || (!attack.state.empty() && attack.state != "Ready")) result.seconds = -1;
        return result;
    }
}
