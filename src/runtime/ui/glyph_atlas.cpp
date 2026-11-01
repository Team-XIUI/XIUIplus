#include "glyph_atlas.hpp"
#include <bit>

namespace
{
    constexpr int page_size = 512;
    constexpr size_t page_limit = 16, glyph_limit = 4096;
    constexpr size_t pixel_limit = page_limit * page_size * page_size;
}

namespace xiui::ui
{
    auto glyph_atlas::Clear(void) -> void
    {
        for (auto& page : this->pages_)
            for (auto* texture : page.textures) if (texture != nullptr) texture->Release();
        this->pages_.clear();
        this->glyphs_.clear();
        this->rasterizer_.Clear();
        this->touched_ = 0;
    }

    auto glyph_atlas::Entry(const int font, const int pixels, const uint32_t codepoint, const font_weight weight) -> entry&
    {
        const uint64_t key = (static_cast<uint64_t>(weight) << 56) | (static_cast<uint64_t>(FontIndex(font)) << 48) | (static_cast<uint64_t>(pixels) << 32) | codepoint;
        if (auto found = this->glyphs_.find(key); found != this->glyphs_.end())
        { found->second.touched = ++this->touched_; return found->second; }
        if (this->glyphs_.size() >= glyph_limit)
        {
            const auto oldest = std::min_element(this->glyphs_.begin(), this->glyphs_.end(), [](const auto& a, const auto& b) { return a.second.touched < b.second.touched; });
            this->glyphs_.erase(oldest);
        }
        entry value{};
        value.advance = this->rasterizer_.Advance(FontIndex(font), pixels, codepoint, weight);
        value.touched = ++this->touched_;
        return this->glyphs_.emplace(key, value).first->second;
    }

    auto glyph_atlas::Advance(const int font, const int pixels, const uint32_t codepoint, const font_weight weight) -> float
    { return this->Entry(font, pixels, codepoint, weight).advance; }

    auto glyph_atlas::NewPage(IDirect3DDevice8* device, const int size) -> std::optional<size_t>
    {
        size_t used{};
        for (const auto& page : this->pages_) used += page.size * page.size;
        while (used + size * size > pixel_limit)
        {
            auto oldest = this->pages_.end();
            for (auto at = this->pages_.begin(); at != this->pages_.end(); ++at)
                if (at->size && (oldest == this->pages_.end() || at->touched < oldest->touched)) oldest = at;
            if (oldest == this->pages_.end()) return std::nullopt;
            if (oldest->size == size)
            {
                oldest->x = oldest->y = oldest->row_height = 0;
                ++oldest->generation;
                return static_cast<size_t>(oldest - this->pages_.begin());
            }
            used -= oldest->size * oldest->size;
            for (auto*& texture : oldest->textures) { if (texture) texture->Release(); texture = nullptr; }
            oldest->size = oldest->x = oldest->y = oldest->row_height = 0;
            ++oldest->generation;
        }
        size_t index{};
        while (index < this->pages_.size() && this->pages_[index].size) ++index;
        if (index == this->pages_.size()) this->pages_.emplace_back();
        auto& page = this->pages_[index];
        for (auto*& texture : page.textures)
        {
            if (FAILED(device->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture)))
            {
                for (auto*& created : page.textures) { if (created) created->Release(); created = nullptr; }
                return std::nullopt;
            }
        }
        page.size = size;
        return index;
    }

    auto glyph_atlas::Get(IDirect3DDevice8* device, const int font, const int pixels, const uint32_t codepoint, const std::function<void()>& beforeUpload, const font_weight weight) -> atlas_glyph
    {
        auto& entry = this->Entry(font, pixels, codepoint, weight);
        if (entry.generation != 0 && entry.generation == this->pages_[entry.page_index].generation)
        {
            this->pages_[entry.page_index].touched = ++this->touched_;
            return entry.glyph;
        }
        if (device == nullptr) return {};
        const auto raster = this->rasterizer_.Rasterize(FontIndex(font), pixels, codepoint, weight);
        if (!raster || raster->width + 1 > 2048 || raster->height + 1 > 2048) return {};

        // Finish batches before writing or reusing a texture they may reference.
        beforeUpload();
        size_t index{};
        for (; index < this->pages_.size(); ++index)
        {
            auto& page = this->pages_[index];
            if (raster->width + 1 > page.size) continue;
            const int rowY = page.x + raster->width + 1 <= page.size ? page.y : page.y + page.row_height;
            if (rowY + raster->height + 1 <= page.size) break;
        }
        if (index == this->pages_.size())
        {
            const int size = static_cast<int>(std::bit_ceil(static_cast<unsigned>(std::max({page_size, raster->width + 1, raster->height + 1}))));
            const auto next = this->NewPage(device, size);
            if (!next) return {};
            index = *next;
        }
        auto& page = this->pages_[index];
        if (page.x + raster->width + 1 > page.size)
        { page.x = 0; page.y += page.row_height; page.row_height = 0; }
        RECT region{page.x, page.y, page.x + raster->width, page.y + raster->height};
        for (size_t mask = 0; mask < page.textures.size(); ++mask)
        {
            D3DLOCKED_RECT locked{};
            if (FAILED(page.textures[mask]->LockRect(0, &locked, &region, 0))) return {};
            for (int y = 0; y < raster->height; ++y)
            {
                auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch);
                for (int x = 0; x < raster->width; ++x)
                    row[x] = (static_cast<uint32_t>(raster->masks[mask][y * raster->width + x]) << 24) | 0xFFFFFFu;
            }
            if (FAILED(page.textures[mask]->UnlockRect(0))) return {};
        }
        entry.page_index = index; entry.generation = page.generation;
        entry.glyph = {page.textures, page.x / static_cast<float>(page.size), page.y / static_cast<float>(page.size),
            region.right / static_cast<float>(page.size), region.bottom / static_cast<float>(page.size),
            static_cast<float>(raster->width), static_cast<float>(raster->height), static_cast<float>(raster->padding), raster->ascent};
        page.x += raster->width + 1;
        page.row_height = std::max(page.row_height, raster->height + 1);
        page.touched = ++this->touched_;
        return entry.glyph;
    }
}
