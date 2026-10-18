#pragma once

#include "action_store.hpp"
#include "action_executor.hpp"
#include "action_import.hpp"
#include "actionbar_slot_style.hpp"
#include "combat/skillchain_source.hpp"
#include "combat/combat_settings.hpp"
#include "controller_visuals.hpp"
#include "modules/module.hpp"
#include "config/text_input.hpp"
#include "config/config_text.hpp"
#include "config/config_scrollbar.hpp"
#include "config/choice_dropdown.hpp"
#include "config/slider_value_input.hpp"
#include "controller_input.hpp"
#include "ui/focus_frame.hpp"
#include "ui/native_ui_sounds.hpp"
#include "input/cursor_renderer.hpp"

#include <optional>
#include <unordered_map>
#include <utility>

namespace xiui
{
    struct actionbar_binding
    {
        uint16_t key{};
        uint8_t modifiers{};
        auto operator==(const actionbar_binding&) const -> bool = default;
    };

    struct actionbar_presentation
    {
        module_state state{false, {-1, -1, 520, 92}, 120, 44};
        float columns{12}, slots{12}, size{36}, gap{4};
        bool locked{true}, heading{true}, labels{}, bindings{true}, resources{true}, cooldowns{true};
        bool empty{true}, background{}, tooltips{true};
        actionbar_slot_style style{};
        std::array<actionbar_binding, action_bar_slots> keys{};
    };

