#include "text_rasterizer.hpp"
#include "text_size.hpp"

#include <cmath>
#include <cstring>

namespace
{
    constexpr int samples = 4;

    auto Text(const uint32_t codepoint, std::array<wchar_t, 2>& text) -> int
    {
        if (codepoint > 0xFFFF && codepoint <= 0x10FFFF)
        {
            text[0] = static_cast<wchar_t>(0xD800 + ((codepoint - 0x10000) >> 10));
            text[1] = static_cast<wchar_t>(0xDC00 + ((codepoint - 0x10000) & 0x3FF));
            return 2;
        }
        text[0] = static_cast<wchar_t>(codepoint <= 0xFFFF ? codepoint : '?');
        return 1;
    }

    auto Dilate(const std::vector<uint8_t>& source, const int width, const int height, const int radius) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> first(source.size()), result(source.size());
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            for (int dx = std::max(0, x - radius); dx <= std::min(width - 1, x + radius); ++dx)
                first[y * width + x] = std::max(first[y * width + x], source[y * width + dx]);
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            for (int dy = std::max(0, y - radius); dy <= std::min(height - 1, y + radius); ++dy)
                result[y * width + x] = std::max(result[y * width + x], first[dy * width + x]);
        return result;
    }

    auto Blur(const std::vector<uint8_t>& source, const int width, const int height, const int radius) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> first(source.size()), result(source.size());
        const int divisor = radius * 2 + 1;
        for (int y = 0; y < height; ++y)
        {
            unsigned sum{};
            for (int x = 0; x < std::min(width, radius); ++x) sum += source[y * width + x];
            for (int x = 0; x < width; ++x)
            {
                if (x + radius < width) sum += source[y * width + x + radius];
                if (x - radius - 1 >= 0) sum -= source[y * width + x - radius - 1];
                first[y * width + x] = static_cast<uint8_t>(sum / divisor);
            }
        }
        for (int x = 0; x < width; ++x)
        {
            unsigned sum{};
            for (int y = 0; y < std::min(height, radius); ++y) sum += first[y * width + x];
            for (int y = 0; y < height; ++y)
            {
                if (y + radius < height) sum += first[(y + radius) * width + x];
                if (y - radius - 1 >= 0) sum -= first[(y - radius - 1) * width + x];
                result[y * width + x] = static_cast<uint8_t>(sum / divisor);
            }
        }
        return result;
    }
}

namespace xiui::ui
{
    auto NativeFontMetrics(const int font, const int pixels, const font_weight weight) -> raster_font_metrics
    {
        static std::array<std::optional<raster_font_metrics>, font_count * 91 * 3> cache{};
        const int family = FontIndex(font), size = std::clamp(pixels, 6, 96);
        auto& metrics = cache[(family * 3 + static_cast<int>(weight)) * 91 + size - 6];
        if (!metrics) { text_rasterizer rasterizer; metrics = rasterizer.Metrics(family, size, weight); }
        return *metrics;
    }

    auto text_rasterizer::Clear(void) -> void
    {
        for (const auto& [key, face] : this->faces_) { (void)key; if (face.handle != nullptr) ::DeleteObject(face.handle); }
        this->faces_.clear();
        if (this->dc_ != nullptr)
        {
            if (this->bitmap_ != nullptr) { ::SelectObject(this->dc_, this->old_bitmap_); ::DeleteObject(this->bitmap_); }
            ::DeleteDC(this->dc_);
        }
        this->dc_ = nullptr; this->bitmap_ = nullptr; this->pixels_ = nullptr; this->width_ = this->height_ = 0;
        this->grammara_.Clear();
    }

