#pragma once

#include "nine_slice.hpp"
#include "meter_skin.hpp"
#include "resources/menu_window.hpp"

#include <span>

#if defined(XIUI_ENABLE_INPUT_TESTS)
namespace xiui { struct config_input_test; }
#endif

namespace xiui::ui
{
    struct container_fill
    {
        rect bounds{};
        D3DCOLOR color{};
    };

    struct container_style
    {
        float opacity{1.0f};
        float edge_size{5.0f};
        float padding{8.0f};
        bool scan_lines{true};
        bool border{true};
    };

    class container_renderer
    {
    public:
        container_renderer(void) = default;
        ~container_renderer(void);
        container_renderer(const container_renderer&) = delete;
        auto operator=(const container_renderer&) -> container_renderer& = delete;

        auto LoadSkin(IResourceManager* resources) -> void;
        auto SetDevice(IDirect3DDevice8* device) -> bool;
        auto Shutdown(void) -> void;
        auto SelectSkin(uint8_t type) -> void { if (type < this->windows_.size()) this->selected_ = type; }
        auto SelectedSkin(void) const -> uint8_t { return this->selected_; }
        auto SetAppearanceTheme(int value) -> void { this->appearance_theme_ = value == 1 ? 1 : 0; }
        auto AppearanceTheme(void) const -> int { return this->appearance_theme_; }
        auto SetXiv1BackgroundOpacity(float opacity) -> void { this->xiv1_background_opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
        auto BackgroundOpacity(const container_style& style) const -> float
        { return std::clamp(style.opacity, 0.0f, 1.0f) * (this->appearance_theme_ == 1 ? this->xiv1_background_opacity_ : 1); }
        auto BackgroundColor(void) const -> D3DCOLOR { return this->appearance_theme_ == 1 ? 0xFF0B102B : this->windows_[this->selected_].average; }
        auto MeterBackground(void) const -> meter_texture;
        auto AppearanceMeterSkin(void) const -> std::optional<meter_skin> { return this->appearance_theme_ == 1 ? this->xiv1_meter_ : std::nullopt; }
        // Bounds start at the straight rail; the curved cap extends to the left.
        auto DrawXiv1TargetHeader(const design_system& design, const rect& bounds, float opacity = 1) const -> void;
        auto DrawXiv1ActionMedallion(const design_system& design, const rect& bounds, float opacity = 1) const -> void;
        auto DrawXiv1TitleSeparator(const design_system& design, const rect& bounds, float opacity = 1) const -> void;
        auto Draw(const design_system& design, const rect& bounds, const container_style& style, std::span<const container_fill> fills = {}, const horizontal_fade* fade = nullptr) const -> rect;
        auto DrawFrame(const design_system& design, const rect& bounds, const container_style& style, const horizontal_fade* fade = nullptr) const -> void;
        auto DrawTooltip(const design_system& design, const rect& bounds) const -> rect;
        static constexpr auto TooltipStyle(void) -> container_style { return {1.0f, 2.5f, 10.5f, false}; }
        static auto ContentBounds(const rect& bounds, const container_style& style) -> rect;

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct xiui::config_input_test;
#endif
        IDirect3DDevice8* device_{};
        struct window_skin
        {
            std::optional<resources::menu_window> source{};
            std::optional<resources::menu_texture> meter_background{};
            std::array<IDirect3DTexture8*, 6> textures{};
            std::array<nine_slice_skin, 2> variants{};
            D3DCOLOR average{0xFF202040};
        };
        std::array<window_skin, 8> windows_{};
        uint8_t selected_{};
        int appearance_theme_{};
        float xiv1_background_opacity_{.4f};
        IDirect3DTexture8* xiv1_atlas_{};
        IDirect3DTexture8* xiv1_fabric_{};
        nine_slice_skin xiv1_window_{};
        std::optional<meter_skin> xiv1_meter_{};
        auto LoadAppearanceAssets(void) -> void;
        auto ReleaseTextures(void) -> void;
        auto CreateTexture(const resources::menu_texture& image, IDirect3DTexture8** texture) const -> bool;
        static auto CorrectWindowFrame(resources::menu_window& window, uint8_t type) -> void;
    };
} // namespace xiui::ui
