#pragma once

#include "design_system.hpp"
#include "localization/localization.hpp"
#include "ui/container_renderer.hpp"
#include "ui/button_renderer.hpp"
#include "ui/meter_text_layout.hpp"
#include "runtime_api.hpp"
#include "ui/health_feedback.hpp"
#include "settings/settings_store.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <functional>
#include <string_view>

#include "ui/selection_style.hpp"

namespace xiui::ui { class game_icon_cache; class native_ui_assets; struct tp_meter_colors; }

namespace xiui
{
    enum class module_option_group : uint8_t
    {
        general,
        appearance,
        effects,
        colors,
        text,
    };

    enum class module_option_kind : uint8_t
    {
        toggle,
        color,
        slider,
        choice,
        action,
        text_input,
        dropdown,
        color_override,
        toggle_buttons,
    };

    enum class module_selector : uint8_t { none, element, scope };

    struct module_option
    {
        module_option_kind kind{};
        module_selector selector{};
        module_option_group group{module_option_group::general};
        const char* label{};
        const char* description{};
        const char* submodules{};
        bool available{true};
        bool visible{true};
        bool* toggle{};
        std::span<bool> button_toggles{};
        D3DCOLOR* color{};
        float* slider{};
        float minimum{};
        float maximum{1.0f};
        float step{0.1f};
        float display_scale{1};
        int* choice{};
        bool* choice_explicit{};
        const char* const* choices{};
        size_t choice_count{};
        bool translate_choices{true};
        auto SetChoice(const int value) const -> bool
        {
            if (this->choice == nullptr || value < 0 || static_cast<size_t>(value) >= this->choice_count) return false;
            const bool changed = *this->choice != value || (this->choice_explicit != nullptr && !*this->choice_explicit);
            *this->choice = value;
            if (this->choice_explicit != nullptr) *this->choice_explicit = true;
            return changed;
        }
        auto ChoiceText(size_t index) const -> const char*
        { return this->translate_choices ? i18n::Text(this->choices[index]) : this->choices[index]; }
        auto VerbatimChoices() const -> module_option
        { auto result = *this; result.translate_choices = false; return result; }

        uint32_t choice_status_mask{}, choice_status_enabled{};
        const char* action_text{};
        const char* confirmation{};
        ui::icon_glyph action_icon{ui::icon_glyph::unknown};
        std::string* text{};
        size_t text_limit{500};
        bool choice_preview{};
        size_t preview_first{};
        // Presentation only; option IDs and saved keys stay stable.
        int display_order{1000};
        bool pair_next{};
        bool card{};
        bool warning{};

        auto Ordered(int order, bool pairNext = false) const -> module_option
        {
            auto result = *this;
            result.display_order = order;
            result.pair_next = pairNext;
            return result;
        }
        auto VisibleWhen(bool condition) const -> module_option
        {
            auto result = *this;
            result.visible = result.visible && condition;
            return result;
        }

        static auto Toggle(const char* label, const char* description, bool* value, module_option_group group = module_option_group::general) -> module_option;
        static auto ToggleButtons(const char* label, const char* description, std::span<bool> values, std::span<const char* const> labels) -> module_option;
        auto ToggleButton(size_t index) const -> bool;
        static auto Color(const char* label, const char* description, D3DCOLOR* value, module_option_group group = module_option_group::general) -> module_option;
        static auto Slider(const char* label, const char* description, float* value, float minimum, float maximum, float step, module_option_group group = module_option_group::general) -> module_option;
        static auto Choice(const char* label, const char* description, int* value, const char* const* choices, size_t count, module_option_group group = module_option_group::general) -> module_option;
        static auto Element(const char* label, const char* description, int* value, const char* const* choices, size_t count, module_option_group group = module_option_group::general) -> module_option;
        static auto EditScope(const char* label, const char* description, int* value, const char* const* choices, size_t count) -> module_option;
        static auto Action(const char* label, const char* description, const char* buttonText, module_option_group group = module_option_group::general) -> module_option;
        static auto Text(const char* label, const char* description, std::string* value, size_t limit = 500) -> module_option;
        static auto Dropdown(const char* label, const char* description, int* value, const char* const* choices, size_t count) -> module_option;
        static auto OverrideColor(const char* label, const char* description, bool* enabled, D3DCOLOR* color) -> module_option;
    };

    struct module_text_profile
    {
        ui::text_style style{};
        float offset_x{};
        float offset_y{};
        int placement{};
        bool visible{true};
    };

    struct module_config_section
    {
        const char* name{};
        const char* description{};
        const char* id{};
        const char* category{};
        auto Id(void) const -> std::string_view { return this->id != nullptr ? this->id : this->name; }
    };

    struct meter_animation_state
    {
        float displayed_ratio{};
        uint64_t last_tick{};
        bool initialized{};
        ui::health_feedback_state health{};
        uint64_t identity{};
        auto Bind(uint64_t value) -> void { if (this->identity != value) { *this = {}; this->identity = value; } }
        auto Update(float ratio, uint64_t now, bool enabled = true, float speed = 10.0f) -> float;
    };

