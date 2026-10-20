#pragma once

#include "Ashita.h"
#include "ui/native_ui_assets.hpp"
#include "ui/game_icon_cache.hpp"
#include "design_system.hpp"
#include "settings/layout_viewport.hpp"
#include "config/config_controls.hpp"
#include "config/config_session.hpp"
#include "config/color_picker.hpp"
#include "config/experience_display_editor.hpp"
#include "config/settings_search.hpp"
#include "input/cursor_renderer.hpp"
#include "modules/experience/experience_module.hpp"
#include "modules/module_registry.hpp"
#include "modules/player/player_module.hpp"
#include "modules/target/target_module.hpp"
#include "modules/target/target_child_module.hpp"
#include "modules/pet/pet_module.hpp"
#include "modules/pet/pet_source.hpp"
#include "modules/bluemagic/blue_magic_module.hpp"
#include "modules/phantomroll/phantom_roll_module.hpp"
#include "combat/combat_settings.hpp"
#include "modules/magicburst/magic_burst_module.hpp"
#include "modules/enemylist/enemy_list_module.hpp"
#include "modules/party/party_module.hpp"
#include "modules/inventory/inventory_module.hpp"
#include "modules/satchel/satchel_module.hpp"
#include "modules/satchel/satchel_source.hpp"
#include "modules/satchel/satchel_cache.hpp"
#include "modules/satchel/satchel_actions.hpp"
#include "modules/actionbars/actionbars_module.hpp"
#include "modules/actionbars/native_macros.hpp"
#include "modules/equipment/equipment_module.hpp"
#include "modules/gil/gil_module.hpp"
#include "modules/readycheck/ready_check_module.hpp"
#include "modules/status/status_effects_module.hpp"
#include "modules/castbar/cast_bar_module.hpp"
#include "modules/fishing/fishing_module.hpp"
#include "modules/castcost/cast_cost_module.hpp"
#include "modules/recast/recast_module.hpp"
#include "runtime_api.hpp"
#include "modules/notifications/notification_module.hpp"
#include "modules/game_hud/game_hud_module.hpp"
#include "modules/general/general_module.hpp"
#include "modules/chat/chat_module.hpp"
#include "modules/chat/chat_bubbles_module.hpp"
#include "settings/lua_setup_import.hpp"
#include "modules/compass/compass_module.hpp"
#include "modules/minimap/minimap_module.hpp"
#include "modules/minimap/minimap_source.hpp"
#include "modules/world/world_info_module.hpp"
#include "modules/commands/command_menu_module.hpp"
#include "world/world_source.hpp"
#include "ui/native_window_config.hpp"
#include "ui/native_ui_sounds.hpp"
#include "native_chat_palette.hpp"
#include "notifications/notification_source.hpp"
#include "settings/settings_writer.hpp"
#include "settings/settings_profiles.hpp"
#include "settings/profile_identity.hpp"
#include "config/profile_layout.hpp"
#include "config/share_key_view.hpp"
#include "config/choice_dropdown.hpp"
#include "config/movement_toolbar.hpp"
#include "config/element_selector.hpp"
#include "input/config_controller_focus.hpp"
#include "controller_input.hpp"
#include "startup_profile.hpp"
#include "modules/treasure/treasure_module.hpp"

#include <cstdint>
#include <string>

namespace xiui
{
    class hud_runtime
    {
    public:
        hud_runtime(void);
        ~hud_runtime(void);

