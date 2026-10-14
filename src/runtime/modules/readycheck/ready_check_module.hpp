#pragma once

#include "modules/module.hpp"
#include "modules/party/party_source.hpp"
#include "ready_check_state.hpp"
#include "ready_check_sound.hpp"
#include "ui/native_ui_sounds.hpp"

namespace xiui
{
    class ready_check_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "readycheck"; }
        auto DisplayName(void) const -> const char* override { return "Ready Check"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(party_source* source) -> void { this->source_ = source; }
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->ui_sounds_ = sounds; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->settings_.text); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto Option(size_t index) -> module_option override;
        auto OnOptionChanged(size_t index) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> packet, uint64_t now) -> void;
        auto HandleCommand(IAshitaCore* core, std::string_view command) -> void;
        auto Contains(float x, float y) const -> bool;
        auto ButtonAt(float x, float y) const -> uint32_t;
        auto PointerDown(void) const -> bool { return this->pointer_down_; }
        auto CancelPointer(void) -> void { this->pointer_down_ = false; this->pressed_ = -1; }
        auto SetHover(float x, float y) -> void { this->mouse_x_ = x; this->mouse_y_ = y; }
        auto HandleMouse(uint32_t message, float x, float y, uint64_t now) -> bool;
        auto Stop(void) -> void { this->check_.Clear(); this->CancelPointer(); this->sound_.Stop(); }

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
        enum option : size_t { enabled, start, announce, sound_checker, sound_prompt, volume, test_sound,
            column_width, row_spacing, job_icons, icon_size, opacity, padding, ready_color, no_color, pending_color,
            selected_text, base_options };
        struct preferences
        {
            bool announce{true}, sound_checker{true}, sound_prompt{true}, job_icons{true};
            float volume{50}, column_width{280}, row_spacing{4}, icon_size{24};
            ui::container_style container{0.95f, 5, 10, false};
            D3DCOLOR ready{0xFF95D89E}, no{0xFFFF8C8C}, pending{0xFFE1C98C};
            int selected_text{};
            std::array<module_text_profile, 5> text{};
        } settings_{};
        module_state state_{true, {-1, -1, 310, 280}, 240, 100};
        party_source* source_{};
        IAshitaCore* core_{};
        ready_check_state check_{};
        ready_check_sound sound_{};
        ui::native_ui_sounds* ui_sounds_{};
        std::array<ui::rect, 2> buttons_{};
        float mouse_x_{-1}, mouse_y_{-1};
        bool pointer_down_{};
        int pressed_{-1};
        uint64_t pressed_deadline_{};

        auto Roster(void) const -> std::vector<ready_check_member>;
        auto Start(uint64_t now) -> void;
        auto Answer(bool yes, uint64_t now) -> void;
        auto Send(std::string_view body) -> void;
        auto Report(const char* message) const -> void;
    };
}
