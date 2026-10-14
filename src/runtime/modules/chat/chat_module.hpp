#pragma once

#include "modules/module.hpp"
#include "chat/chat_editor.hpp"
#include "chat/chat_model.hpp"
#include "chat/chat_history.hpp"
#include "chat/chat_alerts.hpp"
#include "chat/chat_colors.hpp"
#include "chat/native_chat_input.hpp"
#include "ui/native_ui_sounds.hpp"
#include <unordered_map>

namespace xiui
{
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
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return this->ColorsOptionStart() + std::size(chat_types); }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnOptionChanged(size_t index) -> void override;
        auto PreviewChoice(size_t index, size_t choice) -> bool override;
        auto HasEditableBounds(void) const -> bool override { return !this->secondary_ || this->WindowVisible(); }
        auto WindowVisible(void) const -> bool { return this->state_.enabled && (this->parent_ == nullptr || this->parent_->state_.enabled); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->WindowVisible(); }
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Receive(IAshitaCore* core, const chat_event& event, bool audible = true) -> void;
        auto HandleKey(IAshitaCore* core, WPARAM key, LPARAM flags, bool blocked, bool replaceNative, std::string_view replyTarget = {}) -> bool;
        auto OpenTell(std::string_view recipient) -> bool;
        auto AdoptDraft(chat_draft draft) -> bool;
        auto AdoptTell(chat_draft draft, std::string_view recipient = {}) -> bool;
        auto LoadCompletions(IAshitaCore* core) -> void;
        auto DescribeCompletions(IAshitaCore* core, std::string_view prefix) -> std::string;
        auto DescribeColors(const chat_palette& palette) const -> std::string;
        auto HandleMouse(uint32_t message, WPARAM wParam, float x, float y, bool editing, const ui::design_system& design, const ui::rect& viewport, bool& save) -> bool;
        auto Focus(bool focused) -> void;
        auto Focused(void) const -> bool { return this->focused_ && this->state_.enabled; }
        auto Resizing(void) const -> bool { return this->resizing_; }
        auto CancelPointer(void) -> void { this->resizing_ = this->pointer_down_ = false; }
        auto Capture(runtime_state& state) const -> void;
        auto Restore(const runtime_state& state) -> void;
        auto Count(void) const -> size_t { return this->History().Lines().size(); }
        auto AttachSecondWindow(chat_module& second) -> void;
        auto EnsureWindowDefaults(void) -> void;
        auto ViewName(void) const -> const char* { return chat_tab_names[this->active_tab_]; }
        auto TabsVisible(void) const -> bool { return this->visible_tabs_[0] || this->visible_tabs_[1]; }
        auto Clear(void) -> void;
        auto Grip(void) const -> ui::rect;
        auto VisualBounds(const ui::design_system& design) const -> ui::rect;
        auto HitTest(const ui::design_system& design, float x, float y) const -> bool;
        auto SoundTarget(const ui::design_system& design, float x, float y, bool editing) const -> uint32_t;
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->sounds_ = sounds; }
        auto SetHovered(bool hovered) -> void { this->hovered_ = hovered; }
        auto SetNativeTimestamps(std::optional<uint8_t> mode) -> void { this->native_timestamp_mode_ = mode.value_or(0); }
        auto HistoryFocused(void) const -> bool { return this->history_view_.focused && this->WindowVisible(); }
        auto HistoryExpanded(void) const -> bool { return this->HistoryFocused() && this->history_view_.expanded; }
        auto SetHistoryViewport(const ui::rect& viewport) -> void { this->history_view_.viewport = viewport; }
        auto HandleHistoryKey(WPARAM key, LPARAM flags, bool blocked, bool replaceNative, const ui::design_system& design) -> bool;
        auto RenderHistory(const hud_snapshot& snapshot, const module_render_context& context) -> void;

    private:
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
        struct tab_state { size_t scroll{}; uint32_t unread{}; };
        struct rendered_span { std::string text; int color{-1}; float width{}; };
        struct rendered_row { uint64_t sequence{}; uint32_t mode{}, color{}; std::vector<rendered_span> spans; };
        struct layout { ui::rect content, log, input, channel, scrollbar, completion; size_t completion_rows{}; float completion_row_height{}; std::array<ui::rect, static_cast<size_t>(chat_tab::count)> tabs; };
        struct history_layout { ui::rect frame, log, scrollbar, footer; std::array<ui::rect, std::size(chat_history_tabs)> tabs; };
        module_state state_{true, {18.0f, -1.0f, 560.0f, 260.0f}, 380.0f, 160.0f};
        ui::container_style container_{};
        module_text_profile text_{};
        module_text_profile chrome_{};
        chat_model model_{};
        chat_editor editor_{};
        ui::native_ui_sounds* sounds_{};
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
        int channel_{};
        bool focused_{}, resizing_{}, pointer_down_{};
        bool hovered_{};
        float input_reveal_{};
        uint64_t input_animation_tick_{};
        uint64_t scrollbar_until_{};
        int timestamp_display_{};
        uint8_t native_timestamp_mode_{};
        chat_history_view history_view_{};
        size_t visible_rows_{1};
        float line_spacing_{2.0f};
        float resize_x_{}, resize_y_{};
        ui::rect resize_bounds_{};
        std::vector<rendered_row> rows_{};
        uint64_t cache_revision_{UINT64_MAX};
        float cache_width_{}, cache_scale_{}, cache_spacing_{};
        int cache_font_{-1};
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
        auto Alerts(void) -> chat_alerts& { return this->parent_ != nullptr ? this->parent_->alerts_ : this->alerts_; }
        auto Colors(void) -> chat_color_overrides& { return this->parent_ != nullptr ? this->parent_->colors_ : this->colors_; }
        auto Colors(void) const -> const chat_color_overrides& { return this->parent_ != nullptr ? this->parent_->colors_ : this->colors_; }
        auto History(void) const -> const chat_model& { return this->shared_model_ != nullptr ? *this->shared_model_ : this->model_; }
        auto MarkUnread(const chat_event& event, uint32_t previousWindows) -> void;
        auto Layout(const ui::design_system& design) const -> layout;
        auto ChannelProfile(void) const -> module_text_profile;
        auto UpdateInputAnimation(uint64_t now) -> void;
        auto ScrollbarOpacity(uint64_t now) const -> float;
        auto RebuildRows(const ui::design_system& design, float width) -> void;
        auto HistoryLayout(const ui::design_system& design) const -> history_layout;
        auto SelectHistoryTab(size_t index) -> void;
        auto ScrollMessage(int direction) -> void;
        auto CurrentScroll(void) -> size_t& { return this->HistoryExpanded() ? this->history_view_.scroll[this->history_view_.tab] : this->tabs_[this->active_tab_].scroll; }
        auto RowFilter(void) const -> int { return this->HistoryExpanded() ? static_cast<int>(100 + this->history_view_.tab) : this->active_tab_; }
        auto DrawLog(const hud_snapshot& snapshot, const module_render_context& context, const ui::rect& log, const ui::rect& scrollbar) -> void;
        auto HandleHistoryMouse(uint32_t message, WPARAM wParam, float x, float y, const ui::design_system& design) -> bool;
        auto HistorySoundTarget(const ui::design_system& design, float x, float y) const -> uint32_t;
        auto TimestampMode(void) const -> int { return this->timestamp_display_ == 0 ? this->native_timestamp_mode_ : this->timestamp_display_ - 1; }
        auto Candidates(IAshitaCore* core) -> std::vector<chat_completion>;
        auto DisplayDraft(std::string_view text) const -> std::string;
        auto DrawCompletions(const layout& layout, const module_render_context& context) const -> void;
        auto Submit(IChatManager* chat) -> bool;
        auto Copy(HWND window, bool cut) -> void;
        auto Paste(HWND window) -> void;
        auto Scroll(int amount) -> void;
    };
}
