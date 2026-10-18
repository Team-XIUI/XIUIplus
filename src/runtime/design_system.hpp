#pragma once

#include "Ashita.h"
#include "ui/glyph_atlas.hpp"
#include "ui/text_size.hpp"
#include "ui/icon_atlas.hpp"
#include "ui/font_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>
#include <string>

namespace xiui::ui
{
    struct meter_skin;
    struct focus_frame_style;
    struct rect
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct point { float x{}, y{}; };
    struct circle_fill { point center{}; float radius{}; D3DCOLOR color{}; };
    struct slot_dot { rect bounds{}; D3DCOLOR color{}; };
    struct meter_fill_layer { float ratio{}; D3DCOLOR top{}, bottom{}; };
    struct meter_strip { float ratio{}, height{3}, inset{2}; D3DCOLOR top{}, bottom{}; };
    struct meter_range { float start{}, end{}; D3DCOLOR top{}, bottom{}; bool beyond_fill{}, striped{}; };

    enum class text_alignment : uint8_t
    {
        left,
        center,
        right,
    };

    struct texture_style
    {
        std::array<D3DCOLOR, 4> colors{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        bool repeat_x{}, repeat_y{}, additive{}, double_modulation{};
    };

    struct texture_quad
    {
        std::array<point, 4> points{};
        rect uv{};
        texture_style style{};
    };

    struct shadow_effect
    {
        bool enabled{true};
        float offset_x{1.0f};
        float offset_y{1.0f};
        D3DCOLOR color{D3DCOLOR_ARGB(220, 0, 0, 0)};
        bool soft{false};
    };

    struct stroke_effect
    {
        bool enabled{false};
        D3DCOLOR color{D3DCOLOR_ARGB(255, 0, 0, 0)};
    };

    struct glow_effect
    {
        bool enabled{false};
        D3DCOLOR color{D3DCOLOR_ARGB(90, 244, 218, 151)};
    };

    struct sheen_effect
    {
        bool enabled{false};
        D3DCOLOR color{D3DCOLOR_ARGB(90, 255, 255, 255)};
        float width{40.0f};
        float speed{0.22f};
    };

    struct text_style
    {
        float scale{1.0f};
        int font{};
        float letter_spacing{};
        bool tabular_numbers{};
        D3DCOLOR top_color{D3DCOLOR_XRGB(255, 253, 248)};
        D3DCOLOR bottom_color{D3DCOLOR_XRGB(255, 253, 248)};
        text_alignment alignment{text_alignment::left};
        shadow_effect shadow{};
        stroke_effect stroke{};
        glow_effect glow{};
        sheen_effect sheen{};
    };

    struct color_tokens
    {
        D3DCOLOR text{};
        D3DCOLOR muted{};
        D3DCOLOR outline{};
        D3DCOLOR chrome{};
        D3DCOLOR surface{};
        D3DCOLOR surface_raised{};
        D3DCOLOR border{};
        D3DCOLOR selected{};
        D3DCOLOR accent{}, accent_dim{}, control{}, control_selected{};
        D3DCOLOR gold{};
        D3DCOLOR gold_dark{};
        D3DCOLOR meter_track{};
    };

    struct spacing_tokens
    {
        float xxs{2.0f};
        float xs{4.0f};
        float sm{7.0f};
        float md{10.0f};
        float lg{16.0f};
        float xl{24.0f};
    };

    struct panel_style
    {
        D3DCOLOR background{};
        D3DCOLOR border{};
        D3DCOLOR shadow{};
        float shadow_offset{2.0f};
        float border_width{1.0f};
    };

    struct meter_style
    {
        static constexpr int window_theme = 0, modern = 1;
        int appearance{window_theme};
        float height{};
        D3DCOLOR track_top{};
        D3DCOLOR track_bottom{};
        D3DCOLOR border{};
        D3DCOLOR bookend_top{};
        D3DCOLOR bookend_bottom{};
        D3DCOLOR shadow{};
        D3DCOLOR glow{};
        D3DCOLOR sheen{};
        float border_width{2.0f};
        float corner_radius{8.0f};
        float bookend_width{10.0f};
        float track_opacity{0.88f};
        float shadow_offset_x{};
        float shadow_offset_y{1.0f};
        float shadow_blur{4.0f};
        float shadow_strength{0.2f};
        float glow_size{3.0f};
        float fill_glow_radius{4.0f};
        float fill_glow_strength{0.35f};
        float sheen_width{34.0f};
        float sheen_speed{0.15f};
        float animation_speed{10.0f};
        bool bookends{};
        bool themed_bookends{true};
        bool shadow_enabled{true};
        bool glow_enabled{};
        bool fill_glow_enabled{};
        bool sheen_enabled{true};
        bool animation_enabled{true};
    };

