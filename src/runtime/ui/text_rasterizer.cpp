#include "text_rasterizer.hpp"
#include "text_size.hpp"

#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>

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
    auto NativeFontMetrics(const int font, const int pixels, const font_variant variant) -> raster_font_metrics
    {
        static std::mutex lock{};
        static std::unordered_map<uint32_t, raster_font_metrics> cache{};
        const int family = FontIndex(font), size = std::clamp(pixels, 6, 96);
        const auto key = static_cast<uint32_t>((family * font_variant_count + variant.Key()) * 128 + size);
        { const std::lock_guard guard{lock}; if (const auto found = cache.find(key); found != cache.end()) return found->second; }
        text_rasterizer rasterizer;
        const auto metrics = rasterizer.Metrics(family, size, variant);
        const std::lock_guard guard{lock};
        return cache.emplace(key, metrics).first->second;
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
        this->grammara_.Clear(); this->highwind_.Clear();
    }

    auto text_rasterizer::Face(const int font, const int pixels, const font_variant variant) -> face*
    {
        const bool fallback = font == cjk_fallback_font;
        const int family = fallback ? cjk_fallback_font : FontIndex(font), size = std::clamp(pixels, 1, text_max_raster_pixels);
        const auto key = static_cast<uint32_t>((family * font_variant_count + variant.Key()) * 1024 + size);
        if (auto found = this->faces_.find(key); found != this->faces_.end())
        { found->second.touched = ++this->touched_; return found->second.handle != nullptr ? &found->second : nullptr; }
        if (this->dc_ == nullptr) this->dc_ = ::CreateCompatibleDC(nullptr);
        if (this->dc_ == nullptr || (family == grammara_font && !this->grammara_.Load())) return nullptr;
        const auto* entry = fallback ? nullptr : &FontEntry(family);
        if (!fallback && !entry->builtin && ::_stricmp(entry->family.c_str(), highwind_family) == 0) this->highwind_.Load(highwind_resource);
        const wchar_t* name = fallback ? L"Yu Gothic UI" : entry->wide_family.c_str();
        const auto weight = variant.weight;
        const int faceWeight = weight == font_weight::medium ? FW_MEDIUM : weight == font_weight::regular ? FW_NORMAL : weight == font_weight::bold ? FW_BOLD :
            fallback ? FW_BOLD : entry->weight;
        const bool italic = variant.italic || (!fallback && entry->italic);
        // Installed fonts may have non-Western names or glyphs; built-ins keep their established mapping.
        const auto create = [&](const wchar_t* face) {
            return ::CreateFontW(-size * samples, 0, 0, 0, faceWeight, italic ? TRUE : FALSE, FALSE, FALSE,
                fallback || !entry->builtin ? DEFAULT_CHARSET : ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                NONANTIALIASED_QUALITY, DEFAULT_PITCH, face);
        };
        auto handle = create(name);
        if (handle == nullptr) return nullptr;
        const auto previous = ::SelectObject(this->dc_, handle);
        wchar_t selected[LF_FACESIZE]{}; ::GetTextFaceW(this->dc_, LF_FACESIZE, selected);
        // Wine lacks the Windows faces unless the player copies them in. Prefer a face it ships over GDI's
        // arbitrary pick; the HUD still draws with whatever GDI chose when neither is present.
        if (!fallback && entry->builtin && family != grammara_font && ::_wcsicmp(selected, name) != 0)
            for (const auto* substitute : {L"Tahoma", L"Arial"})
            {
                const auto candidate = create(substitute);
                if (candidate == nullptr) continue;
                ::SelectObject(this->dc_, candidate);
                wchar_t chosen[LF_FACESIZE]{}; ::GetTextFaceW(this->dc_, LF_FACESIZE, chosen);
                if (::_wcsicmp(chosen, substitute) == 0) { ::DeleteObject(handle); handle = candidate; break; }
                ::SelectObject(this->dc_, handle); ::DeleteObject(candidate);
            }
        TEXTMETRICW metrics{}; ::GetTextMetricsW(this->dc_, &metrics);
        // Static families may map Medium back to Regular; strengthen their supersampled coverage.
        const int embolden = weight == font_weight::medium && metrics.tmWeight < FW_MEDIUM ?
            std::max(1, static_cast<int>(std::lround(size / 15.0f))) : 0;
        raster_font_metrics result{metrics.tmHeight / static_cast<float>(samples), metrics.tmAscent / static_cast<float>(samples), 0,
            ::_wcsicmp(selected, name) == 0, true};
        for (wchar_t digit = L'0'; digit <= L'9'; ++digit)
        {
            SIZE extent{}; ::GetTextExtentPoint32W(this->dc_, &digit, 1, &extent);
            result.digit_advance = std::max(result.digit_advance, extent.cx / static_cast<float>(samples));
        }
        ::SelectObject(this->dc_, previous);
        result.digit_advance += static_cast<float>(embolden) / samples;
        // GDI substitutes missing faces; Grammara and installed choices fall back to XIUI+ text instead.
        // A missing family is remembered, or every draw and measure would create the font again.
        const bool missing = (family == grammara_font || (!fallback && !entry->builtin)) && !result.available;
        if (missing) { ::DeleteObject(handle); handle = nullptr; }
        int shift{};
        float extent = result.height;
        // HUD layouts are tuned to the XIUI+ faces. Installed fonts take Bahnschrift's line box and baseline at
        // this size, so tall accent or descender metrics change only the glyphs, never module heights or text rows.
        if (!missing && !fallback && !entry->builtin)
            if (const auto* reference = this->Face(0, pixels, variant))
            {
                shift = static_cast<int>(std::lround((reference->metrics.ascent - result.ascent) * samples));
                extent = shift / static_cast<float>(samples) + result.height;
                result.height = reference->metrics.height; result.ascent = reference->metrics.ascent;
            }
        if (this->faces_.size() >= 256)
        {
            const auto oldest = std::min_element(this->faces_.begin(), this->faces_.end(), [](const auto& a, const auto& b) { return a.second.touched < b.second.touched; });
            ::DeleteObject(oldest->second.handle); this->faces_.erase(oldest);
        }
        auto& created = this->faces_.emplace(key, face{handle, result, ++this->touched_, embolden, shift, extent}).first->second;
        return missing ? nullptr : &created;
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

    auto text_rasterizer::Metrics(const int font, const int pixels, const font_variant variant) -> raster_font_metrics
    {
        const auto* face = this->Face(font, pixels, variant);
        if (face == nullptr && font != 0)
        { auto fallback = this->Metrics(0, pixels, variant); fallback.available = false; return fallback; }
        return face != nullptr ? face->metrics : raster_font_metrics{pixels * 1.2f, static_cast<float>(pixels), pixels * 0.6f, false};
    }

    auto text_rasterizer::Advance(const int font, const int pixels, const uint32_t codepoint, const font_variant variant) -> float
    {
        auto* face = this->GlyphFace(font, pixels, codepoint, variant);
        if (face == nullptr && font != 0) return this->Advance(0, pixels, codepoint, variant);
        if (face == nullptr) return pixels * 0.6f;
        const auto previous = ::SelectObject(this->dc_, face->handle);
        std::array<wchar_t, 2> text{}; const auto length = Text(codepoint, text);
        SIZE extent{}; ::GetTextExtentPoint32W(this->dc_, text.data(), length, &extent);
        ::SelectObject(this->dc_, previous);
        return (extent.cx + static_cast<float>(face->embolden)) / samples;
    }

    auto text_rasterizer::Rasterize(const int font, const int pixels, const uint32_t codepoint, const font_variant variant) -> std::optional<raster_glyph>
    {
        auto* face = this->GlyphFace(font, pixels, codepoint, variant);
        if (face == nullptr && font != 0) return this->Rasterize(0, pixels, codepoint, variant);
        if (face == nullptr) return std::nullopt;
        raster_glyph result;
        result.advance = this->Advance(font, pixels, codepoint, variant);
        result.ascent = face->metrics.ascent;
        result.padding = static_cast<int>(std::ceil(pixels / 15.0f * 5)) + 2;
        // Fonts drawn above the shared baseline need room for their tallest glyphs.
        if (face->shift < 0) result.padding = std::max(result.padding, static_cast<int>(std::ceil(-face->shift / static_cast<float>(samples))) + 1);
        result.width = static_cast<int>(std::ceil(result.advance)) + result.padding * 2 + 1;
        result.height = static_cast<int>(std::ceil(std::max(face->metrics.height, face->extent))) + result.padding * 2;
        if (!this->Surface(result.width * samples, result.height * samples)) return std::nullopt;
        const auto previous = ::SelectObject(this->dc_, face->handle);
        for (int y = 0; y < result.height * samples; ++y)
            std::memset(this->pixels_ + y * this->width_, 0, result.width * samples * sizeof(uint32_t));
        ::SetBkMode(this->dc_, TRANSPARENT); ::SetTextColor(this->dc_, RGB(255, 255, 255)); ::SetTextAlign(this->dc_, TA_LEFT | TA_TOP);
        std::array<wchar_t, 2> text{}; const auto length = Text(codepoint, text);
        ::TextOutW(this->dc_, result.padding * samples, result.padding * samples + face->shift, text.data(), length);
        ::GdiFlush(); ::SelectObject(this->dc_, previous);
        std::vector<uint8_t> coverage;
        const int sampleWidth = result.width * samples, sampleHeight = result.height * samples;
        if (face->embolden > 0)
        {
            coverage.resize(static_cast<size_t>(sampleWidth) * sampleHeight);
            for (int y = 0; y < sampleHeight; ++y) for (int x = 0; x < sampleWidth; ++x)
                coverage[y * sampleWidth + x] = static_cast<uint8_t>(this->pixels_[y * this->width_ + x] & 255);
            const auto expanded = Dilate(coverage, sampleWidth, sampleHeight, face->embolden);
            for (size_t i = 0; i < coverage.size(); ++i)
                coverage[i] = static_cast<uint8_t>((coverage[i] + expanded[i] + 1) / 2);
        }
        auto& base = result.masks[0]; base.resize(static_cast<size_t>(result.width) * result.height);
        for (int y = 0; y < result.height; ++y) for (int x = 0; x < result.width; ++x)
        {
            unsigned sum{};
            for (int sy = 0; sy < samples; ++sy) for (int sx = 0; sx < samples; ++sx)
                sum += coverage.empty() ? this->pixels_[(y * samples + sy) * this->width_ + x * samples + sx] & 255 :
                    coverage[(y * samples + sy) * sampleWidth + x * samples + sx];
            base[y * result.width + x] = static_cast<uint8_t>((sum + samples * samples / 2) / (samples * samples));
        }
        result.masks[1] = Dilate(base, result.width, result.height, std::max(1, static_cast<int>(std::lround(pixels / 15.0f))));
        const int radius = std::max(1, static_cast<int>(std::ceil(pixels / 15.0f * 2)));
        result.masks[2] = Blur(Blur(result.masks[1], result.width, result.height, radius), result.width, result.height, radius);
        return result;
    }

    auto text_rasterizer::GlyphFace(const int font, const int pixels, const uint32_t codepoint, const font_variant variant) -> face*
    {
        auto* selected = this->Face(font, pixels, variant);
        if (selected == nullptr || codepoint <= 255) return selected;
        std::array<wchar_t, 2> text{};
        const auto length = Text(codepoint, text);
        std::array<WORD, 2> glyphs{};
        const auto previous = ::SelectObject(this->dc_, selected->handle);
        const auto found = ::GetGlyphIndicesW(this->dc_, text.data(), length, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS);
        ::SelectObject(this->dc_, previous);
        if (found != GDI_ERROR && std::all_of(glyphs.begin(), glyphs.begin() + length, [](WORD glyph) { return glyph != 0xFFFF; })) return selected;
        if (auto* fallback = this->Face(cjk_fallback_font, pixels, variant)) return fallback;
        return selected;
    }
}