    class actionbars_module final : public hud_module
    {
    public:
        actionbars_module();
        auto Id(void) const -> const char* override { return "hotbar"; }
        auto DisplayName(void) const -> const char* override { return "Action Bars"; }
        auto State(void) -> module_state& override { return this->bars_[0].state; }
        auto State(void) const -> const module_state& override { return this->bars_[0].state; }
        auto Initialize(IAshitaCore*, action_store*, action_executor*) -> void;
        auto SetSounds(ui::native_ui_sounds* value) -> void { this->sounds_ = value; }
        auto SetSkillchains(const skillchain_source* source) -> void { this->skillchains_ = source; }
        auto SkillchainPredictionsEnabled(void) const -> bool { return this->skillchain_predictions_; }
        auto SetCombatSettings(combat_settings* settings) -> void { this->combat_settings_ = settings; }
        auto SkillchainRules(void) const -> server_rules { return this->CombatSettings().Rules(); }
        auto SetFocusStyle(const ui::focus_frame_style& value) -> void { this->focus_style_ = value; }
        auto SetConfigText(const ui::config_text_settings* value) -> void { this->config_text_ = value; }
        auto UpdateControllerStatus(const controller_status& value) -> void { this->controller_status_ = value; }
        auto SetSurfaceAvailability(const std::array<bool, action_bar_count + 1>& value) -> void { this->available_ = value; }
        auto SetPreview(bool value) -> void { this->crossbar_preview_ = value || (this->dialog_ != dialog::none || this->browsing_) && this->selected_.surface == action_surface::crossbar; }
        auto ControllerBindings(void) const -> const controller_crossbar_settings& { return this->controller_settings_; }
        auto BarState(size_t index) -> module_state& { return this->bars_[std::min(index, action_bar_count)].state; }
        auto BarState(size_t index) const -> const module_state& { return this->bars_[std::min(index, action_bar_count)].state; }
        auto BarVisible(size_t) const -> bool;
        auto RenderBar(size_t, const module_render_context&) -> void;
        auto RenderOverlay(const module_render_context&) -> void;
        auto Update(uint64_t now) -> void;
        auto SuspendInput(void) -> void;
        auto SuspendCrossbarInput(void) -> bool;
        auto BeginControllerBrowse(bool crossbar = false) -> void;
        auto EndControllerBrowse(void) -> void { if (this->browsing_) this->SuspendInput(); }
        auto HandleControllerAction(controller_action, bool repeated) -> bool;
        auto HandleCrossbarEvent(const controller_event&) -> bool;
        auto HandleMouse(uint32_t, WPARAM, float x, float y, bool blocked, bool editing = false) -> bool;
        auto HandleKey(WPARAM, LPARAM) -> bool;
        auto ReplacesNativeMacroKey(WPARAM) const -> bool;
        auto ReplaceNativeMacros(void) const -> bool { return this->allow_native_keys_; }
        auto ObserveNativeMacroKey(WPARAM, LPARAM, bool claimed = false) -> void;
        auto NativeMacroKeysOwned(void) const -> bool { return this->native_macro_modifiers_ != 0; }
        auto KeyboardFocused(void) const -> bool { return this->input_.Focused() || this->binding_capture_ || this->setup_value_input_.Focused(); }
        auto InputFocused(void) const -> bool { return this->browsing_ || this->KeyboardFocused() || this->dialog_ != dialog::none && (!this->IsWindowDialog(this->dialog_) || this->window_focused_) || this->WindowDragging() || this->WindowResizing(); }
        auto DialogOpen(void) const -> bool { return this->dialog_ != dialog::none; }
        auto ModalDialogOpen(void) const -> bool { return this->dialog_ != dialog::none && !this->IsWindowDialog(this->dialog_); }
        auto FloatingWindowOpen(void) const -> bool { return this->WindowDialog() != dialog::none; }
        auto WindowFocused(void) const -> bool { return this->window_focused_; }
        auto KeyboardCapture(void) const -> bool { return this->KeyboardFocused() || this->setup_choice_input_.Opened() || this->navigation_focus_ || this->dialog_ != dialog::none && !this->IsWindowDialog(this->dialog_); }
        auto AssignmentDragging(void) const -> bool { return this->library_drag_.has_value() || this->dragging_; }
        auto CancelPointerGesture(void) -> void;
        auto SetWindowFocused(bool) -> void;
        auto WindowBounds(void) const -> ui::rect;
        auto WindowConfigView(void) const -> size_t;
        auto LibraryStatus(void) const -> std::string;
        auto WindowResizeEdgesAt(float x, float y) const -> uint8_t;
        auto WindowDragging(void) const -> bool { return this->window_gesture_ == window_gesture::move; }
        auto WindowResizing(void) const -> bool { return this->window_gesture_ == window_gesture::resize; }
        auto PointerDown(void) const -> bool { return this->pointer_down_; }
        auto PointerOwns(float, float) const -> bool;
        auto PointerCursor(float, float) const -> cursor_renderer::shape;
        auto SetHover(float x, float y) -> void { this->hover_x_ = x; this->hover_y_ = y; }
        auto TakeSettingsChanged(void) -> bool;
        auto OpenEditor(bool crossbar = false, size_t bar = 0, size_t slot = 0) -> void;
        auto OpenLibrary(bool crossbar = false, size_t bar = 0, size_t slot = 0) -> void;
        auto OpenPalettes(bool crossbar = false, size_t bar = 0) -> void;
        auto OpenSetup(void) -> void;
        auto OpenKeybinds(bool crossbar = false, size_t bar = 0) -> void;
        auto OpenImport(void) -> void;
        auto SelectedBar(void) const -> size_t { return this->view_; }
        auto TakeConfigurationRequest(void) -> std::optional<size_t> { return std::exchange(this->configuration_request_, {}); }
        auto TakeMagicBurstSettingsRequest(void) -> bool { return std::exchange(this->magic_burst_settings_request_, false); }
        auto ConfigViews(void) const -> std::span<const char* const> override;
        auto ConfigView(void) const -> size_t override { return this->view_; }
        auto SelectConfigView(size_t value) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t, size_t) -> bool override;
        auto OptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto Option(size_t) -> module_option override;
        auto OnOptionChanged(size_t) -> void override;
        auto ApplyTextDefaults(const ui::theme&) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visitor) -> void override { VisitTextProfiles(visitor, this->text_); }
        auto ResetDefaults(const ui::theme&) -> void override;
        auto Load(const settings_store&) -> void override;
        auto Save(const settings_store&) const -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->BarVisible(0); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override { this->RenderBar(0, context); }

    private:
#if defined(XIUI_ENABLE_ACTIONBAR_TESTS)
        friend struct actionbars_ui_test;
