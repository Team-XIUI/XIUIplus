#pragma once

#include "Ashita.h"

#include <string>

namespace xiui
{
    enum class action_cost_kind : uint8_t { none, spell, ability, weapon_skill, mount };

    struct action_cost_state
    {
        action_cost_kind kind{};
        uint32_t id{}, mp_cost{}, tp_cost{}, player_mp{}, player_tp{};
        uint32_t charges{}, max_charges{}, charge_cost{};
        float cast_seconds{}, base_recast{}, remaining{}, total{};
        bool recast_known{};
        std::string name{};
        auto Valid(void) const -> bool { return this->kind != action_cost_kind::none && !this->name.empty(); }
        auto EnoughResources(void) const -> bool
        {
            return this->player_mp >= this->mp_cost && this->player_tp >= this->tp_cost &&
                (this->max_charges == 0 || this->charges >= this->charge_cost);
        }
        auto Ready(void) const -> bool
        {
            return this->Valid() && this->EnoughResources() && this->recast_known && (this->max_charges > 0 || this->remaining <= 0);
        }
    };

    auto FormatCastTime(float seconds) -> std::string;

    struct selected_spell_mp_cost
    {
        bool available{}, selected{};
        uint32_t player_id{}, cost{};
        uint64_t sampled_at{};
        auto Fresh(uint64_t now) const -> bool
        { return this->available && this->selected && this->player_id != 0 && now >= this->sampled_at && now - this->sampled_at <= 250; }
    };

    class cast_cost_source
    {
    public:
        auto Initialize(void) -> void;
        auto Clear(void) -> void { this->state_ = {}; this->mp_preview_ = {}; this->next_update_ = 0; }
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto State(void) const -> const action_cost_state& { return this->state_; }
        auto AvailableMask(void) const -> uint32_t;
        auto MPPreview(uint64_t now) const -> selected_spell_mp_cost
        {
            auto preview = this->mp_preview_;
            if (!preview.Fresh(now)) preview.selected = false;
            return preview;
        }

    private:
#if defined(XIUI_ENABLE_CAST_TESTS)
        friend struct casting_test;
#endif
        action_cost_state state_{};
        selected_spell_mp_cost mp_preview_{};
        uintptr_t menu_slot_{}, spell_slot_{}, ability_slot_{}, mount_slot_{};
        uintptr_t spell_item_{}, ability_item_{}, mount_item_{};
        uint64_t next_update_{};
        uint32_t player_{}, zone_{};
        uint8_t main_job_{}, sub_job_{}, main_level_{}, sub_level_{};
        auto Selection(void) const -> std::pair<action_cost_kind, int32_t>;
        auto ReadResource(IAshitaCore* core, action_cost_kind kind, uint32_t id) -> void;
        auto ReadRecast(IAshitaCore* core) -> void;
    };
}
