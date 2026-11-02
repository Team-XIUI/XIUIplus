#pragma once

#include "modules/module.hpp"
#include "shared/controller_input.hpp"
#include "treasure_source.hpp"
#include "ui/native_ui_sounds.hpp"
#include "ui/focus_frame.hpp"
#include <utility>

namespace xiui::ui { struct config_text_settings; }

namespace xiui
{
    class treasure_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "treasure"; }
        auto DisplayName(void) const -> const char* override { return "Treasure Pool"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(treasure_source* source) -> void { this->source_ = source; }
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->sounds_ = sounds; }
        auto SetFocusStyle(const ui::focus_frame_style& style) -> void { this->focus_style_ = style; }
        auto LayoutSoundTarget(float x, float y) const -> uint64_t;
        auto SoundTarget(float x, float y) const -> uint64_t
        { return this->LayoutSoundTarget(this->state_.transform.InverseX(x), this->state_.transform.InverseY(y)); }
        auto Update(IAshitaCore* core, bool available) -> void;
        auto Open(void) -> void { this->forced_ = true; }
        auto EndInput(void) -> void { this->browse_ = false; this->pressed_.reset(); }
        auto InputFocused(void) const -> bool { return this->browse_; }
        auto SelectedItem(void) const -> std::optional<treasure_selection>;
        auto MemberSelection(void) const -> std::optional<treasure_selection>
        { return this->member_status_ ? this->SelectedItem() : std::nullopt; }
        auto BeginBrowse(void) -> bool;
        auto HandleControllerAction(controller_action action, bool repeated) -> bool;
        auto LayoutHandleMouse(uint32_t message, float x, float y) -> bool;
        auto HandleMouse(uint32_t message, float x, float y) -> bool
        { return this->LayoutHandleMouse(message, this->state_.transform.InverseX(x), this->state_.transform.InverseY(y)); }
        auto PointerDown(void) const -> bool { return this->pointer_down_; }
        auto LayoutContains(float x, float y) const -> bool;
        auto Contains(float x, float y) const -> bool
        { return this->LayoutContains(this->state_.transform.InverseX(x), this->state_.transform.InverseY(y)); }
        auto TakeSettingsChanged(void) -> bool { return std::exchange(this->changed_, false); }
        auto Batch(bool pass) -> void;
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextProfiles(visit, this->text_); }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return visibility_start + 4; }
        auto ContentOption(size_t index) -> module_option override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto RenderOverlay(const module_render_context& context, const ui::config_text_settings& text, float mouseX, float mouseY) const -> void;
    private:
        friend struct phoenix_integration_test;
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
        enum option : size_t { enabled, open, automatic, expanded, width, opacity, padding, selected_text, base_options };
        static constexpr size_t visibility_start = base_options + text_profile_option_count;
        enum class action : uint8_t { lot, pass, lot_all, pass_all, expand, details, close };
        struct control { ui::rect bounds{}; action kind{}; size_t slot{}; uint64_t generation{}; bool available{}; };
        struct pointer_press { control target{}; std::vector<treasure_action> batch{}; };
        module_state state_{true, {-1, -1, 540, 200}, 440, 100, 1000};
        treasure_source* source_{};
        IAshitaCore* core_{};
        ui::native_ui_sounds* sounds_{};
        ui::focus_frame_style focus_style_{};
        bool automatic_{true}, expanded_{true}, forced_{}, details_{}, browse_{}, available_{}, changed_{};
        bool member_status_{true};
        size_t focus_{}, selected_{};
        int selected_text_{};
        ui::container_style container_{0.35f, 5, 10, false};
        std::array<module_text_profile, 5> text_{};
        std::vector<control> controls_{};
        std::optional<pointer_press> pressed_{};
        bool pointer_down_{};
        float mouse_x_{-1}, mouse_y_{-1};
        auto Activate(const control& control, const std::vector<treasure_action>* batch = nullptr) -> void;
        auto SubmitBatch(std::span<const treasure_action> batch) -> void;
        auto MoveFocus(int delta, bool vertical = false) -> void;
    };
}
