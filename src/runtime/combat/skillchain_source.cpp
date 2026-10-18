#include "skillchain_source.hpp"
#include "combat_source.hpp"
#include "modules/actionbars/action_executor.hpp"
#include "chat/chat_encoding.hpp"
#include "resources/skillchain_data.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <tuple>

namespace xiui
{
    namespace
    {
        constexpr auto Bit(const skillchain_property value) -> uint32_t { return 1u << static_cast<uint8_t>(value); }
        constexpr uint16_t buff_ids[]{163, 164, 470};
        constexpr uint64_t buff_seconds[]{30, 30, 60};
        struct combination { uint8_t result, opening, closing; };
        constexpr combination combinations[]{
            {13,13,13},{13,12,9},{13,9,12},{14,14,14},{14,11,10},{14,10,11},
            {9,1,5},{9,11,9},{10,3,8},{10,9,10},{11,7,4},{11,12,11},{12,2,6},{12,10,12},
            {1,5,1},{1,4,1},{4,1,4},{4,3,4},{6,4,6},{6,7,6},{3,4,3},{3,5,3},{3,8,3},
            {2,6,2},{5,6,5},{5,2,5},{7,8,7},{8,7,8},{8,2,8}};
        auto Normalize(const std::string_view value) -> std::string
        {
            std::string result;
            for (const unsigned char c : value) if (std::isalnum(c)) result += static_cast<char>(std::tolower(c));
            return result;
        }
        auto Hit(const uint32_t message) -> bool
        {
            switch (message) { case 2: case 103: case 110: case 185: case 187: case 238: case 317: case 802: case 529: return true; default: return false; }
        }
    }

    auto SkillchainName(const skillchain_property property) -> const char*
    {
        static constexpr const char* names[]{"", "Liquefaction", "Induration", "Detonation", "Scission", "Impaction", "Reverberation",
            "Transfixion", "Compression", "Fusion", "Gravitation", "Distortion", "Fragmentation", "Light", "Darkness", "Light", "Darkness", "Radiance", "Umbra"};
        const auto index = static_cast<size_t>(property);
        return index < std::size(names) ? names[index] : "";
    }
    auto SkillchainElements(const skillchain_property property) -> uint8_t
    {
        // Element bits follow the native Fire/Ice/Wind/Earth/Lightning/Water/Light/Dark order.
        static constexpr uint8_t masks[]{0,1,2,4,8,16,32,64,128,65,136,34,20,85,170,85,170,85,170};
        const auto index = static_cast<size_t>(property);
        return index < std::size(masks) ? masks[index] : 0;
    }
    auto SkillchainColor(const skillchain_property property) -> D3DCOLOR
    {
        static constexpr D3DCOLOR colors[]{0xFFFFFFFF,0xFFFF8B69,0xFF91DAF4,0xFF8DDE9A,0xFFE7C17A,0xFFC69BF2,
            0xFF88BCFF,0xFFFFEDB8,0xFFC09BDD,0xFFFFB978,0xFFC1A077,0xFF9ED6F2,0xFFA5DCA0,
            0xFFFFEAC1,0xFFB9A0E2,0xFFFFEAC1,0xFFB9A0E2,0xFFFFF3D6,0xFFCBB6ED};
        const auto index = static_cast<size_t>(property);
        return index < std::size(colors) ? colors[index] : 0xFFFFFFFF;
    }
    auto SkillchainFromMessage(const uint32_t message) -> skillchain_property
    {
        static constexpr uint8_t properties[]{13,14,10,12,11,9,8,1,2,6,7,4,3,5};
        if (message >= 288 && message <= 301) return static_cast<skillchain_property>(properties[message - 288]);
        if (message >= 385 && message <= 398) return static_cast<skillchain_property>(properties[message - 385]);
        if (message == 767 || message == 769) return skillchain_property::radiance;
        if (message == 768 || message == 770) return skillchain_property::umbra;
        return skillchain_property::none;
    }

