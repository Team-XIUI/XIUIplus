#pragma once

#include "ui/text_fit.hpp"
#include "hud_runtime.hpp"
#include "config/contributor_list.hpp"
#include "modules/bar_config.hpp"
#include "BinaryData.h"
#include "resources/menu_texture_scale.hpp"
#include "resources/item_icon.hpp"
#include "settings/profile_share.hpp"
#include "settings/profile_share_delta.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

auto RunPlayerResourceChecks(void) -> bool;
auto RunMinimapChecks(void) -> bool;
auto RunSkillchainChecks(void) -> bool;
auto RunPhoenixIntegrationChecks(void) -> bool;
auto RunTextVisibilityChecks(void) -> bool;
auto RunMpCostPreviewChecks(void) -> bool;
auto RunHealPredictionChecks(void) -> bool;
auto RunTargetRingChecks(const char* client_root) -> bool;
auto RunTargetRingStateChecks(void) -> bool;
auto RunBlueMagicChecks(void) -> bool;
auto RunPhantomRollChecks(void) -> bool;
auto RunIconThemeChecks(void) -> bool;
auto RunConfigResponsiveChecks(void) -> bool;
namespace xiui { auto RunTargetChildChecks(void) -> bool; }
namespace xiui
{
    auto RunTargetDisplayChecks(void) -> bool;
    auto RunReadyCheckSoundChecks(void) -> bool;
    auto RunMobInfoChecks(void) -> bool;
    auto RunPetHudChecks(void) -> bool;
    auto RunNotificationFeedbackChecks(void) -> bool;
}

namespace xiui::config_checks
{
    auto check(bool condition, const char* message) -> bool;
    auto cursor_ownership_cases(void) -> bool;
    auto text_rasterization_cases(void) -> bool;
    auto enemy_cases(void) -> bool;
    auto meter_cases(const char* dat) -> bool;
    auto meter_text_cases(void) -> bool;
    auto text_profile_cases(const xiui::settings_store& store) -> bool;
    template<typename Module, typename Configure>
    auto persistence(const xiui::settings_store& store, int contexts, Configure configure, size_t& checked) -> bool
    {
        const auto theme = xiui::ui::theme::CreateDefault();
        for (int context = 0; context < contexts; ++context)
        {
            for (int pattern = 0; pattern < 3; ++pattern)
            {
                Module source{}, loaded{};
                source.ResetDefaults(theme);
                loaded.ResetDefaults(theme);
                configure(source, context);
                for (size_t i = 0; i < source.OptionCount(); ++i)
                {
                    auto option = source.Option(i);
                    if (option.kind != xiui::module_option_kind::toggle || option.toggle == nullptr) continue;
                    *option.toggle = pattern == 1 || (pattern == 2 && i % 2 == 0);
                    source.OnOptionChanged(i);
                }
                source.Save(store);
                store.Flush();
                loaded.Load(store);
                for (size_t i = 0; i < source.OptionCount(); ++i)
                {
                    auto before = source.Option(i), after = loaded.Option(i);
                    if (before.kind == xiui::module_option_kind::choice && before.choice != nullptr)
                    {
                        if (!check(after.choice != nullptr && *before.choice == *after.choice, "selected profile and info-position choices persist")) return false;
                    }
                    if (before.kind != xiui::module_option_kind::toggle || before.toggle == nullptr) continue;
                    if (after.toggle == nullptr || *before.toggle != *after.toggle)
                    {
                        std::fprintf(stderr, "%s: %s did not persist (context %d, pattern %d).\n", source.Id(), before.label, context, pattern);
                        return false;
                    }
                    ++checked;
                }
            }
        }
        return true;
    }
}

