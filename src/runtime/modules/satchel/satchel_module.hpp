#pragma once

#include "satchel_model.hpp"
#include "modules/module.hpp"
#include "config/text_input.hpp"
#include "config/config_text.hpp"
#include "config/config_scrollbar.hpp"
#include "controller_input.hpp"
#include "ui/focus_frame.hpp"
#include "ui/native_ui_sounds.hpp"

#include <optional>
#include <unordered_map>

namespace xiui
{
    class satchel_source;
    class satchel_cache;
    class satchel_actions;
    enum class satchel_action : uint8_t;

    class satchel_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "satchel"; }
        auto DisplayName(void) const -> const char* override { return "Satchel"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto Initialize(IAshitaCore*, satchel_source*, satchel_cache*, satchel_actions*) -> void;
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->sounds_ = sounds; }
        auto SetFocusStyle(const ui::focus_frame_style& style) -> void { this->focus_style_ = style; }
        auto SetConfigText(const ui::config_text_settings* settings) -> void { this->config_text_ = settings; }
        auto Open(bool opened) -> void;
        auto Opened(void) const -> bool { return this->opened_; }
        auto OverridesSatchel(void) const -> bool { return this->state_.enabled && this->override_satchel_; }
        auto SuspendInput(void) -> void;
        auto BeginControllerBrowse(void) -> void;
        auto EndControllerBrowse(void) -> void;
        auto HandleControllerAction(controller_action, bool repeated) -> bool;
        auto HandleMouse(uint32_t, WPARAM, float x, float y, bool blocked) -> bool;
        auto HandleKey(WPARAM, LPARAM) -> bool;
        auto KeyboardFocused(void) const -> bool { return this->editor_.Focused(); }
        auto InputFocused(void) const -> bool { return this->controller_ || this->keyboard_ || this->editor_.Focused() || this->dialog_ != dialog::none || this->window_action_ != window_action::none; }
        auto PointerDown(void) const -> bool { return this->pointer_down_; }
        auto PointerOwns(float x, float y) const -> bool
        { const auto& b = this->state_.bounds; return this->opened_ && this->state_.enabled && (this->pointer_down_ || this->dialog_ != dialog::none || (x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height)); }
        auto SetHover(float x, float y) -> void { this->hover_x_ = x; this->hover_y_ = y; }
        auto TakeSettingsChanged(void) -> bool { const bool changed = this->settings_changed_; this->settings_changed_ = false; return changed; }
        auto Update(uint64_t now) -> void;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme&) -> void override;
        auto ResetDefaults(const ui::theme&) -> void override;
        auto Load(const settings_store&) -> void override;
        auto Save(const settings_store&) const -> void override;
        auto OptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto Option(size_t) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t, size_t) -> bool override;
        auto OnOptionChanged(size_t) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && this->opened_; }
        auto Render(const hud_snapshot&, const module_render_context&) -> void override;
        auto RenderOverlay(const module_render_context&) -> void;

    private:
#if defined(XIUI_ENABLE_SATCHEL_TESTS)
        friend struct satchel_ui_test;