    auto skillchain_source::Clear(void) -> void
    {
        this->targets_.clear(); this->buffs_.clear(); this->player_ = this->zone_ = this->local_actor_ = this->target_ = 0;
        this->local_buffs_ = 0; this->next_update_ = 0; this->pet_.clear();
    }
    auto skillchain_source::SetRules(const server_rules rules) -> void
    {
        if (this->rules_ == rules) return;
        this->Clear();
        this->names_.clear();
        this->rules_ = rules;
    }
    auto skillchain_source::Skill(const uint8_t category, const uint32_t id, const std::string_view name) const -> const resources::skillchain_skill*
    {
        const std::span<const resources::skillchain_skill> table = this->rules_ == server_rules::phoenix ? std::span<const resources::skillchain_skill>{resources::skillchain_phoenix} :
            this->rules_ == server_rules::horizon ? std::span<const resources::skillchain_skill>{resources::skillchain_horizon} : std::span<const resources::skillchain_skill>{resources::skillchain_retail};
        const auto key = std::pair{category, id};
        const auto found = std::lower_bound(table.begin(), table.end(), key, [](const auto& row, const auto& value) {
            return std::pair{row.category, uint32_t{row.id}} < value;
        });
        if (id != 0 && found != table.end() && found->category == category && found->id == id) return &*found;
        if (name.empty()) return nullptr;
        const auto normalized = Normalize(name), cacheKey = std::to_string(category) + ":" + normalized;
        if (const auto cached = this->names_.find(cacheKey); cached != this->names_.end()) return cached->second;
        const auto match = std::find_if(table.begin(), table.end(), [&](const auto& row) { return row.category == category && Normalize(row.name) == normalized; });
        const auto* result = match != table.end() ? &*match : nullptr;
        if (this->names_.size() >= 1024) this->names_.clear();
        this->names_.emplace(cacheKey, result);
        return result;
    }
    auto skillchain_source::Buffs(const uint32_t actor, const uint64_t now) const -> uint8_t
    {
        if (actor == this->local_actor_ && actor != 0) return this->local_buffs_;
        uint8_t result{};
        if (const auto found = this->buffs_.find(actor); found != this->buffs_.end())
            for (size_t i = 0; i < 3; ++i) if (found->second.expires[i] > now) result |= static_cast<uint8_t>(1u << i);
        return result;
    }
    auto skillchain_source::Put(const uint32_t target, const uint64_t now) -> skillchain_window&
    {
        if (!this->targets_.contains(target) && this->targets_.size() >= capacity)
        {
            const auto oldest = std::min_element(this->targets_.begin(), this->targets_.end(), [](const auto& a, const auto& b) { return a.second.updated < b.second.updated; });
            this->targets_.erase(oldest);
        }
        auto& result = this->targets_[target]; result.target = target; result.updated = now; return result;
    }
    auto skillchain_source::Find(const uint32_t target, const uint64_t now) const -> const skillchain_window*
    {
        const auto found = this->targets_.find(target);
        return found != this->targets_.end() && now >= found->second.updated && now < found->second.closes ? &found->second : nullptr;
    }
    auto skillchain_source::Prune(const uint64_t now) -> void
    {
        std::erase_if(this->targets_, [now](const auto& row) { return now < row.second.updated || now >= row.second.closes; });
        std::erase_if(this->buffs_, [now](const auto& row) { return std::all_of(row.second.expires.begin(), row.second.expires.end(), [now](auto expires) { return now >= expires; }); });
    }
    auto skillchain_source::Observe(const combat_action& action, const uint64_t now, const uint8_t spellElement) -> void
    {
        (void)spellElement;
        if (action.actor == 0 || action.target_count == 0 || action.target_count > action.targets.size()) return;
        const auto category = action.category;
        if (category != 3 && category != 4 && category != 6 && category != 11 && category != 13 && category != 14) return;
        const uint32_t id = action.parameter & 0xFFFF;
        const uint8_t actorBuffs = this->Buffs(action.actor, now);
        bool consumeSpellBuff{};
        for (const auto& target : std::span{action.targets}.first(action.target_count))
        {
            if (target.id == 0 || target.result_count > target.results.size()) continue;
            const action_result* hit{};
            auto chain = skillchain_property::none;
            bool died{};
            for (const auto& result : std::span{target.results}.first(target.result_count))
            {
                died |= result.message == 6 || result.message == 20;
                if (Hit(result.message) && hit == nullptr) hit = &result;
                if (chain == skillchain_property::none) chain = SkillchainFromMessage(result.additional_message);
                if (result.message == 100)
                    for (size_t i = 0; i < 3; ++i) if (result.parameter == buff_ids[i])
                    {
                        if (this->buffs_.size() >= capacity && !this->buffs_.contains(target.id)) this->buffs_.erase(this->buffs_.begin());
                        this->buffs_[target.id].expires[i] = now + buff_seconds[i] * 1000;
                    }
            }
            if (died) { this->Remove(target.id); continue; }
            if (chain != skillchain_property::none)
            {
                uint8_t depth{1}; auto property = chain;
                if (const auto* previous = this->Find(target.id, now); previous != nullptr && now + 1000 >= previous->opens)
                {
                    depth = std::min<uint8_t>(5, previous->depth + 1);
                    if (chain == skillchain_property::light && (previous->properties & Bit(skillchain_property::light))) property = skillchain_property::light2;
                    if (chain == skillchain_property::darkness && (previous->properties & Bit(skillchain_property::darkness))) property = skillchain_property::darkness2;
                }
                auto& window = this->Put(target.id, now);
                window.properties = Bit(property); window.chain = property; window.depth = depth;
                window.order = static_cast<uint8_t>(property);
                window.opens = now + (this->rules_ == server_rules::phoenix ? 3001 : 3500);
                window.closes = now + 9800 - uint64_t{depth} * 1000;
                window.burst_started = now;
                consumeSpellBuff |= category == 4;
                continue;
            }
            if (hit == nullptr) continue;
            uint8_t lookup = static_cast<uint8_t>(category == 6 ? 14 : category);
            if (hit->message == 110 || hit->message == 317) lookup = 13;
            auto* skill = this->Skill(lookup, id);
            if (skill == nullptr && lookup != category) skill = this->Skill(static_cast<uint8_t>(category), id);
            if (skill == nullptr && category == 11) skill = this->Skill(21, id);
            uint32_t properties = skill != nullptr ? skill->properties : 0;
            if (category == 4)
            {
                const bool blue = id >= 512 && id < 768;
                if ((actorBuffs & (blue ? 3 : 4)) == 0) properties = 0;
            }
            if (properties != 0)
            {
                auto& window = this->Put(target.id, now);
                window.properties = properties; window.chain = skillchain_property::none; window.depth = 0;
                window.order = skill->order;
                window.opens = now + (this->rules_ == server_rules::phoenix ? 3001 : 0);
                window.closes = now + 10000;
                window.burst_started = 0;
                consumeSpellBuff |= category == 4;
            }
            else if (category == 3) this->Remove(target.id);
        }
        if (consumeSpellBuff)
        {
            if (action.actor == this->local_actor_) this->local_buffs_ &= 1;
            if (auto found = this->buffs_.find(action.actor); found != this->buffs_.end()) found->second.expires[1] = found->second.expires[2] = 0;
        }
        this->Prune(now);
    }
    auto skillchain_source::Predict(const uint32_t target, const uint8_t category, const uint32_t id, const std::string_view name,
        const uint64_t now, const uint8_t element) const -> skillchain_prediction
    {
        const auto* window = this->Find(target, now);
        if (window == nullptr) return {};
        skillchain_prediction prediction{};
        if (category == 4 && element < 8 && window->Burst(now) && (SkillchainElements(window->chain) & (1u << element)))
        { prediction.burst = true; prediction.elements = SkillchainElements(window->chain); }
        if (now < window->opens || window->depth >= 5) return prediction;
        const auto* skill = this->Skill(category, id, name);
        if (skill == nullptr && category == 11) skill = this->Skill(21, id, name);
        if (skill == nullptr && category == 13) { skill = this->Skill(11, id, name); if (skill == nullptr) skill = this->Skill(21, id, name); }
        uint32_t properties = skill != nullptr ? skill->properties : 0;
        if (category == 4)
        {
            const bool blue = id >= 512 && id < 768;
            if ((this->local_buffs_ & (blue ? 3 : 4)) == 0) properties = 0;
        }
        if (skill != nullptr && !skill->avatar.empty() && Normalize(skill->avatar) != Normalize(this->pet_)) properties = 0;
        if (properties != 0)
            for (auto opening = window->order; opening != 0; opening >>= 5)
                for (auto closing = skill->order; closing != 0; closing >>= 5)
                    for (const auto& combination : combinations)
                        if ((opening & 31) == combination.opening && (closing & 31) == combination.closing)
                        { prediction.chain = static_cast<skillchain_property>(combination.result); return prediction; }
        return prediction;
    }
    auto skillchain_source::PredictSlot(const action_slot& slot, IResourceManager* resources, const uint64_t now) const -> skillchain_prediction
    {
        if (this->Current(now) == nullptr || resources == nullptr) return {};
        auto action = slot;
        if (action.kind == action_kind::macro || action.kind == action_kind::command)
        { const auto representative = action_executor::RepresentativeAction(action); if (!representative) return {}; action = *representative; }
        if (!action.target.empty() && action.target != "<t>") return {};
        if (action.kind == action_kind::spell)
        {
            std::string name;
            if (action.id == 0 && !ToGameText(action.name, name)) return {};
            const auto* spell = action.id != 0 ? resources->GetSpellById(action.id) : resources->GetSpellByName(name.c_str(), 2);
            if (spell == nullptr || (spell->Targets & 0x20) == 0) return {};
            return this->Predict(this->target_, 4, spell->Index, action.name, now, spell->Element < 8 ? static_cast<uint8_t>(spell->Element) : 255);
        }
        if (action.kind != action_kind::weapon_skill && action.kind != action_kind::ability && action.kind != action_kind::pet) return {};
        const uint8_t category = action.kind == action_kind::weapon_skill ? 3 : action.kind == action_kind::pet ? 13 : 14;
        if (category == 3) return this->Predict(this->target_, category, action.id, action.name, now);
        std::string name;
        if (action.id == 0 && !ToGameText(action.name, name)) return {};
        const auto* ability = action.id != 0 ? resources->GetAbilityById(action.id) : resources->GetAbilityByName(name.c_str(), 2);
        const auto label = ability != nullptr && ability->Name[2] != nullptr ? FromGameText(ability->Name[2]) : action.name;
        return this->Predict(this->target_, category, 0, label, now);
    }
    auto skillchain_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (player == nullptr || party == nullptr || entity == nullptr || target == nullptr || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) || party->GetMemberServerId(0) == 0 || party->GetMemberHP(0) == 0)
        { this->Clear(); return; }
        const auto actor = party->GetMemberServerId(0); const auto zone = party->GetMemberZone(0);
        if (actor != this->player_ || zone != this->zone_) { this->Clear(); this->player_ = actor; this->zone_ = zone; }
        const auto index = target->GetTargetIndex(0);
        this->target_ = index > 0 && index < entity->GetEntityMapSize() && (entity->GetSpawnFlags(index) & 0x10) && entity->GetHPPercent(index) > 0 ? entity->GetServerId(index) : 0;
        if (now < this->next_update_) return;
        this->next_update_ = now + 100;
        uint8_t mask{};
        if (const auto* buffs = player->GetBuffs())
            for (size_t i = 0; i < 32; ++i) for (size_t j = 0; j < 3; ++j) if (buffs[i] == buff_ids[j]) mask |= static_cast<uint8_t>(1u << j);
        this->SetLocalBuffs(actor, mask);
        this->pet_.clear();
        const auto pet = entity->GetPetTargetIndex(party->GetMemberTargetIndex(0));
        if (pet > 0 && pet < entity->GetEntityMapSize() && entity->GetName(pet) != nullptr) this->pet_ = FromGameText(entity->GetName(pet));
        this->Prune(now);
        for (auto& [id, window] : this->targets_)
            if (window.index == 0) window.index = FindEntityIndex(*core, id);
        std::erase_if(this->targets_, [&](const auto& row) {
            const auto& window = row.second;
            return window.index == 0 || window.index >= entity->GetEntityMapSize() || entity->GetServerId(window.index) != window.target || entity->GetHPPercent(window.index) == 0;
        });
    }
    auto skillchain_source::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        this->Update(core, now);
        if (this->player_ == 0) return;
        if (id == 0x28)
        {
            if (const auto action = DecodeCombatAction(data))
            {
                uint8_t element{255};
                if (action->category == 4 && core->GetResourceManager() != nullptr)
                    if (const auto* spell = core->GetResourceManager()->GetSpellById(action->parameter & 0xFFFF); spell != nullptr && spell->Element < 8) element = static_cast<uint8_t>(spell->Element);
                this->Observe(*action, now, element);
            }
        }
        else if (id == 0x29 && data.size() >= 0x1C)
        {
            uint32_t target{}, parameter{}; uint16_t message{};
            std::memcpy(&target, data.data() + 8, 4); std::memcpy(&parameter, data.data() + 12, 4); std::memcpy(&message, data.data() + 24, 2); message &= 0x7FFF;
            if (message == 6 || message == 20) this->Remove(target);
            if (message == 206 || message == 204)
                for (size_t i = 0; i < 3; ++i) if (parameter == buff_ids[i])
                { if (auto found = this->buffs_.find(target); found != this->buffs_.end()) found->second.expires[i] = 0; }
        }
    }
}
