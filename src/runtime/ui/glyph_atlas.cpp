#include "glyph_atlas.hpp"

namespace
{
    constexpr int page_size = 512;
    constexpr size_t page_limit = 16, glyph_limit = 4096;
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

    auto glyph_atlas::Entry(const int font, const int pixels, const uint32_t codepoint) -> entry&
    {
        const uint64_t key = (static_cast<uint64_t>(FontIndex(font)) << 40) | (static_cast<uint64_t>(pixels) << 32) | codepoint;
        if (auto found = this->glyphs_.find(key); found != this->glyphs_.end())
        { found->second.touched = ++this->touched_; return found->second; }
        if (this->glyphs_.size() >= glyph_limit)
        {
            const auto oldest = std::min_element(this->glyphs_.begin(), this->glyphs_.end(), [](const auto& a, const auto& b) { return a.second.touched < b.second.touched; });
            this->glyphs_.erase(oldest);
        }
        entry value{};
        value.advance = this->rasterizer_.Advance(FontIndex(font), pixels, codepoint);
        value.touched = ++this->touched_;
        return this->glyphs_.emplace(key, value).first->second;
    }

    auto glyph_atlas::Advance(const int font, const int pixels, const uint32_t codepoint) -> float
    { return this->Entry(font, pixels, codepoint).advance; }

    auto glyph_atlas::NewPage(IDirect3DDevice8* device) -> bool
    {
        page result{};
        for (auto*& texture : result.textures)
        {
            if (FAILED(device->CreateTexture(page_size, page_size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture)))
            {
                for (auto* created : result.textures) if (created != nullptr) created->Release();
                return false;
            }
        }
        this->pages_.push_back(result);
        return true;
    }

    auto glyph_atlas::Get(IDirect3DDevice8* device, const int font, const int pixels, const uint32_t codepoint, const std::function<void()>& beforeUpload) -> atlas_glyph
    {
        auto& entry = this->Entry(font, pixels, codepoint);
        if (entry.generation != 0 && entry.generation == this->pages_[entry.page_index].generation)
        {
            this->pages_[entry.page_index].touched = ++this->touched_;
            return entry.glyph;
        }
        if (device == nullptr) return {};
        const auto raster = this->rasterizer_.Rasterize(FontIndex(font), pixels, codepoint);
        if (!raster || raster->width + 1 > page_size || raster->height + 1 > page_size) return {};

        // Finish batches before writing or reusing a texture they may reference.
        beforeUpload();
        size_t index{};
        for (; index < this->pages_.size(); ++index)
        {
            auto& page = this->pages_[index];
            const int rowY = page.x + raster->width + 1 <= page_size ? page.y : page.y + page.row_height;
            if (rowY + raster->height + 1 <= page_size) break;
        }
        if (index == this->pages_.size())
        {
            if (this->pages_.size() < page_limit)
            { if (!this->NewPage(device)) return {}; }
            else
            {
                index = static_cast<size_t>(std::min_element(this->pages_.begin(), this->pages_.end(), [](const auto& a, const auto& b) { return a.touched < b.touched; }) - this->pages_.begin());
                auto& page = this->pages_[index];
                page.x = page.y = page.row_height = 0;
                ++page.generation;
            }
        }
        auto& page = this->pages_[index];
        if (page.x + raster->width + 1 > page_size)
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
        entry.glyph = {page.textures, page.x / static_cast<float>(page_size), page.y / static_cast<float>(page_size),
            region.right / static_cast<float>(page_size), region.bottom / static_cast<float>(page_size),
            static_cast<float>(raster->width), static_cast<float>(raster->height), static_cast<float>(raster->padding), raster->ascent};
        page.x += raster->width + 1;
        page.row_height = std::max(page.row_height, raster->height + 1);
        page.touched = ++this->touched_;
        return entry.glyph;
    }
}