    auto text_rasterizer::Face(const int font, const int pixels, const font_weight weight) -> face*
    {
        const int family = std::clamp(font, 0, font_count), size = std::clamp(pixels, 1, text_max_raster_pixels);
        const auto key = static_cast<uint32_t>((family * 3 + static_cast<int>(weight)) * 1024 + size);
        if (auto found = this->faces_.find(key); found != this->faces_.end()) { found->second.touched = ++this->touched_; return &found->second; }
        if (this->dc_ == nullptr) this->dc_ = ::CreateCompatibleDC(nullptr);
        if (this->dc_ == nullptr || (family == 3 && !this->grammara_.Load())) return nullptr;
        const auto name = family == font_count ? "Yu Gothic UI" : font_faces[family].family;
        const int faceWeight = weight == font_weight::regular ? FW_NORMAL : weight == font_weight::bold ? FW_BOLD :
            family == font_count ? FW_BOLD : font_faces[family].weight;
        const bool italic = family < font_count && font_faces[family].italic;
        auto handle = ::CreateFontA(-size * samples, 0, 0, 0, faceWeight, italic ? TRUE : FALSE, FALSE, FALSE,
            family == font_count ? DEFAULT_CHARSET : ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, DEFAULT_PITCH, name);
        if (handle == nullptr) return nullptr;
        const auto previous = ::SelectObject(this->dc_, handle);
        char selected[LF_FACESIZE]{}; ::GetTextFaceA(this->dc_, LF_FACESIZE, selected);
        TEXTMETRICA metrics{}; ::GetTextMetricsA(this->dc_, &metrics);
        raster_font_metrics result{metrics.tmHeight / static_cast<float>(samples), metrics.tmAscent / static_cast<float>(samples), 0,
            ::_stricmp(selected, name) == 0};
        for (wchar_t digit = L'0'; digit <= L'9'; ++digit)
        {
            SIZE extent{}; ::GetTextExtentPoint32W(this->dc_, &digit, 1, &extent);
            result.digit_advance = std::max(result.digit_advance, extent.cx / static_cast<float>(samples));
        }
        ::SelectObject(this->dc_, previous);
        if (family == 3 && !result.available) { ::DeleteObject(handle); return nullptr; }
        if (this->faces_.size() >= 64)
        {
            const auto oldest = std::min_element(this->faces_.begin(), this->faces_.end(), [](const auto& a, const auto& b) { return a.second.touched < b.second.touched; });
            ::DeleteObject(oldest->second.handle); this->faces_.erase(oldest);
        }
        return &this->faces_.emplace(key, face{handle, result, ++this->touched_}).first->second;
    }

