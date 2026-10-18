#pragma once

#include "Ashita.h"
#include "action_packet.hpp"
#include "server_rules.hpp"
#include "modules/actionbars/action_model.hpp"

#include <map>
#include <optional>
#include <unordered_map>

namespace xiui::resources { struct skillchain_skill; }

namespace xiui
{
    enum class skillchain_property : uint8_t { none, liquefaction, induration, detonation, scission, impaction,
        reverberation, transfixion, compression, fusion, gravitation, distortion, fragmentation, light, darkness,
        light2, darkness2, radiance, umbra };
    auto SkillchainName(skillchain_property property) -> const char*;
    auto SkillchainElements(skillchain_property property) -> uint8_t;
    auto SkillchainColor(skillchain_property property) -> D3DCOLOR;
    auto SkillchainFromMessage(uint32_t message) -> skillchain_property;

    struct skillchain_window
    {
        uint32_t target{}, index{}, properties{};
        uint64_t order{};
        uint8_t depth{};
        skillchain_property chain{};
        uint64_t updated{}, opens{}, closes{}, burst_started{};
        auto Burst(uint64_t now) const -> bool { return this->chain != skillchain_property::none && now >= this->burst_started && now < this->closes; }
    };
    struct skillchain_prediction
    {
        skillchain_property chain{};
        bool burst{};
        uint8_t elements{};
        auto Active(void) const -> bool { return this->chain != skillchain_property::none || this->burst; }
    };

    class skillchain_source
    {
    public:
        static constexpr size_t capacity = 128;
        auto Clear(void) -> void;
        auto SetRules(server_rules rules) -> void;
        auto Rules(void) const -> server_rules { return this->rules_; }
        auto SetLocalBuffs(uint32_t actor, uint8_t buffs) -> void { this->local_actor_ = actor; this->local_buffs_ = buffs; }
        auto Observe(const combat_action& action, uint64_t now, uint8_t spellElement = 255) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto Prune(uint64_t now) -> void;
        auto Remove(uint32_t target) -> void { this->targets_.erase(target); }
        auto Find(uint32_t target, uint64_t now) const -> const skillchain_window*;
        auto Current(uint64_t now) const -> const skillchain_window* { return this->Find(this->target_, now); }
        auto Count(void) const -> size_t { return this->targets_.size(); }
        auto Target(void) const -> uint32_t { return this->target_; }
        auto Predict(uint32_t target, uint8_t category, uint32_t id, std::string_view name, uint64_t now, uint8_t element = 255) const -> skillchain_prediction;
        auto PredictSlot(const action_slot& slot, IResourceManager* resources, uint64_t now) const -> skillchain_prediction;

    private:
        struct actor_buffs { std::array<uint64_t, 3> expires{}; };
        std::map<uint32_t, skillchain_window> targets_{};
        std::map<uint32_t, actor_buffs> buffs_{};
        mutable std::unordered_map<std::string, const resources::skillchain_skill*> names_{};
        uint32_t player_{}, zone_{}, local_actor_{}, target_{};
        uint8_t local_buffs_{};
        server_rules rules_{};
        uint64_t next_update_{};
        std::string pet_{};
        auto Skill(uint8_t category, uint32_t id, std::string_view name = {}) const -> const resources::skillchain_skill*;
        auto Buffs(uint32_t actor, uint64_t now) const -> uint8_t;
        auto Put(uint32_t target, uint64_t now) -> skillchain_window&;
    };
}
