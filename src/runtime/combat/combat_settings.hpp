#pragma once

#include "server_rules.hpp"
#include "settings/settings_store.hpp"

namespace xiui
{
    class combat_settings
    {
    public:
        auto Rules(void) const -> server_rules { return ServerRulesFromIndex(this->rules_choice_); }
        auto SetRules(const server_rules rules) -> void { this->rules_choice_ = ServerRulesIndex(rules); }
        auto ResetDefaults(void) -> void { this->SetRules(server_rules::retail); }

        auto RulesChoice(void) -> int*
        {
            this->rules_choice_ = ServerRulesIndex(this->Rules());
            return &this->rules_choice_;
        }

        auto Load(const settings_store& settings) -> void
        {
            const auto legacy = settings.ReadInt("magicburst", "rules", 0);
            this->SetRules(ServerRulesFromIndex(settings.ReadInt("actionbars", "skillchain_rules", legacy)));
        }

        auto Save(const settings_store& settings) const -> void
        {
            const auto rules = ServerRulesIndex(this->Rules());
            settings.WriteInt("actionbars", "skillchain_rules", rules);
            // Keep the legacy roll key compatible with existing profiles.
            settings.WriteInt("phantomroll", "rules", rules);
        }

    private:
        // Shared choice controls require integer storage.
        int rules_choice_{};
    };
}
