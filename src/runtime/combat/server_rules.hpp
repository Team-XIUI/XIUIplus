#pragma once

namespace xiui
{
    inline constexpr const char* server_rule_names[]{"Retail", "Horizon", "Phoenix (75 cap)"};
    inline auto ServerRulesName(const int rules) -> const char*
    { return server_rule_names[rules >= 0 && rules < 3 ? rules : 0]; }
}
