#pragma once

#include "modules/module.hpp"
#include "localization/localization.hpp"

namespace xiui
{
    class native_window_module final : public hud_module
    {
    public:
        auto Initialize(size_t index) -> void
        {
            this->index_ = index;
            this->id_ = std::string{"native."} + (index < native_hud_window_count ? native_hud_options[index].key : native_menu_windows[index - native_hud_window_count].key);
        }
        auto Id(void) const -> const char* override { return this->id_.c_str(); }
        auto DisplayName(void) const -> const char* override { return this->index_ < native_hud_window_count ? native_hud_options[this->index_].label : native_menu_windows[this->index_ - native_hud_window_count].label; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SupportsScale(void) const -> bool override { return false; }
        auto ConfigScaleEditable(void) const -> bool override { return false; }
        auto EditGrip(void) const -> edit_grip_kind override { return edit_grip_kind::none; }
        auto CanMove(void) const -> bool override { return this->movable_ && (this->observed_ || this->custom_); }
        auto PreviewVisible(void) const -> bool override { return this->OnScreen(); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->OnScreen(); }
        auto OnScreen(void) const -> bool { return this->editing_ && this->native_visible_; }
        auto HeaderWidth(void) const -> float { return std::max(160.0f, this->state_.bounds.width); }
        auto SetEditing(bool editing) -> void { this->editing_ = editing; }
        auto ResetDefaults(const ui::theme&) -> void override { this->custom_ = this->hidden_ = false; this->visibility_edited_ = this->preferences_known_ = true; }
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return 5; }
        auto ContentOption(size_t index) -> module_option override
        {
            if (index == 0)
            {
                auto option = module_option::Toggle("Use custom position", !this->observed_ ? "Open this native window in the game to capture its actual position." : this->movable_ ? "Move this native window independently. Turn off to restore its normal placement." :
                    "Position controls are unavailable for this native element. You can still change its visibility.", &this->custom_).Ordered(0);
                option.available = this->movable_ && this->observed_;
                return option;
            }
            if (index == 1) return module_option::Toggle("Hide window", "Hide this native dialog's artwork. Its game input remains active; show it again before interacting with it.", &this->hidden_).Ordered(1).VisibleWhen(this->index_ >= native_hud_window_count);
            if (index == 2 || index == 3)
            {
                auto option = module_option::Slider(index == 2 ? "Position X" : "Position Y", !this->observed_ ? "Open this native window in the game to capture its actual position." : this->movable_ ? "Native window position in screen pixels." :
                    "Position controls are unavailable for this native element. You can still change its visibility.", index == 2 ? &this->state_.bounds.x : &this->state_.bounds.y, 0, 8192, 1).Ordered(static_cast<int>(index), index == 2);
                option.available = this->movable_ && this->observed_;
                return option;
            }
            return module_option::Action("Restore native position", "Clear this window's custom position. Visibility is kept.", "Reset position").Ordered(4);
        }
        auto OnContentOptionChanged(size_t index) -> void override
        { this->preferences_known_ = true; if (index == 4) this->custom_ = false; else if (index == 2 || index == 3) this->custom_ = true; else if (index == 1) this->visibility_edited_ = true; }
        auto OnPositionChanged(void) -> void override { this->custom_ = this->preferences_known_ = true; }
        auto Placement(void) const -> native_window_placement { return {this->custom_, this->hidden_, this->state_.bounds.x, this->state_.bounds.y}; }
        auto Hidden(void) const -> bool { return this->hidden_; }
        auto SetHidden(bool hidden) -> void { this->hidden_ = hidden; this->visibility_edited_ = this->preferences_known_ = true; }
        auto TakeVisibilityEdit(void) -> bool { const bool edited = this->visibility_edited_; this->visibility_edited_ = false; return edited; }
        auto PositionKnown(void) const -> bool { return this->observed_ || this->custom_; }
        auto HasGeometry(void) const -> bool { return this->observed_; }
        auto Update(const native_window_state& window, float scaleX, float scaleY, float width, float height, bool moving = false) -> void
        {
            auto& bounds = this->state_.bounds;
            this->movable_ = window.movable;
            this->native_visible_ = window.live && window.visible;
            if (window.live)
            {
                this->native_bounds_ = {window.x * scaleX, window.y * scaleY, window.width * scaleX, window.height * scaleY};
                if (!this->custom_ && !moving) { bounds.x = this->native_bounds_.x; bounds.y = this->native_bounds_.y; }
                bounds.width = this->native_bounds_.width; bounds.height = this->native_bounds_.height;
                this->movable_ = window.movable; this->observed_ = true;
            }
            else if (this->observed_)
            {
                if (!this->custom_ && !moving) { bounds.x = this->native_bounds_.x; bounds.y = this->native_bounds_.y; }
                bounds.width = this->native_bounds_.width; bounds.height = this->native_bounds_.height;
            }
            else
            {
                bounds.width = bounds.height = 0;
                if (!this->custom_ && !moving) bounds.x = bounds.y = 0;
            }
            if (this->custom_ || moving)
            {
                bounds.x = std::clamp(bounds.x, 0.0f, std::max(0.0f, width - bounds.width));
                bounds.y = std::clamp(bounds.y, 0.0f, std::max(0.0f, height - bounds.height));
            }
        }
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
    private:
        size_t index_{};
        std::string id_{};
        module_state state_{true, {}, 32, 24};
        ui::rect native_bounds_{};
        bool preferences_known_{};
        bool custom_{}, hidden_{}, movable_{}, observed_{}, editing_{};
        bool native_visible_{}, visibility_edited_{};
    };
}
