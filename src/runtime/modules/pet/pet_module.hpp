#pragma once

#include "pet_source.hpp"
#include "modules/module.hpp"
#include "modules/recast/recast_source.hpp"
#include "ui/status_strip.hpp"
#include "ui/pet_portraits.hpp"
#include "pet_recast_settings.hpp"

namespace xiui
{
    class pet_module final : public hud_module
    {
    public:
        explicit pet_module(bool target = false);
        auto Id(void) const -> const char* override { return this->target_ ? "pettarget" : "pet"; }
        auto DisplayName(void) const -> const char* override { return this->target_ ? "Pet Target" : "Pet"; }
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto SetOwner(pet_module* owner) -> void { this->owner_ = owner; if (owner != nullptr) owner->target_adapter_ = this; }
        auto SetSource(pet_source* source) -> void { this->source_ = source; }
        auto SetRecasts(const recast_source* source) -> void { this->recasts_ = source; }
        auto SetDisplayAllowed(bool allowed) -> void { this->Main().target_allowed_ = allowed; }
        auto HasEditableBounds(void) const -> bool override { return !this->target_ || !this->Main().target_settings_.snapped; }
        auto PreviewVisible(void) const -> bool override;
        auto PositionBoundsReady(void) const -> bool override { return this->PreviewVisible(); }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override;
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ConfigViews(void) const -> std::span<const char* const> override;
        auto ConfigView(void) const -> size_t override { return this->view_; }
        auto SelectConfigView(size_t index) -> void override { this->view_ = std::min(index, pet_kind_count); }
        auto ContentOptionCount(void) const -> size_t override;
        auto ContentOption(size_t index) -> module_option override;
        auto OnContentOptionChanged(size_t) -> void override { if (this->Main().source_) this->Main().source_->SetBonusCharisma(this->Main().types_[static_cast<size_t>(pet_kind::charm)].bonus_charisma); }
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto BeginControllerStatus(void) -> bool;
        auto HasStatusDetails(void) const -> bool { return !this->status_hits_.empty(); }
        auto MoveControllerStatus(int direction) -> bool;
        auto EndControllerStatus(void) -> void { this->controller_entity_ = 0; this->controller_effect_ = 0; this->controller_occurrence_ = 0; this->controller_since_ = 0; }
        auto ControllerStatusBounds(void) const -> ui::rect;
        auto RenderStatusTooltip(const module_render_context& context, IResourceManager* resources, float x, float y, bool controller = false) const -> void;
    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct pet_hud_test;
#endif
        struct portrait_settings
        {
            float scale{1}, opacity{1}, offset_x{}, offset_y{};
            bool clip{};
            auto LegacyDefault(void) const -> bool { return this->scale == 1 && this->opacity == 1 && this->offset_x == 0 && this->offset_y == 0 && !this->clip; }
            auto Load(const settings_store& store, const std::string& section) -> void;
            auto Save(const settings_store& store, const std::string& section) const -> void;
            auto Bounds(const ui::rect& anchor, float size) const -> ui::rect;
        };
        struct type_settings
        {
            bool enabled{true}, hp{true}, mp{true}, tp{true}, distance{true}, icon{true}, recasts{true}, without_pet{}, elapsed{};
            bool background{true};
            bool portrait{true}, lifetime{true};
            float spacing{4}, portrait_size{48};
            int portrait_position{};
            portrait_settings portrait_style{};
            ui::container_style container{1, 5, 6, false};
            std::array<ui::meter_style, 3> meters{};
            std::array<D3DCOLOR, 3> top{0xFF416348, 0xFF355D81, 0xFF77643C}, bottom{0xFF94D89D, 0xFF8DC7F4, 0xFFFFD787};
            std::array<module_text_profile, 9> text{};
            std::array<float, 3> resource_width{};
            bool split_values{}, align_bottom{}, estimate_lifetime{};
            float bonus_charisma{};
            pet_recast_settings recast_style{};
            ui::status_strip_settings statuses{};
            int resource{}, selected_text{};
        };
        struct target_settings
        {
            bool enabled{}, snapped{true}, distance{true}, percent{true}, label{true};
            bool background{true}, tint{};
            ui::container_style container{1, 5, 2, false};
            D3DCOLOR background_tint{0xFF7088C0};
            float tint_strength{0.2f};
            auto TintColor(void) const -> D3DCOLOR
            { return (this->background_tint & 0xFFFFFF) | (static_cast<uint32_t>((this->background_tint >> 24) * this->tint_strength) << 24); }
            float gap{6}, width{}, offset_x{}, offset_y{};
            D3DCOLOR top{0xFF704242}, bottom{0xFFFB9494};
            ui::meter_style meter{ui::theme::CreateDefault().meter};
            std::array<module_text_profile, 3> text{};
            int selected_text{};
        } target_settings_{};
        bool target_{};
        module_state state_{false, {24, 330, 320, 180}, 140, 40};
        pet_module* owner_{};
        pet_module* target_adapter_{};
        pet_source* source_{};
        const recast_source* recasts_{};
        std::array<type_settings, pet_kind_count> types_{};
        std::array<portrait_settings, std::size(ui::avatar_portrait_keys)> avatar_portraits_{};
        int selected_avatar_{};
        size_t view_{};
        bool target_allowed_{true};
        std::array<meter_animation_state, 3> animations_{};
        meter_animation_state target_animation_{};
        ui::rect status_bounds_{};
        pet_kind drawn_kind_{pet_kind::count};
        uint32_t drawn_pet_id_{}, drawn_pet_index_{}, drawn_target_id_{};
        uint64_t drawn_since_{};
        std::vector<ui::status_tooltip> status_hits_{};
        uint32_t controller_entity_{};
        uint16_t controller_effect_{};
        size_t controller_occurrence_{};
        uint64_t controller_since_{};
        auto ControllerStatusIndex(void) const -> std::optional<size_t>;
        static constexpr size_t base_options = 29, meter_start = base_options, text_start = meter_start + meter_style_option_count;
        static constexpr size_t status_start = text_start + text_profile_option_count;
        static constexpr size_t recast_start = status_start + ui::status_strip_settings::option_count;
        static constexpr size_t resource_extra_start = recast_start + pet_recast_settings::option_count;
        static constexpr size_t target_base_options = 18;
        auto Main(void) -> pet_module& { return this->owner_ != nullptr ? *this->owner_ : *this; }
        auto Main(void) const -> const pet_module& { return this->owner_ != nullptr ? *this->owner_ : *this; }
        auto TargetConfig(void) const -> bool { return this->target_ || this->view_ == pet_kind_count; }
        auto TargetOption(size_t index) -> module_option;
        auto TargetContains(size_t section, size_t index) const -> bool;
        auto DrawTarget(const pet_entity& target, const ui::rect& bounds, const module_render_context& context) -> float;
        auto Cooldowns(pet_kind kind, uint64_t now) const -> std::vector<recast_entry>;
        auto DrawRecasts(pet_kind kind, std::span<const recast_entry> entries, float x, float y, float width, const module_render_context& context) -> void;
        auto DisplayKind(void) const -> pet_kind;
        auto PortraitStyle(pet_kind kind, std::string_view name) -> portrait_settings&;
        static auto DefaultPortrait(size_t index) -> portrait_settings;
        auto ResetPortraitDefaults(void) -> void;
        static auto LifetimeLabel(uint64_t observedSince, uint64_t now, bool elapsed) -> std::string;
    };
}