    struct theme
    {
        color_tokens colors{};
        spacing_tokens spacing{};
        panel_style hud_panel{};
        panel_style config_panel{};
        meter_style meter{};
        text_style body_text{};
        text_style title_text{};
        text_style caption_text{};
        text_style meter_text{};
        text_style accent_text{};

        static auto CreateDefault(void) -> theme;
    };

    class design_system
    {
    public:
        design_system(void);
        ~design_system(void);

        auto SetDevice(IDirect3DDevice8* device) -> bool;
        auto SetThemePath(const char* path) -> void;
        auto ReloadTheme(void) -> bool;
        auto PollTheme(void) -> bool;
        auto SetWindowPalette(D3DCOLOR background) -> void;
        auto SetControlPalette(D3DCOLOR base, D3DCOLOR highlighted) -> void;
        auto SetDarkBackingOpacity(float opacity) -> void;
        auto DarkBackingOpacity(void) const -> float { return this->dark_backing_opacity_; }
        auto Shutdown(void) -> void;
        auto BeginFrame(void) -> bool;
        auto EndFrame(void) -> void;
        auto BeginContentBounds(void) -> void;
        auto EndContentBounds(void) -> rect;
        auto IncludeContentBounds(const rect& bounds) const -> void { this->TrackContentBounds(bounds); }

        auto GetTheme(void) const -> const theme&;
        auto IsReady(void) const -> bool;
        auto FontAvailable(int font) const -> bool { return this->glyphs_.Metrics(font, 15).available; }
        auto SetGpuShaderEnabled(bool enabled) -> void;
        auto IsGpuShaderActive(void) const -> bool;

        auto FillRect(const rect& bounds, D3DCOLOR color) const -> void;
        auto DrawLayoutGrid(const rect& viewport, float spacing, float opacity) const -> void;
        auto FillRoundedRect(const rect& bounds, D3DCOLOR color, float radius) const -> void { this->GradientRoundedRect(bounds, color, color, radius); }
        auto FillCircles(std::span<const circle_fill> circles) const -> void;
        auto DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, D3DCOLOR color) const -> void;
        auto DrawTriangleFan(point center, std::span<const point> arc, D3DCOLOR centerColor, D3DCOLOR edgeColor) const -> void;
        auto DrawSlotDots(std::span<const slot_dot> dots) const -> void;
        auto DrawIcon(const rect& bounds, icon_glyph icon, D3DCOLOR color, int quarterTurns = 0, bool flipX = false) const -> void { this->icons_.Draw(*this, bounds, icon, color, quarterTurns, flipX); }
        auto DrawTexture(IDirect3DTexture8* texture, const rect& bounds, D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255), const rect& uv = {0.0f, 0.0f, 1.0f, 1.0f}, bool repeatX = false, bool repeatY = false) const -> void;
        auto DrawTexture(IDirect3DTexture8* texture, const rect& bounds, const rect& uv, const texture_style& style) const -> void;
        auto DrawTextureQuad(IDirect3DTexture8* texture, const std::array<point, 4>& points, const rect& uv, const texture_style& style) const -> void;
        auto DrawTextureQuads(IDirect3DTexture8* texture, std::span<const texture_quad> quads) const -> void;
        auto DrawTextureDisc(IDirect3DTexture8* texture, const rect& bounds, const rect& uv, D3DCOLOR color) const -> void;
        auto DrawTextureRoundedRect(IDirect3DTexture8* texture, const rect& bounds, const rect& uv, float radius, D3DCOLOR color) const -> void;
        auto DrawRing(const rect& bounds, D3DCOLOR color, float thickness = 2) const -> void;
        auto GradientRect(const rect& bounds, D3DCOLOR left, D3DCOLOR right) const -> void;
        auto GradientRect(const rect& bounds, const std::array<D3DCOLOR, 4>& colors) const -> void;
        auto BorderRect(const rect& bounds, D3DCOLOR color, float thickness = 1.0f) const -> void;
        auto DrawFocusFrame(const rect& bounds, const focus_frame_style& style) const -> void;
        auto DrawPanel(const rect& bounds, const panel_style& style) const -> void;
        auto DrawMeter(const rect& bounds, float ratio, D3DCOLOR color, float sheenPhase) const -> void;
        auto DrawMeter(const rect& bounds, float ratio, D3DCOLOR fillTop, D3DCOLOR fillBottom, float sheenPhase, const meter_style& style, const meter_skin* skin = nullptr, std::span<const meter_fill_layer> layers = {}, bool fillFromRight = false, std::span<const meter_strip> strips = {}, std::span<const meter_range> ranges = {}, std::span<const meter_range> feedback = {}) const -> void;
        auto DrawToggle(const rect& bounds, bool enabled) const -> void;
        auto DrawEditFrame(const rect& bounds, bool resizable = false) const -> void;
        auto DrawText(const char* text, const rect& bounds, const text_style& style) const -> void;
        class clip_scope
        {
        public:
            clip_scope(const design_system& design, const rect& bounds);
            ~clip_scope(void);
            clip_scope(const clip_scope&) = delete;
            auto operator=(const clip_scope&) -> clip_scope& = delete;
        private:
            const design_system& design_;
            std::optional<rect> previous_;
        };
        auto Clip(const rect& bounds) const -> clip_scope { return clip_scope{*this, bounds}; }
        auto MeasureText(const char* text, const text_style& style) const -> float;
        auto TextHeight(const text_style& style) const -> float { return this->glyphs_.Metrics(style.font, TextPixelSize(style.scale)).height; }
        auto DrawSheen(const rect& bounds, D3DCOLOR color, float phase, float width, float radius = 0.0f) const -> void;