        auto Initialize(IAshitaCore* core, const char* installPath = nullptr) -> void;
        auto SetNativeMacroRunner(native_macro_runner runner, void* context) -> void { this->native_macros_.SetRunner(runner, context); }
        auto Shutdown(void) -> void;
        auto SetDevice(IDirect3DDevice8* device) -> bool;
        auto Render(const hud_snapshot& snapshot) -> void;
        auto RenderWorld(const hud_snapshot& snapshot) -> void;
        auto HandleMouseMessage(uint32_t message, WPARAM wParam, LPARAM lParam, bool blocked) -> bool;
        auto HandleChat(const chat_event& event) -> void
        {
            this->chat_.Receive(this->core_, event, this->chat_alerts_ready_ && this->ContextVisible(this->chat_));
            if (this->chat_alerts_ready_ && this->ContextVisible(this->chat_bubbles_))
                if (const auto* line = this->chat_.CapturedLine(event.sequence)) this->chat_bubbles_.Receive(this->core_, *line);
        }
        auto HandleKey(WPARAM key, LPARAM flags, bool blocked) -> bool;
        auto HandleController(const controller_event& event) -> bool;
        auto ControllerCapture(void) const -> controller_capture_mode;
        auto ControllerInputState(void) const -> uint32_t;
        auto CrossbarSettings(void) const -> controller_crossbar_settings;
        auto ControllerCaptured(void) const -> bool { return this->ControllerCapture() != controller_capture_mode::none; }
        auto UpdateControllerStatus(const controller_status& status) -> void;
        auto ChatFocused(void) const -> bool { return (this->ContextVisible(this->chat_) && (this->chat_.Focused() || this->chat_.HistoryFocused())) || (this->ContextVisible(this->chat_second_) && this->chat_second_.HistoryFocused()); }
        auto KeyboardFocused(void) const -> bool { return this->treasure_.InputFocused() || (this->ActionBarsInputAvailable(false, true) && this->actionbars_.KeyboardCapture()) || (this->SatchelInputAvailable(true) && this->satchel_.InputFocused()) || (this->visible_ && this->confirmation_key_release_ != 0) || (this->ContextVisible(this->chat_) && this->chat_.Focused()) || (this->visible_ && this->controller_surface_ == controller_surface::picker) || (this->visible_ && this->editing_ && (this->config_.search_open || this->config_.slider_input.Focused() || this->config_.text_input.Focused() || this->config_.choice_input.Opened() || this->profile_name_.Focused() || this->profile_dropdown_ || this->profile_actions_)); }
        auto CaptureChat(runtime_state& state) const -> void { this->chat_.Capture(state); }
        auto RestoreChat(const runtime_state& state) -> void
        {
            this->chat_.Restore(state);
            if (!this->ContextVisible(this->chat_)) this->chat_.Focus(false);
        }

        auto PostNotification(notification_event event) -> void;
        auto HandlePacket(uint16_t id, const uint8_t* data, uint32_t size) -> void;
        auto HandleCommand(const char* command) -> bool;
        auto ToggleEdit(void) -> bool;
        auto ExitEdit(void) -> void;
        auto RestoreEditing(bool editing) -> bool { return this->SetEditing(editing, false); }
        auto SetVisible(bool visible) -> void;
        auto ReloadSettings(void) -> void;
        auto ResetSettings(void) -> void;
        auto FlushSettings(void) const -> void;

        auto IsEditing(void) const -> bool;
        auto IsVisible(void) const -> bool;
        auto IsGpuShaderActive(void) const -> bool;
        auto NativeHudHiddenMask(void) const -> uint32_t;
        auto MenuLayout(void) const -> native_menu_layout { return this->commands_.Layout(this->visible_); }
#if defined(XIUI_ENABLE_TEST_FAULTS)
        auto SetRenderFaultForTesting(bool enabled) -> void;
#endif

    private:
#if defined(XIUI_ENABLE_ACTIONBAR_TESTS)
        friend struct actionbars_ui_test;
#endif
#if defined(XIUI_ENABLE_SATCHEL_TESTS)
        friend struct satchel_ui_test;
#endif
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
#endif
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
        friend struct controller_config_test;
        friend struct controller_runtime_test;
#endif
#if defined(XIUI_ENABLE_GRID_TESTS)
        friend struct layout_grid_test;
#endif
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
#if defined(XIUI_ENABLE_CAST_TESTS)
        friend struct casting_test;
#endif
        using rect = ui::rect;

        using cursor_shape = cursor_renderer::shape;

        enum class interaction : uint8_t
        {
            none,
            drag_module,
            drag_config,
            resize_config,
            module_toolbar,
            close_config,
            color_pick,
            option_slider,
            option_scrollbar,
        };