    enum class meter_fill_mode : uint8_t { single, tp, tp_blink, health };
    enum class edit_grip_kind : uint8_t { scale, resize, none };

    struct module_state
    {
        bool enabled{true};
        ui::rect bounds{};
        float minimum_width{};
        float minimum_height{};
        float maximum_width{4096.0f};
        ui::rect content_bounds{};
        ui::hud_scale scale{};
        ui::hud_scale scale_checkpoint{};
        ui::hud_transform transform{};
        bool position_initialized{};

        auto AutomaticPosition(float coordinate) const -> bool { return !this->position_initialized && coordinate < 0; }

        auto EditBounds(void) const -> ui::rect
        {
            if (this->content_bounds.width <= 0.0f || this->content_bounds.height <= 0.0f)
                return this->bounds;
            constexpr float padding = 8.0f;
            return {
                this->bounds.x + this->content_bounds.x - padding,
                this->bounds.y + this->content_bounds.y - padding,
                this->content_bounds.width + padding * 2.0f,
                this->content_bounds.height + padding * 2.0f};
        }
    };

    auto ModuleWidthOption(module_state& state) -> module_option;
    inline auto TextSizeOption(const char* label, const char* description, float* scale) -> module_option
    {
        auto option = module_option::Slider(label, description, scale, ui::text_min_scale, ui::text_max_scale, 1 / ui::text_base_pixels, module_option_group::text);
        option.display_scale = ui::text_base_pixels;
        return option;
    }

    class module_render_context
    {
    public:
        explicit module_render_context(ui::design_system& design, ui::container_renderer* containers = nullptr, bool editing = false, ui::game_icon_cache* icons = nullptr, const ui::rect& viewport = {}, ui::native_ui_assets* nativeAssets = nullptr, const ui::health_feedback_style* feedback = nullptr);
        auto Design(void) const -> const ui::design_system& { return this->design_; }
        auto Containers(void) const -> const ui::container_renderer* { return this->containers_; }
        auto Viewport(void) const -> ui::rect { return this->design_.Transform().Inverse(this->viewport_); }
        auto Icons(void) const -> ui::game_icon_cache* { return this->icons_; }
        auto NativeAssets(void) const -> ui::native_ui_assets* { return this->native_assets_; }
        auto DrawButton(const ui::rect& bounds, const char* label, bool highlighted, const module_text_profile& profile, ui::button_tone tone = ui::button_tone::normal) const -> void;
        auto IsEditing(void) const -> bool { return this->editing_; }
        auto DrawContainer(const ui::rect& bounds, const ui::container_style& style = {}, std::span<const ui::container_fill> fills = {}) const -> ui::rect;
        auto DrawContainerFrame(const ui::rect& bounds, const ui::container_style& style) const -> void;
        auto DrawTooltip(const ui::rect& bounds) const -> ui::rect;
        auto DrawFadingContainer(const ui::rect& bounds, const ui::container_style& style, bool anchoredRight) const -> void;
        auto DrawSelection(const ui::rect& bounds, bool subtarget, const ui::selection_style& style) const -> void;
        auto DrawText(const char* text, const ui::rect& bounds, const ui::rect& placementBounds, const module_text_profile& profile) const -> void;
        auto TextBounds(const char* text, const ui::rect& bounds, const ui::rect& placementBounds, const module_text_profile& profile) const -> ui::rect;
        auto DrawBar(const ui::rect& bounds, float ratio, D3DCOLOR fillTop, D3DCOLOR fillBottom, float phase, const ui::meter_style& style, bool fillFromRight = false) const -> void;
        auto MeterTextBounds(const ui::rect& bounds, const ui::meter_style& style) const -> ui::rect;
        auto DrawMeter(const ui::rect& bounds, const char* label, const char* value, float ratio, D3DCOLOR fillTop, D3DCOLOR fillBottom, const ui::meter_style& style, meter_animation_state& animation, bool overlayLabel = false, const module_text_profile* labelProfile = nullptr, const module_text_profile* valueProfile = nullptr, ui::meter_text_anchor textAnchor = ui::meter_text_anchor::automatic, ui::meter_text_anchor valueAnchor = ui::meter_text_anchor::automatic, meter_fill_mode fillMode = meter_fill_mode::single, std::span<const ui::meter_strip> strips = {}, std::span<const ui::meter_range> ranges = {}, const ui::tp_meter_colors* tpColors = nullptr, bool healthWarnings = true) const -> void;
        static auto Ratio(uint32_t value, uint32_t maximum) -> float;

    private:
        ui::design_system& design_;
        ui::container_renderer* containers_{};
        ui::game_icon_cache* icons_{};
        ui::native_ui_assets* native_assets_{};
        const ui::health_feedback_style* feedback_{};
        bool editing_{};
        ui::rect viewport_{};
        auto MeterSkin(const ui::meter_style& style) const -> std::optional<ui::meter_skin>;
    };

    constexpr size_t meter_style_option_count = 30;
    constexpr size_t text_profile_option_count = 16;
    using text_settings_visitor = std::function<void(ui::text_style&)>;

