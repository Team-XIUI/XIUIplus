#pragma once

namespace xiui
{
    enum class server_rules : int { retail, horizon, phoenix };

    inline constexpr const char* server_rule_names[]{"Retail", "Horizon", "Phoenix (75 cap)"};

    constexpr auto ServerRulesFromIndex(const int index) -> server_rules
    {
        return index <= 0 ? server_rules::retail : index >= 2 ? server_rules::phoenix : server_rules::horizon;
    }

    constexpr auto ServerRulesIndex(const server_rules rules) -> int
    {
        return static_cast<int>(rules);
    }

    inline auto ServerRulesName(const server_rules rules) -> const char*
    {
        return server_rule_names[ServerRulesIndex(rules)];
    }
}