#endif
        enum option : size_t { enabled, override_command, empty_slots, all_bags_search, tooltips, restricted, autosort,
            columns, rows, icon_size, spacing, sort, background, frames, opacity, padding, selected_text, base_options };
        enum class region : uint8_t { grid, bags, character, slips, search, help, minimize, move, resize, close, count };
        enum class dialog : uint8_t { none, characters, slips, actions, destinations, quantity, price, equipment, drop };
        enum class window_action : uint8_t { none, move, resize };
        enum class scroll_area : uint8_t { none, bags, items };
        struct cell { satchel_item item{}; bool empty{}; std::string key{}; };
        struct pick { std::string label{}, value{}; int id{}; bool available{true}; };
        module_state state_{true, {-1, -1, 576, 456}, 330, 200};
        ui::rect viewport_{};
        ui::rect window_start_{};
        float width_{}, height_{};
        float window_columns_{}, window_rows_{};
        window_action window_action_{};
        IAshitaCore* core_{};
        satchel_source* source_{};
        satchel_cache* cache_{};
        satchel_actions* actions_{};
        ui::native_ui_sounds* sounds_{};
        const ui::config_text_settings* config_text_{};
        ui::focus_frame_style focus_style_{};
        std::array<module_text_profile, 4> text_{};
        ui::container_style container_{1, 5, 8, false};
        bool opened_{}, override_satchel_{}, empty_slots_{true}, all_bags_search_{true}, tooltips_{true}, restricted_{}, autosort_{};
        bool background_{true}, frames_{true}, controller_{}, keyboard_{}, settings_changed_{}, pointer_down_{}, dragging_{}, discard_pointer_release_{};
        bool help_open_{}, minimized_{}, navigation_focus_{};
        int sidebar_scroll_{};
        scroll_area scrolling_{};
        float scroll_grab_{};
        float columns_{10}, rows_{5}, icon_size_{32}, spacing_{5}, hover_x_{-1}, hover_y_{-1}, down_x_{}, down_y_{};
        int sort_{}, selected_text_{}, bag_{0}, slip_{-1};
        size_t page_{}, focus_cell_{}, pick_index_{}, pick_page_{}, focused_pick_{};
        region region_{region::grid};
        dialog dialog_{};
        std::string character_{}, query_{}, message_{}, view_identity_{}, action_identity_{}, down_key_{};
        ui::text_input editor_{};
        std::vector<cell> cells_{};
        std::vector<pick> picks_{};
        std::vector<satchel_item> owned_slips_{};
        satchel_item action_item_{}, down_item_{};
        int action_{}, destination_{}, equipment_slot_{-1}, pending_autosort_bag_{-1};
        uint32_t quantity_{1}, price_{};
        uint64_t now_{}, last_click_{}, last_generation_{}, autosort_generation_{}, next_autosort_{};
        uint8_t last_click_bag_{}, last_click_slot_{};
        int last_sort_{}, last_bag_{-99}, last_slip_{-99};
        std::string last_query_{}, last_character_{};
        bool rebuild_{true};
        std::unordered_map<uint16_t, std::string> names_{};
        auto Snapshot(void) const -> const satchel_snapshot*;
        auto Live(void) const -> bool;
        auto ReadOnly(void) const -> bool;
        auto Name(uint16_t) -> const std::string&;
        auto ItemKey(const satchel_item&) const -> std::string;
        auto Rebuild(void) -> void;
        auto ReconcileFocus(const std::optional<std::string>&) -> void;
        auto ChangeBag(int) -> void;
        auto Page(int) -> void;
        auto PageSize(void) const -> size_t;
        auto Columns(void) const -> size_t;
        auto Rows(void) const -> size_t;
        auto PageCount(void) const -> size_t;
        auto CurrentCell(void) const -> const cell*;
        auto VisibleBagTabs(void) const -> std::vector<size_t>;
        auto MoveBag(int direction) -> void;
        auto ItemAt(float, float) const -> std::optional<size_t>;
        auto CellBounds(size_t) const -> ui::rect;
        auto Inner(void) const -> ui::rect;
        auto SidebarBounds(void) const -> ui::rect;
        auto GridBounds(void) const -> ui::rect;
        auto SidebarScrollbar(void) const -> ui::config_scrollbar;
        auto GridScrollbar(void) const -> ui::config_scrollbar;
        auto Scroll(scroll_area, int) -> void;
        auto EnsureSidebarFocus(void) -> void;
        auto RegionAvailable(region) const -> bool;
        auto CycleRegion(int) -> void;
        auto ToggleMinimized(void) -> void;
        auto TitleBounds(void) const -> ui::rect;
        auto MinimizeBounds(void) const -> ui::rect;
        auto ResizeBounds(void) const -> ui::rect;
        auto UpdateSize(void) -> void;
        auto BeginWindow(window_action) -> void;
        auto AdjustWindow(float, float) -> void;
        auto FinishWindow(bool) -> void;
        auto CharacterBounds(void) const -> ui::rect;
        auto SlipBounds(void) const -> ui::rect;
        auto SearchBounds(void) const -> ui::rect;
        auto BagBounds(size_t) const -> ui::rect;
        auto HelpBounds(void) const -> ui::rect;
        auto CloseBounds(void) const -> ui::rect;
        auto DialogBounds(void) const -> ui::rect;
        auto PickBounds(size_t) const -> ui::rect;
        auto FocusBounds(void) const -> ui::rect;
        auto OpenPicker(dialog) -> void;
        auto OpenActions(const satchel_item&) -> void;
        auto SelectPick(void) -> void;
        auto CancelDialog(void) -> void;
        auto StartAction(int) -> void;
        auto EnterQuantity(void) -> void;
        auto Submit(void) -> void;
        auto Transfer(const satchel_item&, int) -> void;
        auto BeginSearch(void) -> void;
        auto ReleaseBrowseFocus(void) -> void;
        auto CommitEditor(void) -> void;
        auto ActivateRegion(void) -> void;
        auto MoveGrid(int, int) -> void;
        auto PlaySelect(void) -> void;
        auto DrawTooltip(const satchel_item&, const ui::rect&, const module_render_context&) -> void;
        auto DrawDialog(const module_render_context&) -> void;
        auto DrawButton(const ui::rect&, const char*, bool, const module_render_context&) const -> void;
    };
}
