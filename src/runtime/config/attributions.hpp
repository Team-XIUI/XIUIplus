#pragma once

#include <array>
#include <cstdint>

namespace xiui::ui
{
    struct attribution { const char* project; const char* credit; const char* url; };
    inline constexpr std::array attributions{
        attribution{"Cadence", "Kipling - attack and resting timer features.", "https://github.com/KiplingFFXI/cadence"},
        attribution{"Momentum", "Kipling - EXP tracking features.", "https://github.com/KiplingFFXI/momentum"},
        attribution{"Chains", "Sippius, Ivaar and NerfOnline - skillchain display and pet availability data (GPL-3.0-or-later).", "https://github.com/NerfOnline/Chains"},
        attribution{"ChatMon", "Windower - chat notification sounds.", "https://github.com/Windower/Lua/tree/live/addons/chatmon"},
        attribution{"Cursor Pack", "Kenney - cursor artwork (CC0).", "https://kenney.nl/assets/cursor-pack"},
        attribution{"Find", "MalRD, zombie343, sippius - storage-slip catalog (MIT).", "https://github.com/Sippius/Ashita-v4-addons/tree/main/find"},
        attribution{"Grammara", "Grammara font (freeware, non-commercial).", "https://www.fontspace.com/grammara-font-f4454"},
        attribution{"HeadsUp", "Tanyrus - Phoenix mob data snapshot (GPL-3.0-or-later).", "https://github.com/Tanyrus/headsup"},
        attribution{"libwebp", "Google - WebP image decoding (BSD-3-Clause).", "https://chromium.googlesource.com/webm/libwebp"},
        attribution{"MinHook", "Tsuda Kageyu - native function hooking (BSD-2-Clause).", "https://github.com/TsudaKageyu/minhook"},
        attribution{"miniz", "Rich Geldreich and contributors - resource decompression (MIT).", "https://github.com/richgel999/miniz"},
        attribution{"MobDB", "ThornyFFXI - mob damage and detection icons (MIT).", "https://github.com/ThornyFFXI/MobDB"},
        attribution{"Phoenix", "Phoenix contributors - game rules and data.", "https://github.com/phoenixffxi/Phoenix"},
        attribution{"Remapster", "AkadenTK and contributors - enhanced minimap artwork (CC BY-SA 4.0).", "https://github.com/AkadenTK/remapster_maps"},
        attribution{"Vincerno icons", "Vincerno - custom macro icons (GPL-3.0).", "https://github.com/Vincerno/xiui-icons"},
        attribution{"XivParty", "Tylas - job icons (BSD-3-Clause).", "https://github.com/Tylas11/XivParty"}};

    inline constexpr std::array ashita_credits{
        attribution{"Ashita", "The framework and plugin SDK that XIUI+ runs on.", "https://www.ashitaxi.com/"},
        attribution{"atom0s", "Thank you for your work on Ashita and the tools that make XIUI+ possible.", "https://github.com/atom0s"}};

    enum class contributor_group : uint8_t { founders, core, contributors };
    struct xiui_contributor { const char* name; const char* url; contributor_group group; };

    // Curated from XIUI revision 4493ec0, with separate founder and core credits.
    inline constexpr std::array xiui_contributors{
        xiui_contributor{"Tirem", "https://github.com/tirem", contributor_group::founders},
        xiui_contributor{"Shuu", "https://github.com/Shuu-37", contributor_group::founders},
        xiui_contributor{"JQuick", "https://github.com/jquick", contributor_group::core},
        xiui_contributor{"Nerf", "https://github.com/NerfOnline", contributor_group::core},
        xiui_contributor{"eduardocopat", "https://github.com/eduardocopat", contributor_group::contributors},
        xiui_contributor{"extrasupervery", "https://github.com/extrasupervery", contributor_group::contributors},
        xiui_contributor{"jshackles", "https://github.com/jshackles", contributor_group::contributors},
        xiui_contributor{"Lydya", nullptr, contributor_group::contributors},
        xiui_contributor{"m4thmatic", "https://github.com/m4thmatic", contributor_group::contributors},
        xiui_contributor{"Rave7283", "https://github.com/Rave7283", contributor_group::contributors},
        xiui_contributor{"Xcior37", "https://github.com/Xcior37", contributor_group::contributors},
        xiui_contributor{"yzyii", "https://github.com/yzyii", contributor_group::contributors}};

    inline constexpr const char* attribution_views[]{"Projects", "Original XIUI contributors", "Ashita"};
    inline constexpr const char* contributor_groups[]{"Founders", "Core Contributors", "Contributors"};
}
