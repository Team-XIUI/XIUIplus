#pragma once

#include "modules/module.hpp"
#include "chat/chat_editor.hpp"
#include "chat/chat_model.hpp"
#include "chat/chat_history.hpp"
#include "chat/chat_alerts.hpp"
#include "chat/chat_colors.hpp"
#include "chat/native_chat_input.hpp"
#include "chat/chat_destination.hpp"
#include "chat/native_chat_filters.hpp"
#include "ui/native_ui_sounds.hpp"
#include "ui/native_window_config.hpp"
#include "controller_input.hpp"
#include <unordered_map>

namespace xiui
{
    enum class chat_window_layout : uint8_t { follow_game, combined };
    class chat_module final : public hud_module
    {
    public:
        explicit chat_module(bool secondary = false);
        auto Id(void) const -> const char* override { return this->secondary_ ? "chat2" : "chat"; }
        auto DisplayName(void) const -> const char* override { return this->secondary_ ? "Chat 2" : "Chat"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->text_.style, &this->chrome_.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto LoadScale(const settings_store& store) -> void override;
        auto SupportsNonUniformScale(void) const -> bool override { return false; }
        auto EditGrip(void) const -> edit_grip_kind override { return this->MatchingWindow1() ? edit_grip_kind::none : edit_grip_kind::resize; }
        auto EditGripBounds(const ui::design_system& design) const -> std::optional<ui::rect> override
        { const auto scope = design.Transform(this->state_.transform); return this->state_.transform.Map(this->Grip(design)); }
        auto OnBoundsResized(const ui::rect& original) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return this->ContainerOptionStart() + 2; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto PreviewChoice(size_t index, size_t choice) -> bool override;
        auto HasEditableBounds(void) const -> bool override { return !this->secondary_ || this->WindowVisible(); }
        auto WindowVisible(void) const -> bool
        { return this->state_.enabled && (this->parent_ == nullptr || this->parent_->state_.enabled) &&
            (!this->secondary_ || this->native_filters_ == nullptr || this->native_filters_->WindowHasRouting(true) != false); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->WindowVisible(); }
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Receive(IAshitaCore* core, const chat_event& event, bool audible = true) -> void;
        auto ObserveTellPacket(uint16_t id, std::span<const uint8_t> packet) -> void;
        auto SetInputContext(IChatManager* chat) -> void;
        auto SetNativeDestination(std::optional<std::string> prefix, std::string reply) -> void
        { this->native_send_prefix_ = std::move(prefix); this->native_reply_recipient_ = std::move(reply); }
        auto SendChannelLabel(void) const -> std::string { return this->InputDestination().label; }
        auto SetDialogueActive(bool active) -> void;
        auto HandleKey(IAshitaCore* core, WPARAM key, LPARAM flags, bool blocked, bool replaceNative, std::span<const std::string> replyTargets = {}, bool startTyping = false) -> bool;
        auto OpenTell(std::string_view recipient) -> bool;
        auto AdoptDraft(chat_draft draft) -> bool;
        auto AdoptTell(chat_draft draft, std::string_view recipient = {}) -> bool;
        auto LoadCompletions(IAshitaCore* core) -> void;
        auto DescribeCompletions(IAshitaCore* core, std::string_view prefix) -> std::string;
        auto DescribeColors(const chat_palette& palette) const -> std::string;
        auto LayoutHandleMouse(uint32_t message, WPARAM wParam, float x, float y, bool editing, const ui::design_system& design, const ui::rect& viewport, bool& save, IChatManager* chat) -> bool;
        auto HandleMouse(uint32_t message, WPARAM keys, float x, float y, bool editing, const ui::design_system& design, const ui::rect& viewport, bool& save, IChatManager* chat = nullptr) -> bool
        { const auto scope = design.Transform(this->state_.transform); return this->LayoutHandleMouse(message, keys, this->state_.transform.InverseX(x), this->state_.transform.InverseY(y), editing, design, this->state_.transform.Inverse(viewport), save, chat); }
        auto Focus(bool focused, bool preserveExpandedHistory = false) -> void;
        auto Focused(void) const -> bool { return this->focused_ && this->state_.enabled; }
        auto Resizing(void) const -> bool { return this->resizing_; }
        auto CancelPointer(void) -> void
        { this->resizing_ = this->pointer_down_ = this->channel_picker_open_ = this->destination_hovered_ = false; this->channel_pointer_press_ = -2; }
        auto ChannelPickerOpen(void) const -> bool { return this->channel_picker_open_; }
        auto RenderChannelPicker(const module_render_context& context) const -> void;
        auto Capture(runtime_state& state) const -> void;
        auto Restore(const runtime_state& state) -> void;
        auto Count(void) const -> size_t { return this->History().Lines().size(); }
        auto CapturedLine(uint64_t sequence) const -> const chat_line*
        { const auto& lines = this->History().Lines(); return !lines.empty() && lines.back().sequence == sequence ? &lines.back() : nullptr; }
        auto ColorOverrides(void) const -> const chat_color_overrides& { return this->Colors(); }
        auto AttachSecondWindow(chat_module& second) -> void;
        auto InitializeNativeWindows(std::optional<unsigned> count) -> bool;
        auto WindowLayout(void) const -> std::optional<chat_window_layout>;
        auto SetWindowLayout(chat_window_layout layout, std::optional<unsigned> nativeCount) -> void;
        auto EnsureWindowDefaults(void) -> void;
        auto ViewName(void) const -> const char* { return chat_tab_names[this->active_tab_]; }
        auto TabsVisible(void) const -> bool { return this->visible_tabs_[0] || this->visible_tabs_[1]; }
        auto Clear(void) -> void;
        auto Grip(const ui::design_system& design) const -> ui::rect;
        auto CompactBounds(const ui::design_system& design) const -> ui::rect;
        auto FrameBounds(const ui::design_system& design) const -> ui::rect;
        auto UpdateCompactLayout(const ui::design_system& design, uint64_t now) -> void;
        auto SetLayoutEditing(bool editing) -> void { this->layout_editing_ = editing; }
        auto SetStackPeerVisible(bool visible) -> void { this->stack_peer_visible_ = visible; }
        auto SetNativeWindowConfig(std::optional<native_chat_window_settings> settings) -> void
        {
            if (this->native_window_settings_ == settings) return;
            this->native_window_settings_ = settings;
            if (this->collapse_mode_ == 0) this->ResetCollapse();
            if (this->second_window_ != nullptr && this->second_window_->MatchingWindow1()) this->second_window_->ResetCollapse();
        }
        auto VisualBounds(const ui::design_system& design) const -> ui::rect;
        auto LayoutHitTest(const ui::design_system& design, float x, float y) const -> bool;
        auto HitTest(const ui::design_system& design, float x, float y) const -> bool
        { return this->LayoutHitTest(design, this->state_.transform.InverseX(x), this->state_.transform.InverseY(y)); }
        auto LayoutSoundTarget(const ui::design_system& design, float x, float y, bool editing) const -> uint32_t;
        auto SoundTarget(const ui::design_system& design, float x, float y, bool editing) const -> uint32_t
        { return this->LayoutSoundTarget(design, this->state_.transform.InverseX(x), this->state_.transform.InverseY(y), editing); }
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->sounds_ = sounds; }
        auto SetHovered(bool hovered, float x = 0, float y = 0) -> void
        { this->hovered_ = hovered; if (!hovered) this->destination_hovered_ = false; this->hover_point_ = {this->state_.transform.InverseX(x), this->state_.transform.InverseY(y)}; }
        auto SetNativeTimestamps(std::optional<uint8_t> mode) -> void { this->native_timestamp_mode_ = mode.value_or(0); }
        auto SetNativeFilters(native_chat_filters& filters) -> void { this->native_filters_ = &filters; }
        auto HistoryFocused(void) const -> bool { return this->history_view_.focused && this->WindowVisible(); }
        auto HistoryExpanded(void) const -> bool { return this->HistoryFocused() && this->history_view_.expanded; }
        auto ControllerBounds(const ui::design_system& design) const -> ui::rect { return this->state_.transform.Map(this->HistoryExpanded() ? this->HistoryLayout(design).frame : this->VisualBounds(design)); }
        auto SetHistoryViewport(const ui::rect& viewport) -> void { this->history_view_.viewport = this->state_.transform.Inverse(viewport); }
        auto HandleHistoryKey(WPARAM key, LPARAM flags, bool blocked, bool replaceNative, const ui::design_system& design) -> bool;
        auto BeginHistory(bool controller = false, bool playSound = true) -> bool;
        auto BeginControllerHistory(bool playSound = true) -> bool { return this->BeginHistory(true, playSound); }
        auto BeginControllerCompose(void) -> bool;
        auto ControllerComposing(void) const -> bool { return this->controller_composition_ && this->Focused(); }
        auto HandleControllerAction(controller_action action, bool repeated, const ui::design_system& design, IChatManager* chat = nullptr) -> bool;
        auto SetControllerHistoryPrompts(std::string compact, std::string expanded) -> void;
        auto RenderHistory(const hud_snapshot& snapshot, const module_render_context& context) -> void;

