#pragma once

#include "modules/module.hpp"
#include "chat/chat_bubbles.hpp"
#include "chat/chat_colors.hpp"

namespace xiui
{
    class chat_bubbles_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "chat.bubbles"; }
        auto DisplayName(void) const -> const char* override { return "Bubbles"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto HasEditableBounds(void) const -> bool override { return true; }
        auto CanMove(void) const -> bool override { return false; }
        auto PreviewVisible(void) const -> bool override { return false; }
        auto EditGrip(void) const -> edit_grip_kind override { return edit_grip_kind::none; }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextStyles(visit, {&this->text_.style}); }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return text_start + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t) -> void override;
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override { return this->state_.enabled && snapshot.camera_valid && (!this->bubbles_.empty() || !this->outgoing_.empty()); }
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Receive(IAshitaCore* core, const chat_line& line) -> void;
        auto Update(IAshitaCore* core, bool dialogueActive, uint64_t now) -> void;
        auto Clear(void) -> void;
        auto SetColors(const chat_color_overrides& colors) -> void { this->colors_ = &colors; }

    private:
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
        static constexpr size_t channel_start = 8, text_start = channel_start + static_cast<size_t>(bubble_channel::count);
        struct bubble_run { std::string text; int color_index{-1}; float width{}; };
        struct bubble_row { std::vector<bubble_run> spans; float width{}; };
        struct bubble
        {
            uint32_t id{}, index{}, mode{}, color{};
            uint64_t started{}, expires{};
            bubble_channel channel{};
            std::vector<chat_span> spans;
            std::vector<bubble_row> rows;
            float cache_width{}, cache_scale{}, cache_spacing{};
            int cache_font{-1};
            float x{}, y{};
            bool prompt{}, retiring{};
            float exit_opacity{1}, exit_offset{};
        };
        struct bubble_motion { float opacity{}, offset_y{}; };
        static auto Motion(const bubble& value, uint64_t now) -> bubble_motion;
        auto Retire(bubble value, uint64_t now) -> void;
        module_state state_{};
        module_text_profile text_{};
        std::array<bool, static_cast<size_t>(bubble_channel::count)> channels_{true, true, true, true, true, true, true};
        float range_{30}, duration_{8}, width_{360}, offset_y_{28}, opacity_{}, padding_{10};
        bool chat_colors_{true}, dialogue_active_{};
        uint16_t zone_{};
        uint32_t npc_id_{}, npc_index_{};
        uint64_t last_sequence_{};
        std::vector<bubble> bubbles_{};
        std::vector<bubble> outgoing_{};
        IAshitaCore* core_{};
        const chat_color_overrides* colors_{};
        auto FindSpeaker(IAshitaCore* core, std::string_view name, bool npc) const -> uint32_t;
        auto SpeakerAvailable(const IEntity& entities, uint32_t player, uint32_t index, uint32_t id) const -> bool;
        auto Add(const chat_line& line, bubble_message message, uint32_t index, uint32_t id, uint64_t now) -> void;
        auto RebuildRows(bubble& value, const ui::design_system& design, float width) const -> void;
    };
}
