#pragma once

#include "text_rasterizer.hpp"

#include <functional>
#include <unordered_map>

namespace xiui::ui
{
    struct atlas_glyph
    {
        std::array<IDirect3DTexture8*, 3> textures{};
        float u0{}, v0{}, u1{}, v1{}, width{}, height{}, padding{}, ascent{};
    };

    class glyph_atlas
    {
    public:
        ~glyph_atlas(void) { this->Clear(); }
        auto Clear(void) -> void;
        auto Metrics(int font, int pixels) -> raster_font_metrics { return this->rasterizer_.Metrics(FontIndex(font), pixels); }
        auto Advance(int font, int pixels, uint32_t codepoint) -> float;
        auto Get(IDirect3DDevice8* device, int font, int pixels, uint32_t codepoint, const std::function<void()>& beforeUpload) -> atlas_glyph;

    private:
        struct page
        {
            std::array<IDirect3DTexture8*, 3> textures{};
            int x{}, y{}, row_height{};
            uint64_t generation{1}, touched{};
        };
        struct entry
        {
            float advance{};
            atlas_glyph glyph{};
            size_t page_index{};
            uint64_t generation{}, touched{};
        };
        text_rasterizer rasterizer_{};
        std::vector<page> pages_{};
        std::unordered_map<uint64_t, entry> glyphs_{};
        uint64_t touched_{};
        auto Entry(int font, int pixels, uint32_t codepoint) -> entry&;
        auto NewPage(IDirect3DDevice8* device) -> bool;
    };
}
