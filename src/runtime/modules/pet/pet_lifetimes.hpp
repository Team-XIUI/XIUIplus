#pragma once

#include "combat/server_rules.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

namespace xiui
{
    // Duration and Charm+ data from XIUI Lua 1.8.5 / PetMe; see bundled XIUI notices.
    struct jug_lifetime { const char* name; uint32_t retail, horizon; };
    inline constexpr jug_lifetime jug_lifetimes[]{
        {"AlluringHoney", 7200, 7200},
        {"AmbusherAllie", 3600, 3600},
        {"AmiableRoche", 7200, 7200},
        {"AmigoSabotender", 1800, 3600},
        {"AnklebiterJedd", 7200, 7200},
        {"AntlionFamiliar", 3600, 1800},
        {"AttentiveIbuki", 7200, 7200},
        {"AudaciousAnna", 7200, 7200},
        {"BeetleFamiliar", 3600, 1800},
        {"BlackbeardRandy", 7200, 7200},
        {"BloodclawShasra", 7200, 7200},
        {"BouncingBertha", 7200, 7200},
        {"BrainyWaluis", 7200, 7200},
        {"BraveHeroGlenn", 7200, 7200},
        {"BugeyedBroncha", 7200, 7200},
        {"CaringKiyomaro", 7200, 7200},
        {"ChopsueyChucky", 3600, 3600},
        {"ColdbloodComo", 3600, 3600},
        {"CourierCarrie", 1800, 3600},
        {"CrabFamiliar", 1800, 1800},
        {"CraftyClyvonne", 7200, 7200},
        {"CrudeRaphie", 5400, 5400},
        {"CursedAnnabelle", 7200, 7200},
        {"DapperMac", 7200, 7200},
        {"DipperYuly", 7200, 7200},
        {"DiscreetLouise", 7200, 7200},
        {"DroopyDortwin", 7200, 7200},
        {"EftFamiliar", 3600, 1800},
        {"FaithfulFalcorr", 7200, 7200},
        {"FatsoFargann", 7200, 7200},
        {"FleetReinhard", 7200, 7200},
        {"FlowerpotBen", 3600, 3600},
        {"FlowerpotBill", 3600, 1800},
        {"FlowerpotMerle", 10800, 10800},
        {"FlytrapFamiliar", 3600, 1800},
        {"FunguarFamiliar", 3600, 1800},
        {"GenerousArthur", 7200, 7200},
        {"GooeyGerard", 5400, 5400},
        {"GorefangHobs", 7200, 7200},
        {"HareFamiliar", 5400, 1800},
        {"HeadbreakerKen", 7200, 7200},
        {"HeraldHenry", 7200, 7200},
        {"Homunculus", 3600, 3600},
        {"HurlerPercival", 7200, 7200},
        {"KeenearedSteffi", 5400, 3600},
        {"LifedrinkerLars", 3600, 3600},
        {"LizardFamiliar", 3600, 1800},
        {"LuckyLulush", 7200, 7200},
        {"LullabyMelodia", 3600, 3600},
        {"MailbusterCetas", 7200, 7200},
        {"MayflyFamiliar", 3600, 1800},
        {"MiteFamiliar", 3600, 1800},
        {"NurseryNazuna", 7200, 7200},
        {"PanzerGalahad", 3600, 3600},
        {"PrestoJulio", 7200, 7200},
        {"RedolentCandi", 7200, 7200},
        {"RhymingShizuna", 7200, 7200},
        {"SaberSiravarde", 3600, 3600},
        {"ScissorlegXerin", 7200, 7200},
        {"SharpwitHermes", 7200, 7200},
        {"SheepFamiliar", 3600, 1800},
        {"ShellbusterOrob", 3600, 3600},
        {"SlipperySilas", 1800, 1800},
        {"SunburstMalfik", 7200, 7200},
        {"SuspiciousAlice", 7200, 7200},
        {"SwiftSieghard", 7200, 7200},
        {"SwoopingZhivago", 7200, 7200},
        {"ThreestarLynn", 7200, 7200},
        {"TigerFamiliar", 3600, 1800},
        {"TurbidToloi", 7200, 7200},
        {"VoraciousAudrey", 3600, 3600},
        {"WarlikePatrick", 7200, 7200},
    };
    struct charm_equipment { uint16_t id; uint8_t ranks; };
    inline constexpr charm_equipment charm_equipment_bonuses[]{
        {12517, 4},
        {12646, 5},
        {13667, 5},
        {13969, 3},
        {14097, 2},
        {14222, 6},
        {14319, 5},
        {14320, 6},
        {14418, 5},
        {14419, 6},
        {14481, 6},
        {14508, 7},
        {14566, 5},
        {14567, 6},
        {14658, 4},
        {14850, 5},
        {14851, 6},
        {14898, 3},
        {14917, 4},
        {14981, 5},
        {14982, 6},
        {15080, 5},
        {15095, 6},
        {15110, 4},
        {15125, 2},
        {15140, 3},
        {15157, 5},
        {15158, 6},
        {15233, 4},
        {15253, 5},
        {15307, 5},
        {15308, 6},
        {15360, 2},
        {15569, 6},
        {15588, 2},
        {15645, 5},
        {15646, 6},
        {15673, 3},
        {15731, 5},
        {15732, 6},
        {16104, 5},
        {16105, 6},
        {17936, 1},
        {17950, 2},
    };
    inline auto JugLifetimeSeconds(std::string_view name, server_rules rules) -> uint32_t
    {
        std::string key; for (unsigned char c : name) if (c != ' ') key += static_cast<char>(std::tolower(c));
        for (const auto& row : jug_lifetimes)
        {
            std::string candidate{row.name}; for (auto& c : candidate) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (candidate == key) return rules == server_rules::horizon ? row.horizon : row.retail;
        }
        return 0;
    }
    inline auto CharmLifetimeSeconds(int playerLevel, int mobLevel, int charisma, float bonus, uint32_t gear) -> uint32_t
    {
        if (playerLevel <= 0 || mobLevel <= 0 || mobLevel > 255 || charisma <= 0) return 0;
        constexpr float modifiers[]{.04f,.08f,.12f,.16f,.33f,.66f,1,1.4f,1.8f,2.2f,2.6f,3,3.4f,4,5,6};
        const auto difference = std::clamp(playerLevel - mobLevel, -6, 9);
        return static_cast<uint32_t>(std::floor(std::floor(1.25f * (charisma + bonus) + 150) * modifiers[difference + 6] * (1 + .05f * gear)));
    }
}