#endif
        enum option : size_t { enabled, locked, edit, palettes, import, scope, pet_aware, columns, slots, size, gap,
            heading, labels, bindings, resources, cooldowns, empty, background, tooltips, opacity, padding,
            bank, setup, crossbar_enabled, native_replace, expanded, double_tap, palette_cycle, inactive,
            prompts, prompt_theme, double_window, native_keys, selected_text, bar_selector, shape,
            slot_frame, slot_radius, slot_border_width, slot_icon_inset, slot_icon_opacity, slot_shadow, slot_sheen,
            slot_fill, slot_empty_fill, slot_border, slot_icon_tint, slot_unavailable_tint, slot_inactive_tint, slot_inactive_opacity,
            slot_hover_highlight, slot_hover_color, slot_press_highlight, slot_pressed_color, slot_activation_flash,
            slot_cooldown_style, slot_cooldown_color, slot_cooldown_text,
            slot_binding_anchor, slot_binding_x, slot_binding_y, slot_count_anchor, slot_count_x, slot_count_y,
            slot_timer_anchor, slot_timer_x, slot_timer_y, slot_mp_cost, slot_cost_anchor, slot_cost_x, slot_cost_y,
            slot_apply_all, slot_reset, crossbar_animations, skillchain_predictions, skillchain_rules, magic_burst_settings, keybinds, base_options };
        enum class dialog : uint8_t { none, picker, editor, palettes, name, delete_palette, delete_macro, import, setup, palette_dropdown, slot_menu, bar_menu, keybinds };
        enum class control_kind : uint8_t { button, field, choice, list, toggle, label, slider, dropdown };
        enum class window_gesture : uint8_t { none, move, resize };
        enum class slot_pass : uint8_t { all, background, content, foreground };
        enum keybind_control : int { keybind_bar = 1100, keybind_bank, keybind_key_first = 1200, keybind_clear_first = 1300 };
        struct address
        {
            action_surface surface{};
            size_t bar{}, slot{};
            auto operator==(const address&) const -> bool = default;
        };
        struct control
        {
            int id{};
            ui::rect bounds{};
            std::string text{}, hint{};
            control_kind kind{};
            bool enabled{true}, selected{}, danger{};
            float value{}, minimum{}, maximum{}, step{}, label_width{-1};
        };
        struct hit { address slot{}; ui::rect bounds{}; int header{}; };
        struct inspection { action_availability value{}; uint64_t at{}, revision{}; };
        struct library_metrics
        {
            float name_height{17}, caption_height{15}, nav_height{17}, button_height{17};
            float sidebar_width{136}, filter_width{108}, create_width{108}, search_with_shortcut_width{300};
        };
        struct library_layout
        {
            ui::rect bounds{}, search{}, your_actions{}, all_actions{}, sidebar{}, list{}, scrollbar{}, footer{}, done{};
            std::array<ui::rect, 3> create{};
            float row_pitch{}, row_height{}, column_pitch{}, column_width{}, name_height{}, caption_height{}, nav_pitch{};
            size_t rows{}, columns{2};
        };
        std::array<actionbar_presentation, action_bar_count + 1> bars_{};
        std::array<bool, action_bar_count + 1> available_{true, true, true, true, true, true, true, true, true, true, true};
        std::array<module_text_profile, 7> text_{};
        ui::container_style container_{1, 5, 8, false};
        ui::focus_frame_style focus_style_{};
        IAshitaCore* core_{};
        action_store* store_{};
        action_executor* executor_{};
        const skillchain_source* skillchains_{};
        bool skillchain_predictions_{true}, magic_burst_settings_request_{};
        combat_settings local_combat_settings_{};
        combat_settings* combat_settings_{};
        ui::native_ui_sounds* sounds_{};
        const ui::config_text_settings* config_text_{};
        controller_crossbar_settings controller_settings_{};
        controller_status controller_status_{};
        ui::rect viewport_{};
        library_metrics library_metrics_{};
        std::array<ui::rect, 5> workspace_bounds_{{{-1, -1, 640, 480}, {-1, -1, 580, 460}, {-1, -1, 720, 548}, {-1, -1, 680, 416}, {-1, -1, 680, 580}}};
        ui::rect window_start_{}, window_original_{};
        window_gesture window_gesture_{};
        uint8_t window_resize_edges_{};
        size_t window_workspace_{};
        bool window_focused_{};
        dialog dialog_{};
        dialog name_return_{};
        address selected_{}, pressed_{};
        action_slot draft_{};
        std::string palette_id_{}, name_value_{}, import_path_{}, message_{}, query_{};
        action_import_preview import_preview_{};
        std::vector<action_slot> results_{};
        std::optional<action_slot> library_drag_{};
        std::optional<size_t> library_pressed_{}, configuration_request_{};
        ui::text_input input_{};
        ui::slider_value_input setup_value_input_{};
        ui::choice_dropdown setup_choice_input_{};
        std::vector<std::pair<int, std::string>> setup_choices_{};
        std::vector<const char*> setup_choice_labels_{};
        int setup_control_{}, setup_choice_index_{}, setup_slider_drag_{}, setup_slider_adjust_{};
        std::optional<int> setup_choice_pressed_{};
        float setup_value_{};
        size_t view_{}, result_scroll_{}, palette_scroll_{}, line_scroll_{}, focused_result_{};
        int selected_bank_{}, selected_text_{}, selected_bar_{}, storage_scope_{}, category_{}, target_{}, editor_kind_{}, editor_equipment_{}, prompt_theme_{}, shape_{};
        int focused_control_{}, editing_field_{}, name_action_{}, setup_step_{}, setup_backend_{}, setup_preset_{}, pressed_control_{}, down_header_{};
        bool storage_pet_{}, browsing_{}, navigation_focus_{}, pointer_down_{}, dragging_{}, discard_pointer_release_{};
        bool settings_changed_{}, binding_capture_{}, inactive_{true}, prompts_{true}, allow_native_keys_{true}, only_learned_{true}, draft_pet_{}, scrolling_{};
        bool placing_{}, crossbar_preview_{}, trigger_controls_{};
        bool crossbar_animations_{true};
        float scroll_grab_{};
        float hover_x_{-1}, hover_y_{-1}, down_x_{}, down_y_{};
        uint64_t now_{}, last_catalog_refresh_{}, store_revision_{}, message_until_{};
        crossbar_bank active_bank_{};
        uint8_t triggers_{};
        uint8_t native_macro_modifiers_{};
        std::array<bool, 256> owned_keys_{};
        std::unordered_map<uint32_t, inspection> inspected_{};
        std::unordered_map<uint32_t, uint64_t> flashes_{};
        std::array<std::array<bool, action_crossbar_slots>, action_crossbar_banks> held_slots_{};
        std::array<crossbar_visuals::press_motion, 2> trigger_motion_{};
        std::array<std::array<crossbar_visuals::press_motion, action_crossbar_slots>, action_crossbar_banks> button_motion_{};
        std::array<crossbar_visuals::bank_motion, 2> bank_motion_{};
        auto Surface(void) const -> action_surface { return this->view_ == action_bar_count ? action_surface::crossbar : action_surface::hotbar; }
        auto CombatSettings(void) -> combat_settings& { return this->combat_settings_ != nullptr ? *this->combat_settings_ : this->local_combat_settings_; }
        auto CombatSettings(void) const -> const combat_settings& { return this->combat_settings_ != nullptr ? *this->combat_settings_ : this->local_combat_settings_; }
        auto ConfigBar(void) const -> size_t { return this->view_ == action_bar_count ? static_cast<size_t>(this->selected_bank_) : this->view_; }
        auto Presentation(const address& value) -> actionbar_presentation& { return this->bars_[value.surface == action_surface::crossbar ? action_bar_count : value.bar]; }
        auto Presentation(const address& value) const -> const actionbar_presentation& { return this->bars_[value.surface == action_surface::crossbar ? action_bar_count : value.bar]; }
        auto Layout(size_t) -> void;
        auto SlotBounds(const address&) const -> ui::rect;
        auto BarNumberBounds(size_t) const -> ui::rect;
        auto LockBounds(size_t) const -> ui::rect;
        auto MenuBounds(size_t) const -> ui::rect;
        auto PaletteBounds(size_t) const -> ui::rect;
        auto CrossbarGroups(void) const -> std::vector<size_t>;
        auto HitTest(float, float) const -> std::optional<hit>;
        auto Activate(const address&) -> bool;
        auto Inspect(const address&) -> const action_availability&;
        auto DrawSlot(const address&, const ui::rect&, bool active, const module_render_context&, float opacity = 1, float yOffset = 0, bool outgoing = false, int inactive = -1, slot_pass pass = slot_pass::all) -> void;
        auto DrawTooltip(const address&, const ui::rect&, const module_render_context&) -> void;
        auto DialogBounds(void) const -> ui::rect;
        static auto IsWindowDialog(dialog) -> bool;
        auto WindowDialog(void) const -> dialog;
        auto WindowIndex(void) const -> size_t;
        auto WindowTitleBounds(void) const -> ui::rect;
        auto WindowCloseBounds(void) const -> ui::rect;
        auto WindowResizeBounds(void) const -> ui::rect;
        auto WindowMinimum(size_t) const -> ui::point;
        auto BeginWindowGesture(window_gesture, uint8_t edges = 0) -> void;
        auto AdjustWindow(float x, float y) -> void;
        auto FinishWindowGesture(bool) -> void;
        auto AddWindowControls(std::vector<control>&) const -> void;
        auto HandleWindowControl(int) -> bool;
        auto DrawWindowChrome(const module_render_context&) const -> void;
        auto PaletteRows(void) const -> size_t;
        auto Controls(void) const -> std::vector<control>;
        auto SetupControls(const ui::rect&) const -> std::vector<control>;
        auto KeybindControls(const ui::rect&) const -> std::vector<control>;
        auto KeybindRows(void) const -> size_t;
        auto ActivateKeybindControl(int, int) -> bool;
        auto MoveKeybindControl(int, int) -> bool;
        auto KeybindMappingLabel(size_t slot) const -> std::string;
        auto ActivateSetupControl(int, int) -> bool;
        auto SetupSliderOption(const control&, float&) const -> module_option;
        auto SetSetupSlider(int, float) -> void;
        auto OpenSetupValue(const control&) -> void;
        auto FinishSetupValue(bool apply, bool keepInvalid = false) -> bool;
        auto OpenSetupChoice(const control&) -> void;
        auto SelectSetupChoice(int) -> void;
        auto CancelSetupControls(void) -> void;
        auto SetupViewport(void) const -> D3DVIEWPORT8;
        auto HandleSetupMouse(uint32_t, WPARAM, float, float) -> bool;
        auto HandleSetupController(controller_action, bool repeated) -> bool;
        auto DrawSetupChoice(const module_render_context&) const -> void;
        auto Scrollbar(void) const -> ui::config_scrollbar;
        auto SetScroll(int) -> void;
        auto ActivateControl(int, int adjustment = 1) -> void;
        auto MoveControl(int, int) -> void;
        auto CloseDialog(void) -> void;
        auto BeginField(int) -> void;
        auto CommitField(void) -> void;
        auto FieldValue(int) const -> std::string;
        auto SetFieldValue(int, std::string) -> void;
        auto OpenPicker(void) -> void;
        auto OpenSlotMenu(const address&) -> void;
        auto OpenBarMenu(const address&) -> void;
        auto PrepareAction(action_slot&) const -> void;
        auto AssignAction(const address&, action_slot) -> bool;
        auto MoveAssignment(controller_action) -> void;
        auto CycleShape(size_t) -> void;
        auto PickerRows(void) const -> size_t;
        auto LibraryLayout(void) const -> library_layout;
        auto UpdateLibraryMetrics(const ui::design_system&) -> void;
        auto DrawLibraryIcon(const action_slot&, const ui::rect&, const module_render_context&) const -> void;
        auto RebuildResults(void) -> void;
        auto ChooseResult(size_t) -> void;
        auto NewDraft(action_kind) -> void;
        auto SaveDraft(void) -> void;
        auto NamePalette(int) -> void;
        auto PreviewImport(void) -> void;
        auto Notice(std::string) -> void;
        auto SyncStorage(void) -> void;
        auto Binding(const address&) const -> actionbar_binding;
        static auto BindingLabel(actionbar_binding) -> std::string;
        auto Mapping(void) -> controller_crossbar_mapping&;
        auto Mapping(void) const -> const controller_crossbar_mapping&;
    };

    class actionbar_module final : public hud_module
    {
    public:
        actionbar_module(actionbars_module& owner, size_t index) : owner_(owner), index_(index) {}
        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override { return this->owner_.BarState(this->index_); }
        auto State(void) const -> const module_state& override { return this->owner_.BarState(this->index_); }
        auto ResetDefaults(const ui::theme&) -> void override {}
        auto Load(const settings_store&) -> void override {}
        auto Save(const settings_store&) const -> void override {}
        auto OptionCount(void) const -> size_t override { return 0; }
        auto Option(size_t) -> module_option override { return {}; }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->owner_.BarVisible(this->index_); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override { this->owner_.RenderBar(this->index_, context); }
    private:
        actionbars_module& owner_;
        size_t index_{};
    };
}
