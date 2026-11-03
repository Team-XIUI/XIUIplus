#include "hud_runtime.hpp"
#include "config/basic_settings_layout.hpp"
#include "resources/embedded_resource.hpp"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui
{
    namespace
    {
        auto PresetText(const size_t preset) -> std::string
        {
            if (preset >= ui::hud_presets.size()) return {};
            const auto bytes = resources::LoadEmbeddedResource(reinterpret_cast<HMODULE>(&__ImageBase),
                ui::hud_presets[preset].resource, 1024 * 1024);
            return bytes.empty() ? std::string{} : std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }
    }

    auto hud_runtime::ApplyHudPreset(const size_t preset) -> bool
    {
        if (preset >= ui::hud_presets.size()) return false;
        const auto text = PresetText(preset);
        if (text.empty()) return false;
        this->FinishScale(false); this->FinishControllerMove(false); this->FinishConfigResize(false);
        this->FinishConfigInput(false); this->CloseChoiceInput();
        this->SaveSettings();
        if (!this->DrainSettings()) { this->profile_error_ = "Could not save the current profile. Try applying the preset again."; return false; }
        auto snapshot = this->CaptureSettings();
        OverlayProfilePreferences(snapshot, settings_store::FromIniText(text));
        this->LoadSettings(snapshot); this->SaveSettings();
        return true;
    }

    namespace
    {
        auto PrepareBasicPresetSamples(const std::span<party_module> samples, bool& ready) -> void
        {
            if (ready) return;
            for (size_t preset = 0; preset < samples.size(); ++preset)
                samples[preset].ConfigurePresetSample(settings_store::FromIniText(PresetText(preset)));
            ready = true;
        }

        class preset_preview_theme
        {
        public:
            preset_preview_theme(ui::design_system& design, ui::container_renderer& containers, const int appearance)
                : design_(design), containers_(containers), design_theme_(design.IsXiv1Theme() ? ui::xiv1_appearance : ui::native_appearance),
                  container_theme_(containers.AppearanceTheme()), background_(containers.BackgroundColor())
            {
                this->containers_.SetAppearanceTheme(appearance);
                this->design_.SetWindowPalette(this->containers_.BackgroundColor());
                this->design_.SetAppearanceTheme(appearance);
            }
            ~preset_preview_theme(void)
            {
                this->containers_.SetAppearanceTheme(this->container_theme_);
                this->design_.SetWindowPalette(this->background_);
                this->design_.SetAppearanceTheme(this->design_theme_);
            }
            preset_preview_theme(const preset_preview_theme&) = delete;
            auto operator=(const preset_preview_theme&) -> preset_preview_theme& = delete;
        private:
            ui::design_system& design_;
            ui::container_renderer& containers_;
            int design_theme_{}, container_theme_{};
            D3DCOLOR background_{};
        };
    }

    auto hud_runtime::SettingsModeBounds(void) const -> rect
    {
        return this->config_.collapsed ? rect{} : this->ConfigLayout().ModeButton();
    }

    auto hud_runtime::DrawSettingsFooter(void) -> void
    {
        this->DrawProfileControls();
        auto style = this->design_system_.GetTheme().body_text;
        style.scale = .76f; style.alignment = ui::text_alignment::center;
        const auto button = this->SettingsModeBounds();
        const bool hovered = Contains(button, this->mouse_x_, this->mouse_y_);
        this->config_controls_.IconButton(button, ui::icon_glyph::general, hovered);
        if (hovered)
        {
            this->config_.tooltip = this->config_.basic ? "Advanced settings: individual modules and appearance" : "Basic settings: presets and global HUD scale";
            this->config_.tooltip_anchor = button;
        }
        const auto edit = this->ConfigLayout().EditModeButton();
        const bool editHovered = Contains(edit, this->mouse_x_, this->mouse_y_);
        this->config_controls_.Button(edit, "", style, editHovered);
        this->config_controls_.ButtonText(this->config_.edit_mode ? i18n::Text("Edit Mode  ON") : i18n::Text("Edit Mode  OFF"),
            {edit.x + 14, edit.y, edit.width - 14, edit.height}, style, editHovered);
        const ui::slot_dot dot{{edit.x + 10, edit.y + (edit.height - 7) * .5f, 7, 7}, this->config_.edit_mode ? 0xFF8FD16A : 0xFF161616};
        this->design_system_.DrawSlotDots(std::span{&dot, 1});
        if (editHovered)
        {
            this->config_.tooltip = this->config_.edit_mode ? "Hide inactive previews. You can still drag visible modules." : "Preview all enabled modules to arrange your HUD.";
            this->config_.tooltip_anchor = edit;
        }
    }

    auto hud_runtime::BasicSettingBounds(const size_t index) const -> rect
    { return ui::basic_settings_layout{this->config_.bounds}.Control(index); }

    auto hud_runtime::SetBasicSettings(const bool basic, const bool save) -> void
    {
        if (this->config_.basic == basic) return;
        this->FinishScale(false); this->FinishControllerMove(false); this->FinishConfigResize(false);
        this->FinishConfigInput(false); this->CloseChoiceInput(); this->CloseSettingsSearch(); this->CloseProfileControls();
        this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
        this->config_.header_menu_open = false;
        if (basic)
        {
            PrepareBasicPresetSamples(this->basic_preset_samples_, this->basic_preset_samples_ready_);
            this->config_.search_expansion = 0; this->config_.search_animation_at = 0;
            this->config_.advanced_module = this->config_.module;
            this->config_.advanced_section = this->config_.section; this->config_.advanced_group = this->config_.group;
            this->config_.advanced_scroll = this->config_.scroll;
            this->config_.module = &this->general_; this->config_.section = 0; this->config_.scroll = 0;
        }
        else if (this->config_.advanced_module != nullptr)
        {
            this->config_.module = this->config_.advanced_module; this->config_.section = this->config_.advanced_section;
            this->config_.group = this->config_.advanced_group; this->config_.scroll = static_cast<int>(this->config_.advanced_scroll);
        }
        this->config_.basic = basic;
        this->config_.preview_selected = false;
        this->config_.focus.region = config_controller_region::options; this->config_.focus.id.clear();
        this->config_.focus.adjusting = false; this->config_.collapsed = false;
        if (save) this->SaveSettings();
    }

    auto hud_runtime::DrawBasicSettings(const D3DVIEWPORT8& viewport) -> void
    {
        PrepareBasicPresetSamples(this->basic_preset_samples_, this->basic_preset_samples_ready_);
        const auto layout = this->ConfigLayout();
        const ui::basic_settings_layout basic{this->config_.bounds};
        const auto& theme = this->design_system_.GetTheme();
        const auto hovered = [&](const rect& bounds) { return Contains(bounds, this->mouse_x_, this->mouse_y_); };
        const std::array<ui::container_fill, 2> fills{{
            {layout.HeaderBackground(), 0xD0000000 | ((theme.colors.chrome & 0xFEFEFE) >> 1)},
            {basic.FooterBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x60000000}}};
        this->config_controls_.Panel(this->ConfigVisibleBounds(), std::span{fills}.first(this->config_.collapsed ? 1 : fills.size()));
        auto header = theme.accent_text; header.scale = .82f; header.alignment = ui::text_alignment::left;
        auto button = theme.body_text; button.scale = .76f; button.alignment = ui::text_alignment::center;
        auto title = theme.body_text; title.scale = .94f; title.alignment = ui::text_alignment::left;
        auto detail = theme.caption_text; detail.scale = .72f; detail.alignment = ui::text_alignment::left;
        detail.top_color = detail.bottom_color = theme.colors.muted;
        if (this->design_system_.IsXiv1Theme())
        {
            const auto band = layout.HeaderBackground();
            const auto panel = this->ConfigVisibleBounds();
            this->container_renderer_.DrawXiv1TitleSeparator(this->design_system_, {panel.x + 7, band.y + band.height - 3, panel.width - 14, 4}, .65f);
        }
        this->config_controls_.Text(i18n::Text("Basic settings"), layout.HeaderTitle(), header);
        this->config_controls_.IconButton(layout.CloseButton(), ui::icon_glyph::close, hovered(layout.CloseButton()));
        this->config_controls_.IconButton(layout.HeaderAction(1), this->config_.collapsed ? ui::icon_glyph::add : ui::icon_glyph::minus,
            hovered(layout.HeaderAction(1)) || this->config_.collapsed);
        this->config_controls_.IconButton(layout.HeaderAction(), ui::icon_glyph::attribution, hovered(layout.HeaderAction()));
        const auto tooltip = [&](const char* text, const rect& anchor) {
            if (hovered(anchor)) { this->config_.tooltip = text; this->config_.tooltip_anchor = anchor; }
        };
        tooltip(this->config_.collapsed ? "Show Basic settings" : "Collapse to title bar", layout.HeaderAction(1));
        tooltip("Attribution", layout.HeaderAction());
        if (this->config_.collapsed) return;

        auto heading = title; heading.scale = 1.05f;
        this->config_controls_.Text(i18n::Text("Choose your look"), basic.Heading(), heading);
        if (basic.Expanded()) this->config_controls_.Text(i18n::Text("Choose a preset, theme and HUD size."), basic.Description(), detail);
        const module_render_context previewContext{this->design_system_, &this->container_renderer_, false, &this->game_icons_,
            {static_cast<float>(viewport.X), static_cast<float>(viewport.Y), static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, &this->native_assets_};
        party_member sample;
        sample.id = 0xFFFF0000; sample.active = sample.in_zone = sample.preview = sample.leader = true;
        sample.name = this->party_source_.Count(0) > 0 ? this->party_source_.Members()[0].name : "Player";
        sample.main_job = 2; sample.main_level = 75;
        sample.hp = sample.hp_max = 9999; sample.hp_percent = 100; sample.hp_max_exact = true;
        sample.mp = 8349; sample.mp_max = 10000; sample.mp_percent = 83; sample.mp_max_exact = true; sample.tp = 2900;
        for (size_t preset = 0; preset < ui::hud_presets.size(); ++preset)
        {
            const auto card = basic.Preset(preset), action = basic.PresetButton(preset), preview = basic.Preview(preset);
            this->design_system_.FillRoundedRect(card, (theme.colors.chrome & 0xFFFFFF) | 0x50000000, 4);
            this->config_controls_.Text(ui::hud_presets[preset].name, {card.x + 14, card.y + 7, card.width - 28, 22}, title);
            {
                const preset_preview_theme appearance{this->design_system_, this->container_renderer_, ui::hud_presets[preset].appearance};
                this->basic_preset_samples_[preset].RenderSample(sample, preview, previewContext);
            }
            const auto apply = i18n::Format("Apply %s", ui::hud_presets[preset].name);
            this->config_controls_.Button(action, apply.c_str(), button, hovered(action));
            tooltip("Restore this complete HUD preset, including positions and enabled modules.", action);
        }

        const auto themeOption = this->general_.Option(this->general_.AppearanceThemeOption());
        const auto themeRow = basic.ThemeRow();
        auto themeLabel = button; themeLabel.alignment = ui::text_alignment::left;
        this->config_controls_.Text(i18n::Text(themeOption.label), {themeRow.x, themeRow.y, ui::basic_settings_layout::scale_label_width, themeRow.height}, themeLabel);
        this->config_controls_.Segmented(basic.ThemeChoices(), {themeOption.choices, themeOption.choice_count},
            static_cast<size_t>(std::max(0, *themeOption.choice)), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        tooltip("Change window, bar and text styling while keeping your HUD layout.", basic.ThemeChoices());

        auto scale = this->general_.Option(this->general_.HudScaleStart());
        scale.label = "Global scale";
        const auto scaleRow = basic.ScaleRow();
        const bool editing = this->config_.slider_input.Focused() && this->config_.slider_module == &this->general_ &&
            this->config_.slider_option == this->general_.HudScaleStart();
        if (editing) this->config_.slider_bounds = basic.ScaleValue();
        if (this->interaction_ == interaction::option_slider && this->config_.active_slider == scale.slider) this->config_.slider_row = scaleRow;
        this->config_controls_.Slider(scaleRow, scale, hovered(scaleRow), editing ? &this->config_.slider_input : nullptr,
            ui::basic_settings_layout::scale_label_width, true, "%");
        this->config_controls_.Button(basic.ResetButton(), i18n::Text("Reset"), button, hovered(basic.ResetButton()));
        tooltip("Restore Global Scale to 100%", basic.ResetButton());
        tooltip("Scale all enabled HUD elements uniformly", scaleRow);
        tooltip("Click to type a percentage. Enter applies; Escape cancels.", basic.ScaleValue());
        this->DrawSettingsFooter();
        const auto grip = layout.ResizeGrip();
        const auto gripColor = hovered(grip) || this->interaction_ == interaction::resize_config ? theme.colors.control_selected : theme.colors.muted;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column <= row; ++column)
                this->design_system_.FillRect({grip.x + grip.width - 5 - column * 4, grip.y + grip.height - 5 - (row - column) * 4, 2, 2}, gripColor);
        tooltip("Drag an edge or corner to resize. Alt+F8: resize with arrow keys.", grip);
    }

    auto hud_runtime::ProcessBasicSettingsClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto layout = this->ConfigLayout();
        if (!Contains(this->ConfigVisibleBounds(), x, y)) return this->ProcessMovementToolbarClick(viewport);
        if (Contains(this->SettingsModeBounds(), x, y)) { this->SetBasicSettings(false); return true; }
        if (Contains(layout.CloseButton(), x, y)) { this->BeginCloseInteraction(interaction::close_config, layout.CloseButton()); return true; }
        if (Contains(layout.HeaderAction(1), x, y)) { this->config_.collapsed = !this->config_.collapsed; return true; }
        if (Contains(layout.HeaderAction(), x, y)) { this->SetBasicSettings(false); this->SelectConfigModule(&this->general_); this->config_.section = general_module::attribution_section; return true; }
        if (const auto edges = this->ConfigResizeEdges(x, y); edges != 0)
        {
            this->FinishConfigInput(true, false); this->CloseProfileControls();
            this->interaction_ = interaction::resize_config; this->config_resize_edges_ = edges;
            this->config_resize_original_ = this->config_.bounds; this->config_resize_pointer_ = {x, y};
            return true;
        }
        if (this->config_.collapsed) return false;
        for (size_t row = 0; row < ui::basic_settings_layout::control_count; ++row)
        {
            const auto bounds = this->BasicSettingBounds(row);
            if (!Contains(bounds, x, y)) continue;
            if (row == ui::basic_settings_layout::theme_control)
            {
                const auto option = this->general_.Option(this->general_.AppearanceThemeOption());
                for (size_t theme = 0; theme < option.choice_count; ++theme)
                    if (Contains(ui::config_controls::Segment(bounds, theme, option.choice_count), x, y))
                        return this->ActivateBasicItem(row, theme == 0 ? controller_action::left : controller_action::right, false);
                return false;
            }
            if (row != 2) return this->ActivateBasicItem(row, controller_action::confirm, false);
            const auto index = this->general_.HudScaleStart();
            const auto option = this->general_.Option(index);
            this->config_.option = index;
            const auto field = ui::basic_settings_layout{this->config_.bounds}.ScaleValue();
            if (Contains(field, x, y))
            {
                this->config_.slider_input.Open(option); this->config_.slider_module = &this->general_;
                this->config_.slider_option = index; this->config_.slider_bounds = field;
            }
            else
            {
                this->config_.active_slider = option.slider; this->config_.slider_minimum = option.minimum;
                this->config_.slider_maximum = option.maximum; this->config_.slider_step = option.step;
                this->config_.slider_row = bounds; this->interaction_ = interaction::option_slider;
                this->SetActiveSliderFromMouse();
            }
            return true;
        }
        return false;
    }

    auto hud_runtime::BasicControllerItems(const config_controller_region area) -> std::vector<config_controller_item>
    {
        std::vector<config_controller_item> result;
        if (area != config_controller_region::options || this->config_.collapsed) return result;
        static constexpr const char* names[]{"Classic", "XIV", "Global Scale", "Reset Global Scale", "XIVClassic", "Theme"};
        static constexpr const char* help[]{"Restore the complete Classic setup and layout.", "Restore the complete XIV setup and layout.",
            "Uniform scale for all enabled HUD elements. Confirm adjusts; Actions opens percentage entry.", "Restore Global Scale to 100%.",
            "Restore the complete XIVClassic setup and XIV 1.0 theme.", "Confirm adjusts; Left/Right chooses a theme without changing your HUD layout."};
        for (size_t index = 0; index < std::size(names); ++index)
            result.push_back({std::string{"basic/"} + names[index], this->BasicSettingBounds(index), index, help[index]});
        return result;
    }

    auto hud_runtime::ActivateBasicItem(const size_t row, const controller_action action, const bool repeated) -> bool
    {
        if (row >= ui::basic_settings_layout::control_count) return false;
        if (row == ui::basic_settings_layout::theme_control)
        {
            if (repeated || (action != controller_action::confirm && action != controller_action::left && action != controller_action::right)) return true;
            if (action == controller_action::confirm) { this->config_.focus.adjusting = !this->config_.focus.adjusting; return true; }
            const auto index = this->general_.AppearanceThemeOption();
            const auto option = this->general_.Option(index);
            const int selected = action == controller_action::left ? 0 : 1;
            this->config_.option = index;
            if (*option.choice != selected)
            {
                *option.choice = selected;
                this->general_.OnOptionChanged(index);
                this->SaveSettings();
            }
            return true;
        }
        if (row == 2)
        {
            const auto option = this->general_.Option(this->general_.HudScaleStart());
            if (action == controller_action::actions && !repeated)
            {
                this->config_.slider_input.Open(option); this->config_.slider_module = &this->general_;
                this->config_.slider_option = this->general_.HudScaleStart();
                this->config_.slider_bounds = ui::basic_settings_layout{this->config_.bounds}.ScaleValue();
                this->config_.focus.adjusting = false;
            }
            else if (action == controller_action::confirm && !repeated) this->config_.focus.adjusting = !this->config_.focus.adjusting;
            else if (this->config_.focus.adjusting && (action == controller_action::left || action == controller_action::right))
            {
                *option.slider = ui::slider_value_input::Normalize(*option.slider + (action == controller_action::left ? -.01f : .01f), option.minimum, option.maximum, option.step);
                this->SaveSettings();
            }
            return true;
        }
        if (action != controller_action::confirm || repeated) return true;
        return this->ActivateConfigAction(this->general_, row == 3 ? this->general_.HudScaleStart() + 1 :
            this->general_.HudPresetOption(row == ui::basic_settings_layout::xivclassic_control ? 2 : row));
    }
}
