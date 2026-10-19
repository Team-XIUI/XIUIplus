#pragma once

#include "modules/module.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace xiui
{
    struct module_visibility_context
    {
        bool event_active{};
        bool dialogue_active{};
        bool menu_open{};
        bool macro_palette{};
        bool map_open{};
        bool interface_hidden{};
        bool in_party{};
        bool macro_available{};
        bool menu_available{};
        bool event_available{};
        bool interface_available{};
    };

    struct module_visibility_rule
    {
        bool hide_events{};
        bool hide_menu{};
        bool keep_macro{true};
    };

    class module_visibility_settings
    {
    public:
        static constexpr size_t option_count = 6;
        auto SetModules(std::span<hud_module* const> modules) -> void;
        auto Reset(void) -> void;
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        auto Visible(std::string_view module, const module_visibility_context& context, bool editing = false) const -> bool;
        auto SetContext(const module_visibility_context& context) -> void { this->macro_available_ = context.macro_available; }

    private:
        struct entry { hud_module* module{}; module_visibility_rule rule{}; };
        std::vector<entry> modules_{};
        std::vector<const char*> scopes_{"All modules"};
        module_visibility_rule global_{};
        bool alliance_only_menu_{};
        bool player_in_party_{};
        bool macro_available_{};
        int scope_{};
        auto SelectedRule(void) -> module_visibility_rule&;
    };

    class module_visibility_source
    {
        friend struct config_input_test;

    public:
        auto Initialize(void) -> void;
        auto Read(IAshitaCore* core, bool nativeMenuOpen) const -> module_visibility_context;

    private:
        uintptr_t menu_slot_{};
        uintptr_t event_slot_{};
        uintptr_t event_owner_slot_{};
        uintptr_t event_entity_slot_{};
        uintptr_t player_entity_slot_{};
        uintptr_t interface_object_{};
    };
} // namespace xiui