namespace xiui
{
    struct config_input_test
    {
        static auto NativeUi(void) -> bool;
        inline static size_t preview_sounds{};
        inline static size_t hover_sounds{}, select_sounds{}, menu_sounds{};
        static inline uint32_t* window_record{};
        static inline unsigned window_writes{};
        static auto StartupWiring(void) -> bool;
        static auto StartupSettings(void) -> bool;
        static auto SetupWizard(const char* defaultsPath) -> bool;
        static auto HudScaling(const char* defaultsPath) -> bool;
        static auto HudPresets(const char* defaultsPath) -> bool;
        static auto ModuleMenu(const char* defaultsPath) -> bool;
        static auto EditMode(const char* defaultsPath) -> bool;
        static auto ConfigMemory(void) -> bool;
        static auto WindowInput(void) -> bool;
        static auto TreasureInput(void) -> bool;
        static auto Fishing(void) -> bool;
        static auto ControlCount(hud_runtime& runtime) -> size_t;
        static auto PartyNameCast(void) -> bool;
        static auto SettingsPresentation(const char* reportPath) -> bool;
        static auto ConfigText(void) -> bool;
        static auto ConfigScrollbar(void) -> bool;
        static auto HeaderInputMode(void) -> bool;
        static auto AllBarsScope(void) -> bool;
        static auto ElementTabs(void) -> bool;
        static auto PartyEntries(void) -> bool;
        static auto ChoiceArrows(void) -> bool;
        static auto __cdecl PreviewSound(void) -> void;
        static auto ChatSoundPreview(void) -> bool;
        static auto ProfileSharing(void) -> bool;
        static auto ChatCustomization(void) -> bool;
        static auto ChatAlertInput(void) -> bool;
        static auto ConfigCollapse(void) -> bool;
        static auto CloseInput(void) -> bool;
        static auto ProfileDeletion(void) -> bool;
        static auto Profiles(void) -> bool;
        static auto CharacterProfiles(void) -> bool;
        static auto PreviewFocus(void) -> bool;
        static auto ExperienceDisplay(void) -> bool;
        static auto __cdecl HoverSound(void) -> void;
        static auto __cdecl SelectSound(void) -> void;
        static auto __cdecl MenuSound(void) -> void;
        static auto Sounds(void) -> bool;
        static auto SliderValues(void) -> bool;
        static auto ColorPicker(void) -> bool;
        static auto StatusSnapshots(void) -> bool;
        static auto Party(void) -> bool;
        static auto WindowFrame(const char* dat) -> bool;
        static auto NativeWindowBorders(const char* type6Dat, const char* type7Dat, const char* liveLeft) -> bool;
        static auto DefaultProfile(const char* source) -> bool;
        static auto ViewportLayout(const char* source) -> bool;
        static auto PartyPresets(const char* classicPath = nullptr, const char* xivPath = nullptr) -> bool;
        static auto ThemedButtons(void) -> bool;
        static auto GlobalText(void) -> bool;
        static auto Attribution(void) -> bool;
        static auto TargetOfTargetSettings(void) -> bool;
        static auto TargetOfTargetThemeDefaults(void) -> bool;
        static auto TargetChildren(void) -> bool;
        static auto PartyModuleVisibility(void) -> bool;
        static auto EnabledModules(void) -> bool;
        static auto NativeWindowsTab(void) -> bool;
        static auto DarkBacking(void) -> bool;
        static auto __cdecl SetWindowType(const uint32_t id, const int value) -> int;
        static auto WindowTheme(void) -> bool;
        static auto Xiv1Theme(void) -> bool;
        static auto Presentation(const char* dat) -> bool;
        static auto EmptyCategories(void) -> bool;
        static auto SecondaryMenus(void) -> bool;
        static auto ChatExtrasMenus(void) -> bool;
        static auto SettingsSearch(void) -> bool;
        static auto ConfigSidebar(void) -> bool;
        static auto ConfigResize(void) -> bool;
        static auto ConfigWindowScaling(void) -> bool;
        static auto NotificationGroupsInput(void) -> bool;
        static auto MinimapEdgeButtons(void) -> bool;
        static auto AllUiVisibility(void) -> bool;
        static auto PetTabs(void) -> bool;
        static auto Navigation(void) -> bool;
        static auto ExperiencePositionPersistence(void) -> bool;
        static auto DragPersistence(void) -> bool;
        static auto Run(void) -> bool;
    };
}
