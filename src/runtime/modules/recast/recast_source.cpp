#include "recast_source.hpp"
#include "ashita_compat.hpp"
#include "chat/chat_encoding.hpp"
#include "combat/ability_recast.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace xiui
{
    auto recast_source::AbilityName(IResourceManager* resources, ashita::player_view player, const uint32_t timer) const -> std::string
    {
        static constexpr const char* primary[]{"Job ability", "Mighty Strikes", "Hundred Fists", "Benediction", "Manafont", "Chainspell",
            "Perfect Dodge", "Invincible", "Blood Weapon", "Familiar", "Soul Voice", "Eagle Eye Shot", "Meikyo Shisui", "Mijin Gakure",
            "Spirit Surge", "Astral Flow", "Azure Lore", "Wild Card", "Overdrive", "Trance", "Tabula Rasa", "Bolster", "Elemental Sforzo"};
        static constexpr const char* secondary[]{"Job ability", "Brazen Rush", "Inner Strength", "Asylum", "Subtle Sorcery", "Stymie", "Larceny",
            "Intervene", "Soul Enslavement", "Unleash", "Clarion Call", "Overkill", "Yaegasumi", "Mikage", "Fly High", "Astral Conduit",
            "Unbridled Wisdom", "Cutting Cards", "Heady Artifice", "Grand Pas", "Caper Emissarius", "Widened Compass", "Odyllic Subterfuge"};
        if (timer == 0) return primary[this->job_ < std::size(primary) ? this->job_ : 0];
        if (timer == 254) return secondary[this->job_ < std::size(secondary) ? this->job_ : 0];
        if (timer == 102)
        {
            const auto* ready = resources->GetAbilityByName("Ready", 2);
            return ready != nullptr && player->HasAbility(ready->Id) ? "Ready" : "Sic";
        }
        if (timer == 195) return "Quick Draw";
        if (timer == 231) return "Stratagems";
        if (timer == 210) return "Maneuvers";
        if (timer == 10) return "Rune Enchantment";
        const IAbility* chosen = resources->GetAbilityByTimerId(timer);
        if (chosen == nullptr || !player->HasAbility(chosen->Id))
        {
            // Pet commands are unlisted without a pet, so fall back to the first ability sharing the timer.
            const IAbility* fallback = chosen;
            for (uint32_t id = 0x200; id < 0x700; ++id)
                if (const auto* ability = resources->GetAbilityById(id); ability != nullptr && ability->RecastTimerId == timer)
                {
                    if (player->HasAbility(ability->Id)) { fallback = ability; break; }
                    if (fallback == nullptr) fallback = ability;
                }
            chosen = fallback;
        }
        if (chosen != nullptr)
            if (const char* name = chosen->Name[2] != nullptr ? chosen->Name[2] : chosen->Name[0]; name != nullptr && *name) return FromGameText(name);
        char fallback[40]{};
        std::snprintf(fallback, sizeof(fallback), "Ability %u", timer);
        return fallback;
    }

    auto recast_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        if (now < this->next_update_) return;
        this->next_update_ = now + 250;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        auto* recast = memory != nullptr ? memory->GetRecast() : nullptr;
        auto* resources = core != nullptr ? core->GetResourceManager() : nullptr;
        if (player == nullptr || party == nullptr || recast == nullptr || resources == nullptr ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) || player->GetIsZoning() != 0 ||
            party->GetMemberServerId(0) == 0 || party->GetMemberZone(0) == 0)
        { this->Clear(); this->next_update_ = now + 250; return; }
        if (this->player_ != party->GetMemberServerId(0) || this->zone_ != party->GetMemberZone(0) ||
            this->job_ != player->GetMainJob() || this->subjob_ != player->GetSubJob() ||
            this->level_ != player->GetMainJobLevel() || this->sublevel_ != player->GetSubJobLevel())
        {
            this->tracker_.Clear();
            this->player_ = party->GetMemberServerId(0); this->zone_ = party->GetMemberZone(0);
            this->job_ = player->GetMainJob(); this->subjob_ = player->GetSubJob();
            this->level_ = player->GetMainJobLevel(); this->sublevel_ = player->GetSubJobLevel();
            this->known_names_.clear(); this->known_timers_.fill(false);
            // Blood Pact: Rage/Ward are unlisted while no avatar is summoned.
            const bool summoner = this->job_ == 15 || this->subjob_ == 15;
            for (uint32_t abilityId = 0x200; abilityId < 0x700; ++abilityId)
                if (const auto* ability = resources->GetAbilityById(abilityId); ability && (player->HasAbility(ability->Id) || player->HasPetCommand(ability->Id) ||
                    (summoner && ability->Id < 0x400 && (ability->RecastTimerId == 173 || ability->RecastTimerId == 174))))
                {
                    if (ability->RecastTimerId < this->known_timers_.size()) this->known_timers_[ability->RecastTimerId] = true;
                    const auto* name = ability->Name[2] ? ability->Name[2] : ability->Name[0];
                    if (name && *name) this->known_names_.insert(FromGameText(name));
                }
        }
        this->available_ = true;
        std::array<bool, 256> seen{};
        for (uint32_t slot = 0; slot < 32; ++slot)
        {
            const auto timer = recast->GetAbilityTimerId(slot), frames = recast->GetAbilityTimer(slot);
            if (frames == 0 || timer >= seen.size() || (timer == 0 && slot != 0) || timer == 92 || seen[timer]) continue;
            seen[timer] = true;
            recast_sample sample{recast_kind::ability, timer};
            sample.remaining = frames / 60.0f;
            sample.name = this->AbilityName(resources, player, timer);
            const auto pool = AbilityChargePool(core, timer);
            const auto modifier = static_cast<int16_t>(recast->GetAbilityCalc2(slot));
            if (const auto charges = CalculateChargeRecast(frames, pool, modifier); charges.maximum != 0)
            {
                sample.charges = charges.charges;
                sample.maximum_charges = charges.maximum;
                sample.remaining = charges.remaining;
                sample.charge_duration = charges.duration;
            }
            this->tracker_.Observe(sample, now);
        }
        for (uint32_t id = 0; id < 1024; ++id)
        {
            const auto frames = recast->GetSpellTimer(id);
            if (frames == 0) continue;
            const auto* spell = resources->GetSpellById(id);
            if (spell == nullptr) continue;
            const char* name = spell->Name[2] != nullptr ? spell->Name[2] : spell->Name[0];
            if (name == nullptr || !*name) continue;
            recast_sample sample{recast_kind::spell, id};
            sample.remaining = frames / 60.0f; sample.name = FromGameText(name);
            this->tracker_.Observe(sample, now);
        }
        this->tracker_.FinishSample(now);
    }
}
