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
        auto BackgroundColor(void) const -> D3DCOLOR { return this->windows_[this->selected_].average; }
        auto MeterBackground(void) const -> meter_texture;
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
        auto ReleaseTextures(void) -> void;
        auto CreateTexture(const resources::menu_texture& image, IDirect3DTexture8** texture) const -> bool;
        static auto CorrectWindowFrame(resources::menu_window& window, uint8_t type) -> void;
    };
} // namespace xiui::ui
