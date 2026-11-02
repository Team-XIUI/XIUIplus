#include "config_input_fixture.hpp"

using namespace xiui::config_checks;

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--xiv1-theme-only") == 0)
    {
        if (!xiui::config_input_test::Xiv1Theme()) return 1;
        std::puts("XIV 1.0 theme persistence, native preference isolation, controller access and text fades passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--edit-mode-only") == 0)
    {
        if (!xiui::config_input_test::EditMode(argv[2])) return 1;
        std::puts("Explicit edit mode, selected previews, hidden input controls and live hover editing passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--setup-wizard-only") == 0)
    {
        if (!xiui::config_input_test::SetupWizard(argv[2])) return 1;
        std::puts("First-load setup, shared input, native fallback, local persistence and native chat defaults passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--window-input-only") == 0)
    {
        if (!xiui::config_input_test::WindowInput()) return 1;
        std::puts("Hex editing, focus/draft suspension, native window cycling and fresh history checks passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--hud-presets-only") == 0)
    {
        if (!xiui::config_input_test::HudPresets(argv[2])) return 1;
        std::puts("Captured full HUD presets, Party-only Classic differences and independent reset defaults passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-memory-only") == 0)
    {
        if (!xiui::config_input_test::ConfigMemory()) return 1;
        std::puts("Configuration mode, page, scroll and position memory passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--module-menu-only") == 0)
    {
        if (!xiui::config_input_test::ModuleMenu(argv[2])) return 1;
        std::puts("Module context menu, owned clicks, scoped resets and confirmation checks passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--hud-scaling-only") == 0)
    {
        if (!xiui::config_input_test::HudScaling(argv[2])) return 1;
        std::puts("HUD scaling geometry, gestures, input ownership, Basic/Advanced, presets and persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--fishing-only") == 0)
    {
        if (!xiui::config_input_test::Fishing()) return 1;
        std::puts("Fishing visibility, Extras registration, controller controls, resolution defaults and persistence passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--viewport-layout-only") == 0)
    {
        if (!xiui::config_input_test::ViewportLayout(argv[2])) return 1;
        std::puts("First-load viewport fitting, missing positions, saved layouts and position-only recovery passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--lua-profile-import-only") == 0)
    {
        if (!xiui::config_input_test::LuaProfileImport()) return 1;
        std::puts("XIUI Lua profile conversion, discovery, preview, controller routing and isolated import checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--startup-wiring-only") == 0)
    {
        if (!xiui::config_input_test::StartupWiring()) return 1;
        std::puts("Runtime startup dependencies and shared UTF-8 text fitting passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--cursor-ownership-only") == 0)
    {
        if (!cursor_ownership_cases()) return 1;
        std::puts("Cursor capture, render-thread release, visibility balance and native cursor ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--notification-feedback-only") == 0)
    {
        if (!xiui::RunNotificationFeedbackChecks() || !xiui::config_input_test::NotificationGroupsInput()) return 1;
        std::puts("Notification groups, bounded invitations, controller details, nameplate preferences and HP feedback checks passed.");
        return 0;
    }

    if (argc == 2 && std::strcmp(argv[1], "--all-ui-visibility-only") == 0)
    {
        if (!xiui::config_input_test::AllUiVisibility()) return 1;
        std::puts("Combined UI hiding, pointer release, controller activation and restored preferences passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--settings-search-only") == 0)
    {
        if (!xiui::config_input_test::SettingsSearch()) return 1;
        std::puts("Settings search catalog, matching, navigation, input ownership and compact input control passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-sidebar-only") == 0)
    {
        if (!xiui::config_input_test::ConfigSidebar()) return 1;
        std::puts("Collapsible config sidebar, hover names, responsive geometry, profile controls, persistence and controller focus passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-resize-only") == 0)
    {
        if (!xiui::config_input_test::ConfigResize() || !RunConfigResponsiveChecks()) return 1;
        std::puts("Config resizing, bounded geometry, responsive content, focus and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--target-ring-state-only") == 0) return RunTargetRingStateChecks() ? 0 : 1;
    if (argc >= 2 && std::strcmp(argv[1], "--target-rings-only") == 0) return RunTargetRingChecks(argc > 2 ? argv[2] : nullptr) ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--heal-prediction-only") == 0) return RunHealPredictionChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--profile-delete-only") == 0)
    {
        if (!xiui::config_input_test::ProfileDeletion()) return 1;
        std::puts("Profile delete confirmation, red button, cancellation, failures, Default fallback and persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--exp-position-only") == 0)
    {
        if (!xiui::config_input_test::ExperiencePositionPersistence()) return 1;
        std::puts("EXP saved positions, measured bounds, profile reloads and viewport clamping passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-text-only") == 0)
    {
        if (!xiui::config_input_test::ConfigText()) return 1;
        std::puts("Config fonts, pixel letter spacing, shared text effects, HUD independence, key wrapping, persistence and reset passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--input-mode-only") == 0)
    {
        if (!xiui::config_input_test::HeaderInputMode()) return 1;
        std::puts("Header input mode pointer selection, controller availability ownership and status icon routing passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--scrollbar-only") == 0)
    {
        if (!xiui::config_input_test::ConfigScrollbar()) return 1;
        std::puts("Config scrollbar visibility, gutter, thumb drag, track paging and bounded row alignment passed.");
        return 0;
    }
    if ((argc >= 2 && argc <= 4) && std::strcmp(argv[1], "--party-presets-only") == 0)
    {
        if (!xiui::config_input_test::PartyPresets(argc >= 3 ? argv[2] : nullptr, argc == 4 ? argv[3] : nullptr)) return 1;
        std::puts("Party General preset cards, pointer/controller actions and persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--all-bars-only") == 0)
    {
        if (!xiui::config_input_test::AllBarsScope()) return 1;
        std::puts("All bars pointer/controller selection, per-setting edits and individual geometry preservation passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--element-tabs-only") == 0)
    {
        if (!xiui::config_input_test::ElementTabs()) return 1;
        std::puts("Direct element tabs, combined bar scope, full-width headers, wrapping, input ownership and saved choices passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-entry-only") == 0)
    {
        if (!xiui::config_input_test::PartyEntries()) return 1;
        std::puts("Party visible entry bounds, following job icons, SYNC visibility, cast sizing and independent persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--choice-arrows-only") == 0)
    {
        if (!xiui::config_input_test::ChoiceArrows()) return 1;
        std::puts("Previous/next choice arrows, wraparound, callbacks, hover feedback, capture and dropdown behavior passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--text-rasterization-only") == 0)
    {
        if (!text_rasterization_cases()) return 1;
        std::puts("Native text sizes, antialiasing for all four fonts, effects, Unicode, stable digits and pixel controls passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--attribution-only") == 0)
    {
        if (!xiui::config_input_test::Attribution() || !xiui::config_input_test::CloseInput()) return 1;
        std::puts("Attribution header routing, credits, capture and existing close gesture checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--global-text-only") == 0)
    {
        if (!xiui::config_input_test::GlobalText()) return 1;
        std::puts("Global text controls, all-module updates, independent overrides, profile persistence and embedded Grammara font passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--target-recipient-theme-only") == 0)
    {
        if (!xiui::config_input_test::TargetOfTargetThemeDefaults()) return 1;
        std::puts("Target of Target theme defaults, shared layout choices and persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--target-recipient-only") == 0)
    {
        if (!xiui::config_input_test::TargetOfTargetSettings()) return 1;
        std::puts("Classic inline target-of-target geometry, arrow alignment, topic input, styled persistence and default migration checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-module-only") == 0)
    {
        if (!xiui::config_input_test::PartyModuleVisibility()) return 1;
        std::puts("Party master, per-list live/edit visibility, settings access and profile persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-target-settings-only") == 0)
    {
        if (!xiui::config_input_test::PartyModuleVisibility() || !xiui::config_input_test::TargetOfTargetSettings()) return 1;
        std::puts("Party master and preview visibility, accessible alliance settings, target-of-target controls and profile persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--enabled-modules-only") == 0)
    {
        if (!xiui::config_input_test::EnabledModules()) return 1;
        std::puts("Enabled Modules controls, filtered sidebar routing, grouped pages, fallback and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--native-windows-tab-only") == 0)
    {
        if (!xiui::config_input_test::NativeWindowsTab()) return 1;
        std::puts("General Game HUD sidebar, shared controls, command routing and saved navigation checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--chat-preview-only") == 0)
    {
        if (!xiui::config_input_test::ChatSoundPreview() || !xiui::config_input_test::ChatCustomization()) return 1;
        std::puts("Dropdown play icons, Space previews, duplicate/release handling and normal sound selection checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--profile-sharing-only") == 0)
    {
        if (!xiui::config_input_test::ProfileSharing() || !xiui::config_input_test::Profiles()) return 1;
        std::puts("Background file sharing, cancellation, legacy keys and existing profile persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--chat-customization-only") == 0)
    {
        if (!xiui::config_input_test::ChatCustomization() || !xiui::config_input_test::ChatAlertInput()) return 1;
        std::puts("Chat sound dropdown selection, popup input ownership, optional color picker and keyword field checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--chat-alerts-only") == 0)
    {
        if (!xiui::config_input_test::ChatAlertInput()) return 1;
        std::puts("Chat keyword field editing, commits, cancellation, length limits and keyboard ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--collapse-only") == 0)
    {
        if (!xiui::config_input_test::ConfigCollapse() || !xiui::config_input_test::CloseInput()) return 1;
        std::puts("Config collapse/restore, title-bar dragging, hidden input removal, HUD preview access and close capture checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--close-input-only") == 0)
    {
        if (!xiui::config_input_test::CloseInput()) return 1;
        std::puts("Config close press/release capture, cancellation and mouse block restoration checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--character-profiles-only") == 0)
    {
        if (!xiui::config_input_test::CharacterProfiles() || !xiui::config_input_test::Profiles()) return 1;
        std::puts("Per-character profile restoration, pending saves, identity loss, failure/fallback, shared selection and portability checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--profiles-only") == 0)
    {
        if (!xiui::config_input_test::Profiles()) return 1;
        std::puts("Profile dropdown, modal creation, validation, autosave, switching, session isolation and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--preview-focus-only") == 0)
    {
        if (!xiui::config_input_test::PreviewFocus()) return 1;
        std::puts("Preview module focus, shared-page routing, drag preservation and hidden-preview ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--exp-display-only") == 0)
    {
        if (!xiui::config_input_test::ExperienceDisplay()) return 1;
        std::puts("EXP display views, direct readout selection, ordering, filters, persistence and text separation checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--sounds-only") == 0)
    {
        if (!xiui::config_input_test::Sounds()) return 1;
        std::puts("Native UI sound routing, hover transitions, click/drag ownership, silent restore and HUD controls checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--slider-input-only") == 0)
    {
        if (!xiui::config_input_test::SliderValues()) return 1;
        std::puts("Typed slider values, validation, callbacks, persistence and input ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--pet-hud-only") == 0)
    {
        if (!xiui::RunPetHudChecks() || !xiui::config_input_test::PetTabs()) return 1;
        std::puts("Pet HUD, portrait customization, persistence and tab selection checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--pet-tabs-only") == 0)
    {
        if (!xiui::config_input_test::PetTabs()) return 1;
        std::puts("Pet settings tabs select one view at a time through pointer and controller actions.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--navigation-only") == 0)
    {
        if (!xiui::config_input_test::EmptyCategories() || !xiui::config_input_test::Navigation() || !xiui::config_input_test::SecondaryMenus() || !xiui::config_input_test::ChatExtrasMenus()) return 1;
        std::puts("Populated config categories, compacted input, selection fallback and module navigation checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--color-picker-only") == 0)
    {
        if (!xiui::config_input_test::ColorPicker()) return 1;
        std::puts("Color wheel conversion, interaction, opacity, restoration and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--resource-target-features-only") == 0)
    {
        const std::pair<const char*, bool(*)()> checks[]{
            {"Player resources", &RunPlayerResourceChecks}, {"MP cost preview", &RunMpCostPreviewChecks},
            {"Target child views", &xiui::RunTargetChildChecks}, {"Target child movement", &xiui::config_input_test::TargetChildren},
            {"Target configuration", &xiui::config_input_test::TargetOfTargetSettings}};
        for (const auto& [name, run] : checks)
        {
            std::printf("Checking %s...\n", name); std::fflush(stdout);
            if (!run()) { std::fprintf(stderr, "%s check failed.\n", name); return 1; }
        }
        std::puts("P16/P17/P19/P20 resource, MP preview, target child data/layout and shared input checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--minimap-only") == 0) return RunMinimapChecks() && xiui::config_input_test::MinimapEdgeButtons() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--skillchain-only") == 0) return RunSkillchainChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--phoenix-only") == 0) return RunPhoenixIntegrationChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--text-visibility-only") == 0) return RunTextVisibilityChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--treasure-input-only") == 0)
    {
        if (!xiui::config_input_test::TreasureInput()) return 1;
        std::puts("Treasure input availability and owned key release checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--player-resources-only") == 0) return RunPlayerResourceChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--mp-preview-only") == 0) return RunMpCostPreviewChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--new-features-only") == 0)
    {
        if (!xiui::RunTargetDisplayChecks() || !xiui::RunReadyCheckSoundChecks() || !RunBlueMagicChecks() ||
            !RunPhantomRollChecks() || !RunIconThemeChecks() || !xiui::RunMobInfoChecks() || !xiui::RunPetHudChecks()) return 1;
        std::puts("Target, Ready Check sound, Blue Magic, Phantom Roll, icon themes, Mob Info and Pet HUD checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--icon-themes-only") == 0) return RunIconThemeChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--mob-info-only") == 0)
    {
        if (!xiui::RunMobInfoChecks() || !xiui::RunTargetDisplayChecks()) return 1;
        std::puts("Integrated Target mob information, lookup, legacy settings and layout checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-name-cast-only") == 0)
    {
        return xiui::config_input_test::PartyNameCast() ? 0 : 1;
    }
    if ((argc == 2 || argc == 3) && std::strcmp(argv[1], "--settings-presentation-only") == 0)
    {
        return xiui::config_input_test::SettingsPresentation(argc == 3 ? argv[2] : nullptr) ? 0 : 1;
    }
    if (argc == 3 && std::strcmp(argv[1], "--defaults-only") == 0)
    {
        if (!xiui::config_input_test::DefaultProfile(argv[2])) return 1;
        std::puts("Embedded defaults, fresh installs, existing settings and reset checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--status-snapshots-only") == 0)
    {
        if (!xiui::config_input_test::StatusSnapshots()) return 1;
        std::puts("Local and party zoning snapshots, genuine KO, initialized empty lists and extended status IDs passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-only") == 0)
    {
        if (!xiui::config_input_test::Party() || !xiui::config_input_test::Navigation()) return 1;
        std::puts("Party data, statuses, layout, independent settings and navigation checks passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--type7-edge-only") == 0)
    {
        if (!xiui::config_input_test::WindowFrame(argv[2])) return 1;
        std::puts("Type 7 right-edge correction, original artwork and unrelated/custom theme preservation passed.");
        return 0;
    }
    if (argc >= 4 && std::strcmp(argv[1], "--window-borders-only") == 0)
    {
        if (!xiui::config_input_test::NativeWindowBorders(argv[2], argv[3], argc > 4 ? argv[4] : nullptr)) return 1;
        std::puts("Type 6/7 window vertex colors, native border alpha and original edge blend/artwork checks passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--theme-ui") == 0)
    {
        if (!xiui::config_input_test::DarkBacking() || !xiui::config_input_test::ThemedButtons() || !xiui::config_input_test::WindowFrame(argv[2])) return 1;
        std::puts("Backing opacity, themed buttons and native Type 7 border checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--backing-only") == 0)
    {
        if (!xiui::config_input_test::DarkBacking()) return 1;
        std::puts("Dark backing opacity input and persistence checks passed.");
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--bars-only") == 0)
    {
        if (!meter_cases(argc > 2 ? argv[2] : nullptr) || !meter_text_cases()) return 1;
        std::puts("Bar styles, native gauge artwork, settings and text layout checks passed.");
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--config-ui") == 0)
    {
        if (!xiui::config_input_test::Presentation(argc > 2 ? argv[2] : nullptr) || !xiui::config_input_test::Navigation() || !xiui::config_input_test::Run()) return 1;
        std::puts("Config layout, text effects, sidebar icons, native buttons and click checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--layout-only") == 0)
    {
        if (!enemy_cases() || !meter_text_cases()) return 1;
        std::printf("Enemy header and EXP meter text layout checks passed.\n");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--drag-only") == 0)
    {
        if (!xiui::config_input_test::DragPersistence()) return 1;
        std::printf("Module drop and config layout persistence checks passed.\n");
        return 0;
    }
    if (argc != 1)
    {
        std::fprintf(stderr, "Unknown check or arguments: %s\n", argv[1]);
        return 2;
    }
    if (!xiui::config_input_test::DragPersistence()) return 1;
    if (!xiui::config_input_test::Run() || !enemy_cases() || !meter_text_cases()) return 1;
    const auto path = std::filesystem::temp_directory_path() / ("XIUIplus-toggle-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
    struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
    const xiui::settings_store store{path.string()};
    if (!text_profile_cases(store)) return 1;
    const auto configure = [](auto&, int) {};
    size_t checked{};
    if (!persistence<xiui::player_module>(store, 12, [](auto& module, int context) {
            module.Settings().style_edit_mode = 1;
            module.Settings().selected_bar = context % 3;
            module.Settings().selected_text = context / 3;
        }, checked) ||
        !persistence<xiui::target_module>(store, 4, [](auto& module, int context) { *module.Option(4).choice = context; }, checked) ||
        !persistence<xiui::experience_module>(store, 4, [](auto& module, int context) {
            *module.Option(4).choice = context % 2;
            module.Settings().info_position = context / 2;
        }, checked) ||
        !persistence<xiui::enemy_list_module>(store, 4, [](auto& module, int context) { *module.Option(6).choice = context; }, checked) ||
        !persistence<xiui::notification_module>(store, 1, configure, checked) ||
        !persistence<xiui::game_hud_module>(store, 1, configure, checked)) return 1;
    std::printf("Config click regressions and %zu toggle persistence checks passed.\n", checked);
    return 0;
}