    inline auto VisitTextProfiles(const text_settings_visitor& visit, const std::span<module_text_profile> profiles) -> void
    { for (auto& profile : profiles) visit(profile.style); }

    inline auto VisitTextStyles(const text_settings_visitor& visit, const std::initializer_list<ui::text_style*> styles) -> void
    { for (auto* style : styles) visit(*style); }

    auto TextProfileOption(size_t index, module_text_profile& profile) -> module_option;
    auto TintTextProfile(const module_text_profile& profile, D3DCOLOR tint) -> module_text_profile;
    auto LoadTextProfile(const settings_store& settings, const char* section, const module_text_profile& fallback) -> module_text_profile;
    auto SaveTextProfile(const settings_store& settings, const char* section, const module_text_profile& profile) -> void;
    auto MeterGradientTop(D3DCOLOR color) -> D3DCOLOR;
    auto MeterStyleOption(size_t index, ui::meter_style& style) -> module_option;
    auto CopyMeterStyleOption(size_t index, ui::meter_style& source, ui::meter_style& destination) -> void;
    auto LoadMeterStyle(const settings_store& settings, const char* section, const ui::meter_style& fallback) -> ui::meter_style;
    auto SaveMeterStyle(const settings_store& settings, const char* section, const ui::meter_style& style) -> void;

    class hud_module
    {
    public:
        virtual ~hud_module(void) = default;

        virtual auto Id(void) const -> const char* = 0;
        virtual auto LayoutId(void) const -> const char* { return this->Id(); }
        virtual auto DisplayName(void) const -> const char* = 0;
        virtual auto State(void) -> module_state& = 0;
        virtual auto State(void) const -> const module_state& = 0;
        virtual auto HasEditableBounds(void) const -> bool { return true; }
        virtual auto CanMove(void) const -> bool { return this->HasEditableBounds(); }
        virtual auto ConfigScaleEditable(void) const -> bool { return this->HasEditableBounds(); }
        virtual auto ConfigScaleVisible(void) const -> bool { return true; }
        virtual auto PositionBoundsReady(void) const -> bool { return true; }
        virtual auto PreviewVisible(void) const -> bool { return this->State().enabled; }
        virtual auto ApplyMeterDefaults(const ui::meter_style&) -> void {}
        virtual auto ApplyTextDefaults(const ui::theme&) -> void {}
        virtual auto VisitTextSettings(const text_settings_visitor&) -> void {}
        virtual auto ResetDefaults(const ui::theme& themeDefaults) -> void = 0;
        virtual auto Load(const settings_store& settings) -> void = 0;
        virtual auto Save(const settings_store& settings) const -> void = 0;
        virtual auto ResetSettingsFrom(const hud_module& defaults, const ui::theme& theme) -> void;
        virtual auto ContentOptionCount(void) const -> size_t = 0;
        virtual auto ContentOption(size_t index) -> module_option = 0;
        virtual auto SupportsScale(void) const -> bool { return true; }
        virtual auto SupportsNonUniformScale(void) const -> bool { return true; }
        virtual auto EditGrip(void) const -> edit_grip_kind { return edit_grip_kind::scale; }
        virtual auto EditGripBounds(const ui::design_system&) const -> std::optional<ui::rect> { return std::nullopt; }
        virtual auto OnBoundsResized(const ui::rect&) -> void {}
        virtual auto ConfigScaleState(void) -> module_state& { return this->State(); }
        auto BindConfigScale(module_state* state, bool editable) -> void { this->config_scale_state_ = state; this->config_scale_editable_ = editable; }
        auto ActiveScaleState(void) -> module_state& { return this->config_scale_state_ != nullptr ? *this->config_scale_state_ : this->ConfigScaleState(); }
        auto OptionCount(void) const -> size_t { return this->ContentOptionCount() + (this->SupportsScale() ? 4 : 0); }
        auto Option(size_t index) -> module_option;
        virtual auto ConfigViews(void) const -> std::span<const char* const> { return {}; }
        virtual auto ConfigView(void) const -> size_t { return 0; }
        virtual auto SelectConfigView(size_t) -> void {}
        virtual auto ConfigSections(void) const -> std::span<const module_config_section> { return {}; }
        virtual auto ContentConfigSectionContains(size_t, size_t) -> bool { return false; }
        auto ConfigSectionContains(size_t section, size_t index) -> bool;
        virtual auto OnContentOptionChanged(size_t) -> void {}
        auto OnOptionChanged(size_t index) -> void;
        virtual auto LoadScale(const settings_store& store) -> void;
        auto SaveScale(const settings_store& store) const -> void;
        virtual auto PreviewChoice(size_t, size_t) -> bool { return false; }
        virtual auto OnPositionChanged(void) -> void {}
        virtual auto ShouldRender(const hud_snapshot& snapshot) const -> bool = 0;
        virtual auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void = 0;
    private:
        module_state* config_scale_state_{};
        bool config_scale_editable_{true};
    };
} // namespace xiui