        IAshitaCore* core_{};
        startup_profile startup_profile_{};
        bool startup_render_recorded_{};
        IMouse* mouse_{};
        IDirect3DDevice8* device_{};
        ui::design_system design_system_{};
        ui::container_renderer container_renderer_{};
        ui::game_icon_cache game_icons_{};
        ui::native_ui_assets native_assets_{};
        native_window_config window_config_{};
        native_chat_filters chat_filters_{};
        ui::native_ui_sounds ui_sounds_{};
        native_chat_palette chat_palette_{};
        native_chat_input chat_input_{};
        chat_palette chat_colors_{};
        uint64_t next_chat_palette_update_{};
        ui::config_controls config_controls_{design_system_, &container_renderer_, &native_assets_};
        ui::experience_display_editor experience_display_{};
        cursor_renderer cursor_renderer_{};
        player_module player_{};
        player_cast_source player_cast_source_{};
        cast_bar_module cast_bar_{};
        fishing_module fishing_{};
        cast_cost_source cast_cost_source_{};
        heal_prediction_source heal_prediction_source_{};
        cast_cost_module cast_cost_{};
        recast_source recast_source_{};
        recast_module recast_{};
        target_module target_{};
        target_cast_module target_cast_{this->target_};
        target_ring_renderer target_rings_{};
        target_child_module target_recipient_{this->target_, false};
        target_child_module subtarget_{this->target_, true};
        std::array<target_mob_info_module, mob_info_group_count> mob_info_groups_{{
            {this->target_, 0}, {this->target_, 1}, {this->target_, 2}, {this->target_, 3}, {this->target_, 4},
            {this->target_, 5}, {this->target_, 6}, {this->target_, 7}, {this->target_, 8}, {this->target_, 9}}};
        enemy_list_module enemy_list_{};
        pet_source pet_source_{};
        pet_module pet_{};
        pet_module pet_target_{true};
        std::array<hud_module*, 6> pet_config_members_{};
        combat_settings combat_settings_{};
        combat_source combat_source_{};
        combat_timing_source timing_source_{};
        party_timing_source party_timing_source_{};
        bool timing_ready_{};
        uint64_t recovery_completion_{};
        party_source party_source_{};
        party_module party_{};
        party_module alliance_first_{1};
        party_module alliance_second_{2};
        experience_module experience_{};
        inventory_source inventory_source_{};
        satchel_source satchel_source_{};
        satchel_cache satchel_cache_{};
        satchel_actions satchel_actions_{};
        satchel_module satchel_{};
        action_store action_store_{};
        action_executor action_executor_{};
        native_macros native_macros_{};
        actionbars_module actionbars_{};
        std::array<actionbar_module, action_bar_count> actionbar_children_{{
            {this->actionbars_, 1}, {this->actionbars_, 2}, {this->actionbars_, 3}, {this->actionbars_, 4}, {this->actionbars_, 5},
            {this->actionbars_, 6}, {this->actionbars_, 7}, {this->actionbars_, 8}, {this->actionbars_, 9}, {this->actionbars_, 10}}};
        std::array<hud_module*, action_bar_count + 1> actionbar_members_{};
        std::array<bool, 256> actionbar_key_release_{};
        uint32_t actionbar_pointer_release_{};
        std::array<bool, 256> satchel_key_release_{};
        uint32_t satchel_pointer_release_{};
        std::array<inventory_module, 6> inventory_{{inventory_module{0}, inventory_module{1}, inventory_module{2}, inventory_module{3}, inventory_module{4}, inventory_module{5}}};
        std::array<hud_module*, 6> inventory_pages_{};
        std::array<hud_module*, 10> inventory_config_members_{};
        equipment_source equipment_source_{};
        equipment_module equipment_{};
        gil_source gil_source_{};
        gil_module gil_{};
        ready_check_module ready_check_{};
        status_effects_module buffs_{};
        status_effects_module debuffs_{true};
        notification_module notifications_{};
        std::array<notification_group_module, 5> notification_groups_{{{this->notifications_, 1}, {this->notifications_, 2}, {this->notifications_, 3}, {this->notifications_, 4}, {this->notifications_, 5}}};
        blue_magic_module blue_magic_{};
        phantom_roll_module phantom_roll_{};
        skillchain_source skillchain_source_{};
        magic_burst_module magic_burst_{};
        treasure_source treasure_source_{};
        treasure_module treasure_{};
        std::array<bool, 256> treasure_key_release_{};
        general_module general_{};
        game_hud_module game_hud_{};
        chat_module chat_{};
        chat_module chat_second_{true};
        chat_bubbles_module chat_bubbles_{};
        chat_module* chat_pointer_{};
        compass_module compass_{};
        minimap_module minimap_{};
        minimap_source minimap_source_{};
        minimap_maps minimap_maps_{};
        world_info_module world_info_{};
        std::array<world_info_part_module, 4> world_parts_{{{this->world_info_, 0}, {this->world_info_, 1}, {this->world_info_, 2}, {this->world_info_, 3}}};
        command_menu_module commands_{};
        std::array<hud_module*, 8> extras_pages_{};
        native_ui_state native_ui_{};
        module_visibility_source visibility_source_{};
        module_visibility_context visibility_context_{};
        bool visibility_pointer_release_{};
        world_source world_source_{};
        bool chat_capture_available_{};
        bool chat_alerts_ready_{};
        bool chat_palette_available_{};
        bool chat_activation_enter_down_{};
        bool chat_native_context_suspended_{};
        std::string chat_native_menu_recipient_{};
        uint64_t chat_native_menu_confirmed_at_{};
        bool chat_mouse_block_saved_{}, previous_chat_mouse_blocked_{};
        notification_source notification_source_{};
        module_registry modules_{};
        std::vector<module_state> default_module_states_{};
        config_session config_{};
        hud_module* interaction_module_{};
        enum class controller_surface : uint8_t { none, picker, config, chat, chat2, party, enemies, ready_check, equipment, target_status, buffs, debuffs, pet_status, notifications, satchel, hotbar, crossbar, treasure };
        uint64_t controller_notification_id_{};
        controller_surface controller_surface_{};
        controller_status controller_status_{};
        uint32_t controller_input_revision_{};
        bool controller_presented_{}, controller_pointer_down_{};
        bool keyboard_window_focus_{};
        bool controller_status_details_{};
        size_t controller_picker_index_{}, controller_party_group_{}, controller_equipment_slot_{}, controller_ready_button_{};
        std::string controller_hint_{};
        auto ConfigVisibleBounds(void) const -> rect
        {
            auto bounds = this->config_.bounds;
            if (this->config_.collapsed) bounds.height = 32;
            return bounds;
        }
        std::string settings_path_{};
        std::string default_settings_text_{};
        mutable settings_writer settings_writer_{};
        mutable settings_writer profile_writer_{};
        settings_profiles profiles_{};
        profile_identity_source profile_identity_{};
        std::string profile_character_{}, profile_observed_character_{};
        bool profile_tracking_{}, profile_character_ready_{};
        uint64_t next_profile_identity_update_{};
        ui::text_input profile_name_{};
        bool profile_dropdown_{};
        enum class profile_dialog : uint8_t { create, duplicate, share, import_profile, delete_profile, confirm_action, reset_defaults, rename };
        profile_dialog profile_dialog_{profile_dialog::create};
        bool profile_actions_{}, profile_key_copied_{};
        size_t profile_source_{};
        std::string profile_share_key_{};
        ui::share_key_view profile_key_view_{};
        std::optional<settings_store> profile_import_{};
        bool profile_import_lua_{}, profile_lua_activate_{}, profile_lua_links_{}, profile_lua_controls_{}, profile_lua_done_{};
        ui::text_input profile_lua_path_{};
        std::optional<lua_setup_preview> profile_lua_preview_{};
        size_t profile_lua_selection_{SIZE_MAX};
        std::string profile_lua_report_text_{}, profile_lua_status_{};
        ui::share_key_view profile_lua_report_{};
        int profile_scroll_{};
        size_t profile_keyboard_selection_{};
        hud_module* confirmation_module_{};
        size_t confirmation_option_{}, confirmation_view_{};
        std::string confirmation_message_{};
        WPARAM confirmation_key_release_{};
        std::string profile_error_{};
        bool settings_error_reported_{};
        bool visible_{true};
        bool editing_{false};
        bool layout_initialized_{false};
        layout_viewport layout_viewport_{};
        std::optional<settings_store> pending_layout_settings_{};
        bool mouse_block_state_saved_{false};
        bool previous_mouse_blocked_{false};
        bool mouse_captured_{false};
        uint32_t mouse_buttons_{};
#if defined(XIUI_ENABLE_TEST_FAULTS)
        bool raise_after_begin_for_testing_{false};
#endif
        int mouse_x_{};
        int mouse_y_{};
        bool mouse_down_{false};
        bool hud_target_pressed_{};
        const hud_module* hud_target_module_{};
        HWND mouse_capture_window_{};
        interaction interaction_{interaction::none};
        float drag_offset_x_{};
        float drag_offset_y_{};
        ui::point drag_origin_{};
        bool drag_moved_{};
        rect config_resize_original_{};
        ui::point config_resize_pointer_{};
        uint8_t config_resize_edges_{};
        std::vector<rect> layout_snap_targets_{};
        ui::layout_snap_result layout_snap_{};

