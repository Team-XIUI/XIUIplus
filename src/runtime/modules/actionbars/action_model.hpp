#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace xiui
{
    inline constexpr size_t action_bar_count = 10, action_bar_slots = 12, action_crossbar_banks = 6, action_crossbar_slots = 8;
    enum class action_kind : uint8_t { none, spell, ability, weapon_skill, item, equip, pet, macro, native_macro, command };
    enum class action_surface : uint8_t { hotbar, crossbar };
    enum class action_scope : uint8_t { global, shared_subjob, job_subjob };

    struct native_macro_ref
    {
        uint8_t book{1}, set{1}, slot{1};
        bool alt{};
        auto operator==(const native_macro_ref&) const -> bool = default;
    };

    struct action_slot
    {
        action_kind kind{};
        uint32_t id{};
        std::string name{}, target{"<t>"}, label{}, icon{}, equipment_slot{}, macro_id{};
        std::vector<std::string> lines{};
        native_macro_ref native{};
        auto Empty(void) const -> bool { return this->kind == action_kind::none; }
        auto operator==(const action_slot&) const -> bool = default;
    };

    struct action_context
    {
        uint8_t job{}, subjob{};
        std::string pet{};
        auto operator==(const action_context&) const -> bool = default;
    };

    struct action_macro
    {
        std::string id{}, name{}, icon{};
        std::vector<std::string> lines{};
    };

    struct action_layout
    {
        std::array<std::array<action_slot, action_bar_slots>, action_bar_count> hotbar{};
        std::array<std::array<action_slot, action_crossbar_slots>, action_crossbar_banks> crossbar{};
    };

    struct action_pet_layout
    {
        std::string pet{};
        action_layout actions{};
    };

    struct action_palette
    {
        std::string id{}, name{"Default"};
        uint8_t job{}, subjob{};
        action_layout actions{};
        std::vector<action_pet_layout> pets{};
    };

    struct action_catalog_entry
    {
        action_kind kind{};
        uint32_t id{};
        std::string name{}, icon{};
        uint32_t mp{}, tp{}, timer{};
        bool learned{};
    };

    struct action_availability
    {
        std::string name{}, reason{}, icon{};
        bool known{}, usable{}, learned{}, recast_known{};
        float remaining{}, total{};
        uint32_t mp{}, tp{}, charges{}, maximum_charges{}, item_count{};
    };

    auto ActionKindName(action_kind kind) -> const char*;
    auto ActionKindFromName(const std::string& name) -> action_kind;
    auto ValidateAction(const action_slot& slot) -> std::string;
}
