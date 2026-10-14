#pragma once

#include "Ashita.h"
#include "design_system.hpp"
#include "config/config_controls.hpp"
#include "config/color_picker.hpp"
#include "config/experience_display_editor.hpp"
#include "input/cursor_renderer.hpp"
#include "modules/experience/experience_module.hpp"
#include "modules/module_registry.hpp"
#include "modules/player/player_module.hpp"
#include "modules/target/target_module.hpp"
#include "modules/enemylist/enemy_list_module.hpp"
#include "modules/party/party_module.hpp"
#include "modules/inventory/inventory_module.hpp"
#include "modules/equipment/equipment_module.hpp"
#include "modules/gil/gil_module.hpp"
#include "modules/readycheck/ready_check_module.hpp"
#include "modules/castbar/cast_bar_module.hpp"
#include "modules/castcost/cast_cost_module.hpp"
#include "modules/recast/recast_module.hpp"
#include "runtime_api.hpp"
#include "modules/notifications/notification_module.hpp"
#include "modules/game_hud/game_hud_module.hpp"
#include "modules/general/general_module.hpp"
#include "modules/chat/chat_module.hpp"
#include "modules/compass/compass_module.hpp"
#include "modules/world/world_info_module.hpp"
#include "modules/commands/command_menu_module.hpp"
#include "world/world_source.hpp"
#include "ui/native_window_config.hpp"
#include "ui/native_ui_sounds.hpp"
#include "native_chat_palette.hpp"
#include "notifications/notification_source.hpp"
#include "settings/settings_writer.hpp"
#include "settings/settings_profiles.hpp"
#include "config/profile_layout.hpp"
#include "config/share_key_view.hpp"
#include "config/choice_dropdown.hpp"
#include "config/movement_toolbar.hpp"
#include "config/element_selector.hpp"

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
        auto Shutdown(void) -> void;
        auto SetDevice(IDirect3DDevice8* device) -> bool;
        auto Render(const hud_snapshot& snapshot) -> void;
        auto HandleMouseMessage(uint32_t message, WPARAM wParam, LPARAM lParam, bool blocked) -> bool;
        auto HandleChat(const chat_event& event) -> void { this->chat_.Receive(this->core_, event, this->chat_alerts_ready_ && this->visible_); }
        auto HandleKey(WPARAM key, LPARAM flags, bool blocked) -> bool;
        auto ChatFocused(void) const -> bool { return this->visible_ && (this->chat_.Focused() || this->chat_.HistoryFocused()); }
        auto KeyboardFocused(void) const -> bool { return this->ChatFocused() || (this->visible_ && this->editing_ && (this->slider_input_.Focused() || this->option_text_input_.Focused() || this->choice_input_.Opened() || this->profile_name_.Focused() || this->profile_dropdown_ || this->profile_actions_)); }
        auto CaptureChat(runtime_state& state) const -> void { this->chat_.Capture(state); }
        auto RestoreChat(const runtime_state& state) -> void
        {
            this->chat_.Restore(state);
            if (!this->visible_) this->chat_.Focus(false);
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
        auto NativeHudHiddenMask(void) const -> uint32_t { return this->game_hud_.HiddenMask(); }
        auto MenuLayout(void) const -> native_menu_layout { return this->commands_.Layout(this->visible_); }
#if defined(XIUI_ENABLE_TEST_FAULTS)
        auto SetRenderFaultForTesting(bool enabled) -> void;
#endif

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
#endif
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
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
            module_toolbar,
            close_config,
            color_pick,
            option_slider,
        };

        IAshitaCore* core_{};
        IMouse* mouse_{};
        IDirect3DDevice8* device_{};
        ui::design_system design_system_{};
        ui::container_renderer container_renderer_{};
        ui::game_icon_cache game_icons_{};
        ui::native_ui_assets native_assets_{};
        native_window_config window_config_{};
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
        cast_cost_source cast_cost_source_{};
        cast_cost_module cast_cost_{};
        recast_source recast_source_{};
        recast_module recast_{};
        target_module target_{};
        enemy_list_module enemy_list_{};
        combat_source combat_source_{};
        combat_timing_source timing_source_{};
        bool timing_ready_{};
        uint64_t recovery_completion_{};
        party_source party_source_{};
        party_module party_{};
        party_module alliance_first_{1};
        party_module alliance_second_{2};
        experience_module experience_{};
        inventory_source inventory_source_{};
        std::array<inventory_module, 6> inventory_{{inventory_module{0}, inventory_module{1}, inventory_module{2}, inventory_module{3}, inventory_module{4}, inventory_module{5}}};
        std::array<hud_module*, 6> inventory_pages_{};
        std::array<hud_module*, 8> inventory_config_members_{};
        equipment_source equipment_source_{};
        equipment_module equipment_{};
        gil_source gil_source_{};
        gil_module gil_{};
        ready_check_module ready_check_{};
        notification_module notifications_{};
        general_module general_{};
        game_hud_module game_hud_{};
        chat_module chat_{};
        chat_module chat_second_{true};
        chat_module* chat_pointer_{};
        compass_module compass_{};
        world_info_module world_info_{};
        command_menu_module commands_{};
        std::array<hud_module*, 4> extras_pages_{};
        native_ui_state native_ui_{};
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
        hud_module* active_module_{};
        hud_module* interaction_module_{};
        rect config_bounds_{-1.0f, 20.0f, ui::config_layout::default_width, ui::config_layout::default_height};
        bool config_collapsed_{};
        auto ConfigVisibleBounds(void) const -> rect
        {
            auto bounds = this->config_bounds_;
            if (this->config_collapsed_) bounds.height = 32;
            return bounds;
        }
        std::string settings_path_{};
        std::string default_settings_text_{};
        mutable settings_writer settings_writer_{};
        mutable settings_writer profile_writer_{};
        settings_profiles profiles_{};
        ui::text_input profile_name_{};
        bool profile_dropdown_{};
        enum class profile_dialog : uint8_t { create, duplicate, share, import_profile, delete_profile };
        profile_dialog profile_dialog_{profile_dialog::create};
        bool profile_actions_{}, profile_key_copied_{};
        size_t profile_source_{};
        std::string profile_share_key_{};
        ui::share_key_view profile_key_view_{};
        std::optional<settings_store> profile_import_{};
        int profile_scroll_{};
        size_t profile_keyboard_selection_{};
        std::string profile_error_{};
        bool settings_error_reported_{};
        bool visible_{true};
        bool editing_{false};
        bool layout_initialized_{false};
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
        HWND mouse_capture_window_{};
        D3DCOLOR* active_color_{};
        ui::color_picker color_picker_{};
        float* active_slider_{};
        ui::slider_value_input slider_input_{};
        hud_module* slider_input_module_{};
        size_t slider_input_option_index_{};
        rect slider_input_bounds_{};
        ui::text_input option_text_input_{};
        hud_module* text_input_module_{};
        size_t text_input_option_index_{};
        std::string* text_input_value_{};
        rect text_input_bounds_{};
        ui::choice_dropdown choice_input_{};
        hud_module* choice_input_module_{};
        size_t choice_input_option_index_{};
        float active_slider_minimum_{};
        float active_slider_maximum_{1.0f};
        float active_slider_step_{0.1f};
        rect active_slider_row_{};
        size_t active_option_index_{};
        module_option_group active_option_group_{module_option_group::general};
        size_t active_config_section_{};
        int option_scroll_{};
        std::string config_tooltip_{};
        rect config_tooltip_anchor_{};
        bool reset_defaults_armed_{};
        interaction interaction_{interaction::none};
        float drag_offset_x_{};
        float drag_offset_y_{};
        ui::point drag_origin_{};
        bool drag_moved_{};
        std::vector<rect> layout_snap_targets_{};
        ui::layout_snap_result layout_snap_{};

        auto SetEditing(bool editing, bool playSound = true) -> bool;
        auto ConfigSoundTarget(const D3DVIEWPORT8& viewport) -> uint64_t;
        auto HudSoundTarget(void) -> uint64_t;
        auto UpdateSoundHover(const D3DVIEWPORT8& viewport, bool active) -> void;
        auto SetChatMouseCapture(bool capture) -> void;
        auto ReleaseChatInput(void) -> void;
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
        auto SyncNativeChatInput(IChatManager* manager, bool nativeMenu, bool targeting = false) -> void;
        auto SelectedChatRecipient(void) const -> std::string;
        auto ChatAt(float x, float y) -> chat_module*;
        auto HandleTargetMouse(uint32_t message, bool blocked) -> bool;
        auto ChatResizing(void) const -> bool { return this->chat_.Resizing() || this->chat_second_.Resizing(); }
        auto ChatResizeAt(float x, float y) -> bool;
        auto LoadSettings(void) -> void;
        auto LoadSettings(const settings_store& store) -> void;
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
        auto CreateProfile(void) -> bool;
        auto DeleteProfile(void) -> bool;
        auto CloseProfileControls(void) -> void;
        auto OpenProfileDialog(profile_dialog mode) -> bool;
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
            auto Count(void) const -> size_t { return !this->members.empty() ? this->members.size() : this->tertiary != nullptr ? 3 : this->secondary != nullptr ? 2 : 1; }
            auto Module(size_t index) const -> hud_module* { return !this->members.empty() ? this->members[index] : index == 0 ? this->primary : index == 1 ? this->secondary : this->tertiary; }
            auto Views(void) const -> std::span<const char* const> { return this->primary != nullptr ? this->primary->ConfigViews() : std::span<const char* const>{}; }
            auto SectionCount(void) const -> size_t { return !this->Views().empty() ? this->Views().size() : this->Count() - this->primary_count + 1; }
            auto SectionModule(size_t index) const -> hud_module* { return !this->Views().empty() ? this->primary : this->Module(index == 0 ? 0 : index + this->primary_count - 1); }
            auto SectionName(size_t index) const -> const char* { return !this->Views().empty() ? this->Views()[index] : this->SectionModule(index)->DisplayName(); }
            auto SectionContains(size_t index, const hud_module* module) const -> bool
            {
                if (!this->Views().empty()) return this->primary == module && this->primary->ConfigView() == index;
                if (index != 0) return this->SectionModule(index) == module;
                for (size_t i = 0; module != nullptr && i < this->primary_count; ++i) if (this->Module(i) == module) return true;
                return false;
            }
            auto Contains(const hud_module* module) const -> bool
            {
                for (size_t i = 0; module != nullptr && i < this->Count(); ++i) if (this->Module(i) == module) return true;
                return false;
            }
        };
        static constexpr size_t config_page_count = 12;
        auto ConfigPages(void) -> std::array<config_page, config_page_count>;
        auto ConfigPageVisible(const config_page& page) const -> bool;
        auto VisibleConfigPages(void) const -> std::vector<config_page>;
        auto ActiveConfigPage(void) -> config_page;
        auto SelectConfigModule(hud_module* module, std::optional<size_t> view = std::nullopt) -> void;
        auto HasConfigSections(void) const -> bool
        {
            return const_cast<hud_runtime*>(this)->ActiveConfigPage().SectionCount() > 1;
        }
        auto IsInventorySettings(void) const -> bool
        {
            for (const auto* module : this->inventory_pages_) if (this->active_module_ == module) return true;
            return false;
        }
        auto HasConfigSubmenu(void) const -> bool { return this->active_module_ != nullptr && !this->active_module_->ConfigSections().empty(); }
        struct config_categories
        {
            std::array<module_option_group, 5> groups{};
            size_t count{};
        };
        auto ConfigCategories(void) const -> config_categories;
        auto NormalizeConfigSelection(void) -> void;
        auto IsExperienceDisplay(void) const -> bool { return this->active_module_ == &this->experience_ && this->experience_.UsesDisplayEditor(this->active_config_section_); }
        auto IsAttribution(void) const -> bool { return this->active_module_ == &this->general_ && this->active_config_section_ == general_module::attribution_section; }
        auto IsXIUIContributors(void) const -> bool { return this->IsAttribution() && this->general_.AttributionView() == 1; }
        auto IsGeneralAppearance(void) const -> bool { return this->active_module_ == &this->general_ && this->active_config_section_ == general_module::appearance_section; }
        auto ConfigLayout(void) const -> ui::config_layout;
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
        auto GroupOptionCount(void) const -> size_t;
        auto MatchesConfigOption(size_t index) const -> bool;
        auto GroupOptionIndex(size_t filteredIndex) const -> size_t;
        auto ClampOptionScroll(void) -> void;

        static auto Contains(const rect& bounds, float x, float y) -> bool;
    };
} // namespace xiui