        auto SetEditing(bool editing, bool playSound = true) -> bool;
        auto ContextVisible(const hud_module& module) const -> bool;
        auto TargetStatusVisible(void) const -> bool;
        auto UpdateVisibilityContext(void) -> void;
        auto ControllerSurfaceVisible(void) const -> bool;
        auto ControllerPartyVisible(size_t group) const -> bool;
        auto ControllerPresented(void) const -> bool;
        auto ReleaseControllerFocus(bool preserveExpandedHistory = false) -> void;
        auto UpdateControllerFocus(void) -> void;
        auto BeginControllerFocus(controller_surface surface) -> bool;
        auto ControllerSurfaces(void) const -> std::vector<controller_surface>;
        auto ControllerSurfaceName(controller_surface surface) const -> const char*;
        auto ControllerCommand(std::string_view command) -> bool;
        auto DrawControllerOverlay(const D3DVIEWPORT8& viewport, const module_render_context& context) -> void;
        auto ChatFocusBounds(void) const -> rect;
        auto KeyboardStatusFocusBounds(void) const -> rect;
        auto HandleControllerStatus(controller_action action) -> bool;
        auto ControllerStatusBounds(void) const -> rect;
        auto DrawStatusTooltips(const D3DVIEWPORT8& viewport, const module_render_context& context) -> void;
        auto ControllerPickerBounds(const D3DVIEWPORT8& viewport) const -> rect;
        auto HandleControllerPointer(uint32_t message, float x, float y, const D3DVIEWPORT8& viewport) -> bool;
        auto SelectHudTarget(uint32_t id, uint32_t index) -> bool;
        auto ControllerNativeBusy(void) const -> bool;
        auto GameWindowFocused(void) const -> bool;
        auto ControllerNativeMenuOpen(void) const -> bool;
        auto HandleControllerConfig(controller_action action, bool repeated) -> bool;
        auto DrawControllerConfig(const D3DVIEWPORT8& viewport) -> void;
        auto DrawInputModeControl(const D3DVIEWPORT8& viewport) -> void;
        auto ProcessInputModeClick(const D3DVIEWPORT8& viewport) -> bool;
        auto InputModeControlTarget(const D3DVIEWPORT8& viewport) const -> uint64_t;
        auto ResetControllerConfigFocus(void) -> void;
        auto ControllerConfigItems(config_controller_region region) -> std::vector<config_controller_item>;
        auto ActivateControllerConfigItem(const config_controller_item& item, controller_action action, bool repeated) -> bool;
        auto RefreshControllerConfigFocus(void) -> void;
        auto ConfigResizeEdges(float x, float y) const -> uint8_t;
        auto ToggleConfigSidebar(const D3DVIEWPORT8& viewport) -> void;
        auto BuildSettingsSearch(void) -> void;
        auto BeginSettingsSearch(void) -> void;
        auto CloseSettingsSearch(void) -> void;
        auto ClearSettingsSearch(void) -> void;
        auto AnimateSettingsSearch(uint64_t now) -> void;
        auto SettingsSearchBounds(void) const -> rect;
        auto UpdateSettingsSearch(void) -> void;
        auto MoveSettingsSearch(int steps) -> void;
        auto SelectSettingsSearch(size_t index) -> bool;
        auto HandleSettingsSearchKey(WPARAM key, LPARAM flags, HWND window) -> bool;
        auto HandleSettingsSearchController(controller_action action, bool repeated) -> bool;
        auto ProcessSettingsSearchClick(const D3DVIEWPORT8& viewport) -> bool;
        auto SettingsSearchTarget(const D3DVIEWPORT8& viewport) const -> uint64_t;
        auto DrawSettingsSearch(const D3DVIEWPORT8& viewport) -> void;
        auto ApplyConfigResize(const rect& original, uint8_t edges, float dx, float dy, const D3DVIEWPORT8& viewport) -> void;
        auto BeginConfigResize(bool keyboard = false) -> void;
        auto FinishConfigResize(bool apply) -> void;
        auto HandleConfigResize(controller_action action, bool repeated) -> bool;
        auto DrawControllerInputStatus(void) -> void;
        auto ControllerMovableModules(void) const -> std::vector<hud_module*>;
        auto FinishControllerMove(bool apply) -> void;
        auto HandleControllerMove(controller_action action, bool repeated) -> bool;
        auto DrawControllerMovement(const D3DVIEWPORT8& viewport) -> void;
        auto CenterModuleLayout(hud_module& module, size_t axis, const D3DVIEWPORT8& viewport) -> bool;
        auto ConfigSoundTarget(const D3DVIEWPORT8& viewport) -> uint64_t;
        auto HudSoundTarget(void) -> uint64_t;
        auto UpdateSoundHover(const D3DVIEWPORT8& viewport, bool active) -> void;
        auto PointerInputBlocked(bool blocked) const -> bool;
        auto SetChatMouseCapture(bool capture) -> void;
        auto ReleaseChatInput(bool preserveExpandedHistory = false) -> void;
        auto FinishConfigInput(bool apply, bool keepInvalid = true) -> bool;
        auto HandleConfigInputKey(WPARAM key, LPARAM flags) -> bool;
        auto CloseChoiceInput(void) -> void;
        auto SelectChoiceInput(int selected) -> void;
        auto PreviewChoiceInput(int selected) -> bool;
        auto HandleChoiceKey(WPARAM key, LPARAM flags) -> bool;
        auto ProcessChoiceClick(const D3DVIEWPORT8& viewport) -> bool;
        auto ChoiceSoundTarget(const D3DVIEWPORT8& viewport) const -> uint64_t;
        auto DrawChoiceInput(const D3DVIEWPORT8& viewport) -> void;
        auto HandleChatKey(WPARAM key, LPARAM flags, bool blocked, bool replacement, bool targeting, uint8_t nativeInput) -> bool;
        auto SatchelInputAvailable(bool pointer = false) const -> bool;
        auto SuspendSatchelInput(void) -> void;
        auto UpdateSatchel(uint64_t now) -> void;
        auto HandleSatchelCommand(std::string_view command) -> bool;
        auto HandleSatchelKey(WPARAM key, LPARAM flags, bool blocked) -> bool;
        auto TreasureInputAvailable(bool pointer = false) const -> bool;
        auto UpdateTreasure(uint64_t now) -> void;
        auto HandleTreasureKey(WPARAM key, LPARAM flags, bool blocked) -> bool;
        auto HandleTreasureMouse(uint32_t message, float x, float y, bool blocked) -> bool;
        auto ActionBarsNativePaletteOwned(bool candidate = false) const -> bool;
        auto ActionBarsInputAvailable(bool keyboardReplacement = false, bool pointer = false) const -> bool;
        auto ActionBarsPointerOwns(float x, float y) const -> bool;
        auto ActionBarsExecutionAvailable(bool keyboardReplacement = false) const -> bool;
        auto SuspendActionBarsInput(bool closeWindow = true) -> void;
        auto FinishActionBarsLaunch(void) -> void;
        auto UpdateActionBars(uint64_t now) -> void;
        auto HandleActionBarsCommand(std::string_view command) -> bool;
        auto HandleActionBarsKey(WPARAM, LPARAM, bool blocked) -> bool;
        auto HandleActionBarsMouse(uint32_t, WPARAM, float, float, bool blocked) -> bool;
        auto HandleSatchelMouse(uint32_t message, WPARAM buttons, float x, float y, bool blocked) -> bool;
        auto CycleWindowFocus(bool controller) -> bool;
        auto SyncNativeChatInput(IChatManager* manager, bool nativeMenu, bool targeting = false) -> void;
        auto SelectedChatRecipient(void) const -> std::string;
        auto ChatAt(float x, float y) -> chat_module*;
        auto HandleTargetMouse(uint32_t message, bool blocked) -> bool;
        auto ChatResizing(void) const -> bool { return this->chat_.Resizing() || this->chat_second_.Resizing(); }
        auto ChatResizeAt(float x, float y) -> bool;
        auto MinimapPointerOwns(float x, float y) const -> bool;
        auto LoadSettings(void) -> void;
        auto LoadSettings(const settings_store& store, layout_viewport viewport = {}) -> void;
        auto SaveSettings(void) const -> void;
        auto CaptureSettings(void) const -> settings_store;
        auto DrainSettings(void) const -> bool;
        auto DrawProfileControls(void) -> void;
        auto DrawProfileOverlay(const D3DVIEWPORT8& viewport) -> void;
        auto ProfileSoundTarget(const D3DVIEWPORT8& viewport) const -> uint64_t;
        auto ProfileSurface(const D3DVIEWPORT8& viewport) const -> bool;
        auto ProcessProfileClick(const D3DVIEWPORT8& viewport) -> bool;
        auto HandleProfileKey(WPARAM key, LPARAM flags) -> bool;
        auto SelectProfile(size_t index) -> bool;
        auto UpdateCharacterProfile(uint64_t now) -> void;
        auto ObserveProfileCharacter(std::string character) -> bool;
        auto BindObservedProfile(void) -> void;
        auto CreateProfile(void) -> bool;
        auto RenameProfile(void) -> bool;
        auto DeleteProfile(void) -> bool;
        auto CloseProfileControls(void) -> void;
        auto ActivateConfigAction(hud_module& module, size_t option) -> bool;
        auto ApplyConfirmedAction(void) -> bool;
        auto OpenProfileDialog(profile_dialog mode) -> bool;
        auto SetProfileImportSource(bool lua) -> void;
        auto PreviewLuaProfiles() -> bool;
        auto BrowseLuaProfile() -> void;
        auto CreateLuaProfiles() -> bool;
        auto CycleLuaProfile(int direction) -> void;
        auto LuaImportReady() const -> bool;
        auto LuaImportClick(const D3DVIEWPORT8& viewport) -> bool;
        auto LuaImportTarget(const D3DVIEWPORT8& viewport) const -> uint64_t;
        auto ActivateLuaImportControl(size_t index) -> void;
        auto HandleLuaImportKey(WPARAM key, LPARAM flags) -> bool;
        auto HandleLuaImportController(controller_action action) -> bool;
        auto DrawLuaProfileImport(const D3DVIEWPORT8& viewport) -> void;
        auto ProfileActionAvailable(size_t index) const -> bool;
        auto OpenProfileAction(size_t index) -> bool;
        auto MoveProfileAction(int direction) -> void;
        auto ConfirmingAction(void) const -> bool { return this->profile_dialog_ == profile_dialog::confirm_action || this->profile_dialog_ == profile_dialog::reset_defaults; }
        auto CopyProfileKey(void) -> bool;
        auto PasteProfileKey(void) -> bool;
        auto LoadProfileKey(std::string_view key) -> bool;
        auto EnsureLayout(const D3DVIEWPORT8& viewport) -> void;
        auto ProcessInput(const D3DVIEWPORT8& viewport) -> void;
        auto ProcessPointerMessage(uint32_t message, uint32_t buttons, const D3DVIEWPORT8& viewport) -> bool;
        auto FinishInteraction(void) -> void;
        auto ReleasePointerCapture(void) -> void;
        auto UpdateMouseCapture(const D3DVIEWPORT8& viewport, uint32_t buttons) -> bool;
        auto ProcessConfigClick(const D3DVIEWPORT8& viewport) -> bool;
        auto FocusModuleConfig(hud_module& module) -> void;
        auto BeginPanelInteraction(const D3DVIEWPORT8& viewport) -> void;
        auto UpdateInteraction(const D3DVIEWPORT8& viewport) -> void;
        auto MovementBounds(const hud_module& module) const -> rect;
        auto SnapModuleLayout(const D3DVIEWPORT8& viewport) -> void;
        auto DrawLayoutBackground(const rect& viewport) -> void;
        auto MovementSelectionModule(void) const -> hud_module*;
        auto MovementToolbar(const D3DVIEWPORT8& viewport) const -> std::optional<ui::movement_toolbar>;
        auto MovementToolbarTarget(const D3DVIEWPORT8& viewport) const -> uint64_t;
        auto ProcessMovementToolbarClick(const D3DVIEWPORT8& viewport) -> bool;
        auto DrawMovementToolbar(const D3DVIEWPORT8& viewport) -> void;
        auto ClampLayout(const D3DVIEWPORT8& viewport) -> void;

