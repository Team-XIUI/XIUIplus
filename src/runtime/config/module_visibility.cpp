#include "module_visibility.hpp"

#include <algorithm>
#include <string>

namespace xiui
{
    namespace
    {
        auto PartyGroup(const std::string_view id) -> bool
        {
            return id == "party" || id == "alliance1" || id == "alliance2";
        }
    }

    auto module_visibility_settings::SetModules(const std::span<hud_module* const> modules) -> void
    {
        this->modules_.clear();
        this->scopes_ = {"All modules"};
        for (auto* module : modules)
        {
            if (module == nullptr || std::string_view{module->Id()} == "general" || std::string_view{module->Id()} == "gamehud") continue;
            this->modules_.push_back({module, {PartyGroup(module->Id()), false, true}});
            this->scopes_.push_back(module->DisplayName());
        }
        this->scope_ = 0;
    }

    auto module_visibility_settings::Reset(void) -> void
    {
        this->global_ = {};
        this->alliance_only_menu_ = false;
        this->player_in_party_ = false;
        for (auto& value : this->modules_) value.rule = {PartyGroup(value.module->Id()), false, true};
    }

    auto module_visibility_settings::Load(const settings_store& store) -> void
    {
        this->global_.hide_events = store.ReadBool("visibility", "hide_events", false);
        this->global_.hide_menu = store.ReadBool("visibility", "hide_menu", false);
        this->global_.keep_macro = store.ReadBool("visibility", "keep_macro", true);
        this->alliance_only_menu_ = store.ReadBool("visibility", "alliance_only_menu", false);
        this->player_in_party_ = store.ReadBool("visibility", "player_in_party", false);
        for (auto& value : this->modules_)
        {
            const auto* id = value.module->Id();
            const auto section = std::string{"visibility."} + id;
            value.rule.hide_events = store.ReadBool(section.c_str(), "hide_events", store.ReadBool(id, "hide_events", PartyGroup(id)));
            value.rule.hide_menu = store.ReadBool(section.c_str(), "hide_menu", store.ReadBool(id, "hide_menu", false));
            value.rule.keep_macro = store.ReadBool(section.c_str(), "keep_macro", true);
        }
    }

    auto module_visibility_settings::Save(const settings_store& store) const -> void
    {
        store.WriteBool("visibility", "hide_events", this->global_.hide_events);
        store.WriteBool("visibility", "hide_menu", this->global_.hide_menu);
        store.WriteBool("visibility", "keep_macro", this->global_.keep_macro);
        store.WriteBool("visibility", "alliance_only_menu", this->alliance_only_menu_);
        store.WriteBool("visibility", "player_in_party", this->player_in_party_);
        for (const auto& value : this->modules_)
        {
            const auto section = std::string{"visibility."} + value.module->Id();
            store.WriteBool(section.c_str(), "hide_events", value.rule.hide_events);
            store.WriteBool(section.c_str(), "hide_menu", value.rule.hide_menu);
            store.WriteBool(section.c_str(), "keep_macro", value.rule.keep_macro);
        }
    }

    auto module_visibility_settings::SelectedRule(void) -> module_visibility_rule&
    {
        if (this->scope_ <= 0 || static_cast<size_t>(this->scope_) > this->modules_.size()) return this->global_;
        return this->modules_[static_cast<size_t>(this->scope_ - 1)].rule;
    }

    auto module_visibility_settings::Option(const size_t index) -> module_option
    {
        auto& rule = this->SelectedRule();
        const auto party = std::find_if(this->modules_.begin(), this->modules_.end(), [](const entry& value) { return std::string_view{value.module->Id()} == "party"; });
        const bool partyHides = party != this->modules_.end() && party->rule.hide_menu;
        const bool allScope = this->scope_ <= 0 || static_cast<size_t>(this->scope_) > this->modules_.size();
        const bool anyMenuHiding = this->global_.hide_menu || std::any_of(this->modules_.begin(), this->modules_.end(), [](const entry& value) { return value.rule.hide_menu; });
        bool selectedMenuHiding = this->global_.hide_menu || rule.hide_menu;
        if (allScope) selectedMenuHiding = anyMenuHiding;
        else if (this->alliance_only_menu_ && partyHides)
        {
            const std::string_view id = this->modules_[static_cast<size_t>(this->scope_ - 1)].module->Id();
            selectedMenuHiding |= id == "alliance1" || id == "alliance2";
        }
        const bool partyMenuHiding = this->global_.hide_menu || partyHides || std::any_of(this->modules_.begin(), this->modules_.end(), [](const entry& value) {
            const std::string_view id = value.module->Id();
            return (id == "alliance1" || id == "alliance2") && value.rule.hide_menu;
        });
        switch (index)
        {
        case 0: return module_option::Dropdown("Apply to", "All modules adds a shared rule. Select a module to add its own hiding preferences.", &this->scope_, this->scopes_.data(), this->scopes_.size()).Ordered(-40);
        case 1: return module_option::Toggle("Hide during events", "Temporarily hide during native cutscenes. The shared rule also applies to each module.", &rule.hide_events);
        case 2: return module_option::Toggle("Hide with menus", "Temporarily hide with native menus. Chat input and combat command lists stay usable. Shared hiding applies to each module.", &rule.hide_menu);
        case 3:
        {
            auto option = module_option::Toggle("Keep visible with macros", "Keep visible with the native macro palette while menu hiding is enabled. Both shared and module preferences apply.", &rule.keep_macro);
            option.available = this->macro_available_;
            option.visible = selectedMenuHiding;
            if (!option.available) option.description = "Native macro-palette identity is unavailable on this client.";
            return option;
        }
        case 4: return module_option::Toggle("Keep Party with menus", "Menu hiding affects the alliance panels while the main Party panel stays visible. Other module rules are unchanged.", &this->alliance_only_menu_).VisibleWhen(partyMenuHiding);
        case 5: return module_option::Toggle("Hide Player in party", "Hide Player when another actual party or alliance member is present. Preview rows do not count.", &this->player_in_party_);
        }
        return {};
    }

    auto module_visibility_settings::Visible(const std::string_view module, const module_visibility_context& context, const bool editing) const -> bool
    {
        if (editing) return true;
        if (context.map_open || context.interface_hidden) return false;
        const auto found = std::find_if(this->modules_.begin(), this->modules_.end(), [&](const entry& value) { return module == value.module->Id(); });
        const module_visibility_rule fallback{};
        const auto& rule = found != this->modules_.end() ? found->rule : fallback;
        if (context.event_active && (this->global_.hide_events || rule.hide_events)) return false;
        if (module == "player" && this->player_in_party_ && context.in_party) return false;
        bool hideMenu = this->global_.hide_menu || rule.hide_menu;
        bool keepMacro = this->global_.keep_macro && rule.keep_macro;
        if (this->alliance_only_menu_ && (module == "alliance1" || module == "alliance2"))
        {
            const auto party = std::find_if(this->modules_.begin(), this->modules_.end(), [](const entry& value) { return std::string_view{value.module->Id()} == "party"; });
            if (party != this->modules_.end() && party->rule.hide_menu)
            {
                hideMenu = true;
                keepMacro = keepMacro && party->rule.keep_macro;
            }
        }
        if (!context.menu_open || !hideMenu) return true;
        if (module == "party" && this->alliance_only_menu_) return true;
        return context.macro_palette && keepMacro;
    }
} // namespace xiui