    private:
        struct theme_file_stamp
        {
            uint64_t write_time{};
            uint64_t size{};

            auto operator==(const theme_file_stamp& other) const -> bool = default;
        };

        enum class font_mask : uint8_t
        {
            base,
            outline,
            glow,
        };

        IDirect3DDevice8* device_{};
        mutable glyph_atlas glyphs_{};
        IDirect3DTexture8* dot_texture_{};
        DWORD pixel_shader_{};
        DWORD state_block_{};
        bool frame_active_{};
        mutable bool tracking_content_bounds_{};
        mutable rect content_bounds_{};
        mutable std::optional<rect> clip_bounds_{};
        bool shader_attempted_{};
        bool gpu_shader_enabled_{true};
        icon_atlas icons_{};
        theme theme_{theme::CreateDefault()};
        theme configured_theme_{theme::CreateDefault()};
        D3DCOLOR window_background_{};
        std::array<D3DCOLOR, 2> control_palette_{};
        float dark_backing_opacity_{1.0f};
        auto ApplyWindowPalette(void) -> void;
        std::string theme_path_{};
        theme_file_stamp theme_stamp_{};
        theme_file_stamp pending_theme_stamp_{};
        uint64_t next_theme_poll_time_{};
        bool has_theme_stamp_{};
        bool theme_change_pending_{};

        auto LoadThemeFile(const theme_file_stamp& stamp) -> bool;
        auto ReadThemeSnapshot(const theme_file_stamp& expected, std::string& content) const -> bool;
        auto ReadThemeStamp(theme_file_stamp& stamp) const -> bool;
        auto CreateDotTexture(void) -> void;
        auto CreatePixelShader(void) -> void;
        auto ReleaseResources(void) -> void;
        auto ConfigureCommonPipeline(void) const -> void;
        auto ConfigureShapePipeline(void) const -> void;
        auto ConfigureImagePipeline(IDirect3DTexture8* texture) const -> void;
        auto ConfigureTextPipeline(IDirect3DTexture8* texture, bool additive) const -> void;
        auto DrawTextPass(const char* text, float x, float y, float maxWidth, const text_style& style, font_mask mask, float offsetX, float offsetY, bool additive, bool sheen) const -> void;
        auto TrackContentBounds(const rect& bounds) const -> void;
        auto GradientRoundedRect(const rect& bounds, D3DCOLOR top, D3DCOLOR bottom, float radius, uint8_t roundedCorners = 0xF) const -> void;
        auto DrawMeterStripes(const rect& bounds, D3DCOLOR color, float radius, uint8_t roundedCorners) const -> void;
        auto DrawMeterCap(const rect& bounds, bool right, float opacity) const -> void;

        static auto LerpColor(D3DCOLOR start, D3DCOLOR end, float amount) -> D3DCOLOR;
        static auto Darken(D3DCOLOR color, float amount) -> D3DCOLOR;
        static auto WithAlpha(D3DCOLOR color, float amount) -> D3DCOLOR;
        static auto SheenColor(D3DCOLOR color, float x, float center, float width) -> D3DCOLOR;
    };
} // namespace xiui::ui