        struct config_page
        {
            const char* id; const char* name; hud_module* primary; hud_module* secondary{}; hud_module* tertiary{};
            std::span<hud_module* const> members{};
            size_t primary_count{1};
            const char* primary_section_name{};
            size_t primary_section_index{};
            size_t grouped_views{1};
            auto Count(void) const -> size_t { return !this->members.empty() ? this->members.size() : this->tertiary != nullptr ? 3 : this->secondary != nullptr ? 2 : 1; }
            auto Module(size_t index) const -> hud_module* { return !this->members.empty() ? this->members[index] : index == 0 ? this->primary : index == 1 ? this->secondary : this->tertiary; }
            auto Views(void) const -> std::span<const char* const> { return this->primary != nullptr ? this->primary->ConfigViews() : std::span<const char* const>{}; }
            auto SectionCount(void) const -> size_t { return !this->Views().empty() ? this->Views().size() - this->grouped_views + 1 : this->Count() - this->primary_count + 1; }
            auto SectionView(size_t index) const -> size_t
            {
                if (index == 0 && this->grouped_views > 1)
                    return this->primary->ConfigView() < this->grouped_views ? this->primary->ConfigView() : 0;
                return index + this->grouped_views - 1;
            }
            auto SectionIndex(size_t index) const -> size_t { return index == this->primary_section_index ? 0 : index < this->primary_section_index ? index + 1 : index; }
            auto SectionModule(size_t index) const -> hud_module*
            {
                const auto group = this->SectionIndex(index);
                return !this->Views().empty() ? this->primary : this->Module(group == 0 ? 0 : group + this->primary_count - 1);
            }
            auto SectionName(size_t index) const -> const char*
            {
                if (!this->Views().empty())
                    return index == 0 && this->grouped_views > 1 && this->primary_section_name != nullptr ? this->primary_section_name : this->Views()[this->SectionView(index)];
                return this->SectionIndex(index) == 0 && this->primary_section_name != nullptr ? this->primary_section_name : this->SectionModule(index)->DisplayName();
            }
            auto SectionContains(size_t index, const hud_module* module) const -> bool
            {
                if (!this->Views().empty())
                {
                    if (this->primary == module) return index == 0 && this->grouped_views > 1 ? this->primary->ConfigView() < this->grouped_views : this->primary->ConfigView() == this->SectionView(index);
                    const auto view = this->SectionView(index);
                    if (index == 0 && this->grouped_views > 1)
                        for (size_t member = 1; member < this->grouped_views && member < this->Count(); ++member) if (this->Module(member) == module) return true;
                    return view != 0 && view < this->Count() && this->Module(view) == module;
                }
                if (this->SectionIndex(index) != 0) return this->SectionModule(index) == module;
                for (size_t i = 0; module != nullptr && i < this->primary_count; ++i) if (this->Module(i) == module) return true;
                return false;
            }
            auto Contains(const hud_module* module) const -> bool
            {
                for (size_t i = 0; module != nullptr && i < this->Count(); ++i) if (this->Module(i) == module) return true;
                return false;
            }
        };
        static constexpr size_t config_page_count = 15;
        auto ConfigPages(void) -> std::array<config_page, config_page_count>;
        auto ConfigPageVisible(const config_page& page) const -> bool;
        auto VisibleConfigPages(void) const -> std::vector<config_page>;
        auto ActiveConfigPage(void) -> config_page;
        auto SelectConfigModule(hud_module* module, std::optional<size_t> view = std::nullopt) -> void;
        auto SelectConfigTopic(size_t index) -> void;
        auto SaveConfigSelection(const settings_store& store) const -> void;
        auto LoadConfigSelection(const settings_store& store) -> void;
        auto HasConfigSections(void) const -> bool
        {
            return const_cast<hud_runtime*>(this)->ActiveConfigPage().SectionCount() > 1;
        }
        auto IsInventorySettings(void) const -> bool
        {
            for (const auto* module : this->inventory_pages_) if (this->config_.module == module) return true;
            return false;
        }
        auto HasConfigSubmenu(void) const -> bool { return this->config_.module != nullptr && !this->config_.module->ConfigSections().empty(); }
        struct config_categories
        {
            std::array<module_option_group, 5> groups{};
            size_t count{};
        };
        auto ConfigCategories(void) const -> config_categories;
        auto NormalizeConfigSelection(void) -> void;
        auto IsExperienceDisplay(void) const -> bool { return this->config_.module == &this->experience_ && this->experience_.UsesDisplayEditor(this->config_.section); }
        auto IsAttribution(void) const -> bool { return this->config_.module == &this->general_ && this->config_.section == general_module::attribution_section; }
        auto IsXIUIContributors(void) const -> bool { return this->IsAttribution() && this->general_.AttributionView() == 1; }
        auto IsGeneralAppearance(void) const -> bool { return this->config_.module == &this->general_ && this->config_.section == general_module::appearance_section; }
        auto ConfigLayout(void) const -> ui::config_layout;
        auto ConfigScrollbar(void) const -> ui::config_scrollbar;
        auto ProcessConfigScrollbarClick(void) -> bool;
        auto UpdateConfigScrollbar(void) -> void;
        auto DrawConfigScrollbar(void) -> void;
        auto ElementSelector(void) const -> ui::element_selector;
        auto ElementSelectorLayout(const ui::element_selector& selector, const ui::config_layout& layout) const -> ui::element_selector_layout;
        auto ElementSelectorTarget(void) const -> uint64_t;
        auto ProcessElementSelectorClick(void) -> bool;
        auto DrawElementSelector(void) -> void;
        auto DrawConfig(const D3DVIEWPORT8& viewport) -> void;
        auto DrawContributorList(void) -> void;
        auto DrawColorEditor(const D3DVIEWPORT8& viewport) -> void;
        auto DrawConfigTooltip(const D3DVIEWPORT8& viewport) -> void;
        auto DrawCursor(const D3DVIEWPORT8& viewport) -> void;

        auto FillRect(const rect& bounds, D3DCOLOR color) const -> void;
        auto GradientRect(const rect& bounds, D3DCOLOR left, D3DCOLOR right) const -> void;
        auto BorderRect(const rect& bounds, D3DCOLOR color, float thickness = 1.0f) const -> void;
        auto ResolveCursorShape(const D3DVIEWPORT8& viewport) const -> cursor_shape;
        auto ApplyPickedColor(void) -> void;
        auto SetActiveSliderFromMouse(void) -> void;
        auto GroupOptionSlots(void) const -> std::vector<size_t>;
        auto GroupOptionCount(void) const -> size_t;
        auto MatchesConfigOption(size_t index) const -> bool;
        auto GroupOptionIndex(size_t filteredIndex) const -> size_t;
        auto ClampOptionScroll(void) -> void;

        static auto Contains(const rect& bounds, float x, float y) -> bool;
    };
} // namespace xiui
