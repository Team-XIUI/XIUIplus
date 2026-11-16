#pragma once

#include "font_catalog.hpp"

#include <map>
#include <optional>
#include <vector>

namespace xiui::ui
{
    // available: the requested family is installed. usable: a face was created, possibly a substitute.
    struct raster_font_metrics { float height{18}, ascent{15}, digit_advance{8}; bool available{}, usable{}; };
    auto NativeFontMetrics(int font, int pixels, font_variant variant = {}) -> raster_font_metrics;
    struct raster_glyph
    {
        int width{}, height{}, padding{};
        float advance{}, ascent{};
        std::array<std::vector<uint8_t>, 3> masks{};
    };

    class text_rasterizer
    {
    public:
        ~text_rasterizer(void) { this->Clear(); }
        auto Clear(void) -> void;
        auto Metrics(int font, int pixels, font_variant variant = {}) -> raster_font_metrics;
        auto Advance(int font, int pixels, uint32_t codepoint, font_variant variant = {}) -> float;
        auto Rasterize(int font, int pixels, uint32_t codepoint, font_variant variant = {}) -> std::optional<raster_glyph>;

    private:
        // shift moves drawing (supersampled pixels) onto the layout baseline; extent is the drawn box bottom below the layout top.
        struct face { HFONT handle{}; raster_font_metrics metrics{}; uint64_t touched{}; int embolden{}; int shift{}; float extent{}; };
        std::map<uint32_t, face> faces_{};
        HDC dc_{};
        HBITMAP bitmap_{};
        HGDIOBJ old_bitmap_{};
        uint32_t* pixels_{};
        int width_{}, height_{};
        uint64_t touched_{};
        embedded_font grammara_{};
        auto Face(int font, int pixels, font_variant variant) -> face*;
        auto GlyphFace(int font, int pixels, uint32_t codepoint, font_variant variant) -> face*;
        auto Surface(int width, int height) -> bool;
    };
}