    private:
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct controller_module_test;
        friend struct config_input_test;
#endif
        struct tab_state { size_t scroll{}; uint32_t unread{}; };
        struct rendered_span { std::string text; int color{-1}; float width{}; bool timestamp{}, dialogue_prompt{}, emphasis{}, name{}; };
        struct rendered_row { uint64_t sequence{}; uint32_t mode{}, color{}; std::vector<rendered_span> spans; };
        struct message_entrance { uint64_t sequence{}, started{}; };
        struct message_motion { float opacity{1}, x{}; };
        struct layout { ui::rect content, log, input, input_text, scrollbar, completion, counter, destination; size_t completion_rows{}; float completion_row_height{}; std::array<ui::rect, static_cast<size_t>(chat_tab::count)> tabs; std::array<ui::rect, 3> controller_actions; };
        struct history_layout { ui::rect frame, log, scrollbar, footer; std::array<ui::rect, std::size(chat_history_tabs)> tabs; };
        struct channel_picker_layout { ui::rect bounds; std::array<ui::rect, 10> rows; };
        struct window_settings
        {
            float width{}, height{};
            ui::container_style container{};
            module_text_profile text{}, chrome{};
            float line_spacing{};
            int message_animation{};
            float message_animation_speed{};
            int collapse_mode{};
            float collapse_delay{}, collapse_min_lines{}, collapse_max_lines{};
            bool height_limits_lines{};
            int timestamp_display{};
            bool timestamp_color_enabled{};
            D3DCOLOR timestamp_color{};
            bool background{true};
        };
        module_state state_{true, {18.0f, -1.0f, 560.0f, 260.0f}, 380.0f, 64.0f};
        ui::container_style container_{};
        bool background_{true};
        module_text_profile text_{};
        module_text_profile chrome_{};
        chat_model model_{};
        chat_editor editor_{};
        std::vector<std::string> recent_tellers_{};
        std::string reply_recipient_{};
        std::string reply_destination_{};
        ui::native_ui_sounds* sounds_{};
        native_chat_filters* native_filters_{};
        chat_alerts alerts_{};
        chat_color_overrides colors_{};
        std::array<tab_state, static_cast<size_t>(chat_tab::count)> tabs_{};
        int active_tab_{static_cast<int>(chat_tab::window1)};
        int source_{};
        std::array<bool, 2> visible_tabs_{};
        bool secondary_{};
        chat_module* second_window_{};
        const chat_model* shared_model_{};
        chat_module* parent_{};
        bool defaults_initialized_{};
        bool native_windows_initialized_{true};
        bool match_window1_{};
        std::optional<window_settings> independent_settings_{};
        float matched_width_{}, matched_height_{};
        bool shared_windows_{}, stack_peer_visible_{true};
        static constexpr int game_channel = 9;
        int channel_{game_channel};
        bool focused_{}, resizing_{}, pointer_down_{};
        bool dialogue_active_{};
        bool hovered_{};
        ui::point hover_point_{};
        float input_reveal_{};
        float destination_reveal_{};
        bool destination_hovered_{}, channel_picker_open_{};
        int channel_pointer_press_{-2};
        ui::rect channel_picker_viewport_{0, 0, 1280, 720};
        uint64_t input_animation_tick_{};
        int message_animation_{1};
        float message_animation_speed_{100};
        std::deque<message_entrance> message_entrances_{};
        int collapse_mode_{};
        float collapse_delay_{5}, collapse_min_lines_{}, collapse_max_lines_{12};
        bool height_limits_lines_{};
        std::optional<native_chat_window_settings> native_window_settings_{};
        bool layout_editing_{}, collapse_active_{};
        uint64_t collapse_due_{}, collapsed_through_{}, collapse_fade_sequence_{};
        size_t collapse_begin_{}, collapse_visible_rows_{}, collapse_fade_end_{};
        float collapse_height_rows_{}, collapse_opacity_{1};
        uint64_t scrollbar_until_{};
        bool input_counter_{true};
        uint32_t input_limit_{255};
        IChatManager* input_chat_{};
        std::optional<std::string> native_send_prefix_{};
        std::string native_reply_recipient_{};
        auto InputDestination(void) const -> chat_destination;
        auto InputCommand(std::string_view text) const -> std::string;
        auto InputUsage(std::string_view text) const -> std::pair<size_t, size_t>;
        int timestamp_display_{};
        bool timestamp_color_enabled_{true};
        D3DCOLOR timestamp_color_{0xFFB0B0B0};
        uint8_t native_timestamp_mode_{};
        chat_history_view history_view_{};
        bool controller_history_{};
        bool controller_composition_{}, controller_channel_adjusting_{};
        size_t controller_compose_action_{};
        chat_module* controller_return_history_{};
        std::string controller_compact_prompt_{"Cycle: next window   Confirm: select chat   Cancel: back   Actions: compose"};
        std::string controller_expanded_prompt_{"Cycle window: next tab   Left/Right: page   Up/Down: message   Confirm: compact   Cancel: back"};
        size_t visible_rows_{1};
        float line_spacing_{2.0f};
        float resize_x_{}, resize_y_{};
        ui::rect resize_bounds_{};
        std::vector<rendered_row> rows_{};
        uint64_t cache_revision_{UINT64_MAX};
        float cache_width_{}, cache_scale_{}, cache_spacing_{}, cache_hud_scale_{};
        int cache_font_{-1};
        bool cache_xiv1_{};
        int cache_tab_{-1};
        int cache_timestamp_mode_{-1};
        uint64_t cache_last_sequence_{};
        size_t input_start_{};
        std::string error_{};
        std::vector<chat_completion> resource_completions_{};
        std::unordered_map<std::string, std::string> auto_translate_labels_{};
        IResourceManager* completion_resources_{};
        auto SoundsOptionStart(void) const -> size_t { return (this->secondary_ ? 10 : 12) + text_profile_option_count; }
        auto ColorsOptionStart(void) const -> size_t { return this->SoundsOptionStart() + 2 + std::size(chat_types); }
        auto TimestampColorOptionStart(void) const -> size_t { return this->ColorsOptionStart() + std::size(chat_types); }
        auto NativeFiltersStart(void) const -> size_t { return this->TimestampColorOptionStart() + 2; }
        auto AnimationOptionStart(void) const -> size_t { return this->NativeFiltersStart() + (this->native_filters_ != nullptr ? native_chat_filters::option_count : 0); }
        auto CollapseOptionStart(void) const -> size_t { return this->AnimationOptionStart() + 3; }
        auto ContainerOptionStart(void) const -> size_t { return this->CollapseOptionStart() + 6; }
        auto MatchWindow1Option(void) const -> size_t { return this->CollapseOptionStart() + 5; }
        auto MatchingWindow1(void) const -> bool { return this->secondary_ && this->match_window1_ && this->parent_ != nullptr; }
        auto InheritedWindowOption(size_t index) const -> bool;
        auto WindowOption(size_t index) -> module_option;
        auto CaptureWindowSettings(void) const -> window_settings;
        auto ApplyWindowSettings(const window_settings& settings) -> void;
        auto CollapseSettings(void) const -> std::optional<native_chat_window_settings>;
        auto CollapsePinned(void) const -> bool;
        auto ResetCollapse(void) -> void;
        auto TrackCollapse(const chat_event& event, uint32_t previousWindows, uint64_t now) -> void;
        auto SpanColor(const rendered_row& row, const rendered_span& span, const chat_palette& palette, bool xiv1 = false) const -> D3DCOLOR;
        auto Alerts(void) -> chat_alerts& { return this->parent_ != nullptr ? this->parent_->alerts_ : this->alerts_; }
        auto Colors(void) -> chat_color_overrides& { return this->parent_ != nullptr ? this->parent_->colors_ : this->colors_; }
        auto Colors(void) const -> const chat_color_overrides& { return this->parent_ != nullptr ? this->parent_->colors_ : this->colors_; }
        auto History(void) const -> const chat_model& { return this->shared_model_ != nullptr ? *this->shared_model_ : this->model_; }
        auto MarkUnread(const chat_event& event, uint32_t previousWindows) -> void;
        auto Layout(const ui::design_system& design) const -> layout;
        auto InputRowHeight(const ui::design_system& design) const -> float;
        auto ChannelPickerLayout(const ui::design_system& design) const -> channel_picker_layout;
        auto HandleChannelMouse(uint32_t message, float x, float y, const ui::design_system& design, bool& save) -> bool;
        auto ChooseSendChannel(int choice, bool& save) -> bool;
        auto UpdateInputAnimation(uint64_t now) -> void;
        auto TrackMessageEntrance(const chat_event& event, uint32_t previousWindows, uint64_t now) -> void;
        auto MessageMotion(uint64_t sequence, uint64_t now) const -> message_motion;
        auto MessageScrollOffset(uint64_t now, float lineHeight) const -> float;
        auto MessageAnimationDuration(void) const -> float { return 22000.0f / this->message_animation_speed_; }
        auto ScrollbarOpacity(uint64_t now) const -> float;
        auto RebuildRows(const ui::design_system& design, float width) -> void;
        auto DialoguePromptActive(uint64_t sequence) const -> bool;
        auto HistoryLayout(const ui::design_system& design) const -> history_layout;
        auto CombinedHistory(void) const -> bool
        { return this->secondary_ ? this->parent_ != nullptr && this->parent_->source_ == 2 : this->source_ == 2; }
        auto HistoryTabName(size_t index) const -> const char*
        { return index == 0 && this->CombinedHistory() ? "All" : chat_history_tabs[index]; }
        auto HistoryTabMatches(size_t index, uint32_t mode, uint32_t windows) const -> bool
        { return index == 0 && this->CombinedHistory() ? ChatTabMatches(chat_tab::all, mode, windows) : ChatHistoryMatches(index, mode, windows); }
        auto HistoryTabAvailable(size_t index) const -> bool;
        auto SelectHistoryTab(size_t index) -> void;
        auto ScrollMessage(int direction) -> void;
        auto NavigateHistory(controller_action action, bool repeated, const ui::design_system& design, bool layered) -> bool;
        auto HandleControllerCompose(controller_action action, bool repeated, IChatManager* chat) -> bool;
        auto ActivateControllerCompose(IChatManager* chat) -> bool;
        auto AdjustControllerChannel(int direction) -> void;
        auto CurrentScroll(void) -> size_t& { return this->HistoryExpanded() ? this->history_view_.scroll[this->history_view_.tab] : this->tabs_[this->active_tab_].scroll; }
        auto RowFilter(void) const -> int
        {
            if (!this->HistoryExpanded()) return this->active_tab_;
            return this->history_view_.tab == 0 && this->CombinedHistory() ? 200 : static_cast<int>(100 + this->history_view_.tab);
        }
        auto DrawLog(const hud_snapshot& snapshot, const module_render_context& context, const ui::rect& log, const ui::rect& scrollbar) -> void;
        auto DrawWindowContainer(const module_render_context& context, const ui::rect& bounds) const -> void;
        auto HandleHistoryMouse(uint32_t message, WPARAM wParam, float x, float y, const ui::design_system& design) -> bool;
        auto HistorySoundTarget(const ui::design_system& design, float x, float y) const -> uint32_t;
        auto TimestampMode(void) const -> int { return this->timestamp_display_ == 0 ? this->native_timestamp_mode_ : this->timestamp_display_ - 1; }
        auto Candidates(IAshitaCore* core) -> std::vector<chat_completion>;
        auto DisplayDraft(std::string_view text) const -> std::string;
        auto DrawCompletions(const layout& layout, const module_render_context& context) const -> void;
        auto Submit(IChatManager* chat) -> bool;
        auto OpenReply(std::span<const std::string> targets) -> bool;
        auto ObserveTeller(std::string_view name) -> void;
        auto Copy(HWND window, bool cut) -> void;
        auto Paste(HWND window) -> void;
        auto Scroll(int amount) -> void;
    };
}