    auto text_rasterizer::Surface(const int width, const int height) -> bool
    {
        if (this->bitmap_ != nullptr && width <= this->width_ && height <= this->height_) return true;
        const int nextWidth = std::max(width, this->width_), nextHeight = std::max(height, this->height_);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), nextWidth, -nextHeight, 1, 32, BI_RGB};
        void* pixels{};
        auto bitmap = ::CreateDIBSection(this->dc_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (bitmap == nullptr || pixels == nullptr) { if (bitmap != nullptr) ::DeleteObject(bitmap); return false; }
        const auto previous = ::SelectObject(this->dc_, bitmap);
        if (this->bitmap_ != nullptr) ::DeleteObject(this->bitmap_);
        else this->old_bitmap_ = previous;
        this->bitmap_ = bitmap; this->pixels_ = static_cast<uint32_t*>(pixels); this->width_ = nextWidth; this->height_ = nextHeight;
        return true;
    }

    auto text_rasterizer::Metrics(const int font, const int pixels, const font_weight weight) -> raster_font_metrics
    {
        const auto* face = this->Face(font, pixels, weight);
        if (face == nullptr && font != 0)
        { auto fallback = this->Metrics(0, pixels, weight); fallback.available = false; return fallback; }
        return face != nullptr ? face->metrics : raster_font_metrics{pixels * 1.2f, static_cast<float>(pixels), pixels * 0.6f, false};
    }

    auto text_rasterizer::Advance(const int font, const int pixels, const uint32_t codepoint, const font_weight weight) -> float
    {
        auto* face = this->GlyphFace(font, pixels, codepoint, weight);
        if (face == nullptr && font != 0) return this->Advance(0, pixels, codepoint, weight);
        if (face == nullptr) return pixels * 0.6f;
        const auto previous = ::SelectObject(this->dc_, face->handle);
        std::array<wchar_t, 2> text{}; const auto length = Text(codepoint, text);
        SIZE extent{}; ::GetTextExtentPoint32W(this->dc_, text.data(), length, &extent);
        ::SelectObject(this->dc_, previous);
        return extent.cx / static_cast<float>(samples);
    }

    auto text_rasterizer::Rasterize(const int font, const int pixels, const uint32_t codepoint, const font_weight weight) -> std::optional<raster_glyph>
    {
        auto* face = this->GlyphFace(font, pixels, codepoint, weight);
        if (face == nullptr && font != 0) return this->Rasterize(0, pixels, codepoint, weight);
        if (face == nullptr) return std::nullopt;
        raster_glyph result;
        result.advance = this->Advance(font, pixels, codepoint, weight);
        result.ascent = face->metrics.ascent;
        result.padding = static_cast<int>(std::ceil(pixels / 15.0f * 5)) + 2;
        result.width = static_cast<int>(std::ceil(result.advance)) + result.padding * 2 + 1;
        result.height = static_cast<int>(std::ceil(face->metrics.height)) + result.padding * 2;
        if (!this->Surface(result.width * samples, result.height * samples)) return std::nullopt;
        const auto previous = ::SelectObject(this->dc_, face->handle);
        for (int y = 0; y < result.height * samples; ++y)
            std::memset(this->pixels_ + y * this->width_, 0, result.width * samples * sizeof(uint32_t));
        ::SetBkMode(this->dc_, TRANSPARENT); ::SetTextColor(this->dc_, RGB(255, 255, 255)); ::SetTextAlign(this->dc_, TA_LEFT | TA_TOP);
        std::array<wchar_t, 2> text{}; const auto length = Text(codepoint, text);
        ::TextOutW(this->dc_, result.padding * samples, result.padding * samples, text.data(), length);
        ::GdiFlush(); ::SelectObject(this->dc_, previous);
        auto& base = result.masks[0]; base.resize(static_cast<size_t>(result.width) * result.height);
        for (int y = 0; y < result.height; ++y) for (int x = 0; x < result.width; ++x)
        {
            unsigned sum{};
            for (int sy = 0; sy < samples; ++sy) for (int sx = 0; sx < samples; ++sx)
                sum += this->pixels_[(y * samples + sy) * this->width_ + x * samples + sx] & 255;
            base[y * result.width + x] = static_cast<uint8_t>((sum + samples * samples / 2) / (samples * samples));
        }
        result.masks[1] = Dilate(base, result.width, result.height, std::max(1, static_cast<int>(std::lround(pixels / 15.0f))));
        const int radius = std::max(1, static_cast<int>(std::ceil(pixels / 15.0f * 2)));
        result.masks[2] = Blur(Blur(result.masks[1], result.width, result.height, radius), result.width, result.height, radius);
        return result;
    }

    auto text_rasterizer::GlyphFace(const int font, const int pixels, const uint32_t codepoint, const font_weight weight) -> face*
    {
        auto* selected = this->Face(font, pixels, weight);
        if (selected == nullptr || codepoint <= 255) return selected;
        std::array<wchar_t, 2> text{};
        const auto length = Text(codepoint, text);
        std::array<WORD, 2> glyphs{};
        const auto previous = ::SelectObject(this->dc_, selected->handle);
        const auto found = ::GetGlyphIndicesW(this->dc_, text.data(), length, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS);
        ::SelectObject(this->dc_, previous);
        if (found != GDI_ERROR && std::all_of(glyphs.begin(), glyphs.begin() + length, [](WORD glyph) { return glyph != 0xFFFF; })) return selected;
        if (auto* fallback = this->Face(font_count, pixels, weight)) return fallback;
        return selected;
    }
}
