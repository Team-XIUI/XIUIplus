#include "icon_atlas.hpp"
#include "design_system.hpp"
#include "resources/github_icon.hpp"
#include "resources/target_arrow.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <span>

namespace xiui::ui
{
    namespace
    {
        constexpr int cell = 32, columns = 32, supersample = 4;
        constexpr int rows = (static_cast<int>(icon_glyph::unknown) + columns) / columns;

        class icon_path
        {
        public:
            icon_path(HDC dc, int index) : dc_(dc), offset_x_((index % columns) * cell * supersample), offset_y_((index / columns) * cell * supersample) {}
            auto Begin(float x, float y) const -> void { ::BeginPath(this->dc_); ::MoveToEx(this->dc_, X(x), Y(y), nullptr); }
            auto Line(float x, float y) const -> void { ::LineTo(this->dc_, X(x), Y(y)); }
            auto Curve(float x1, float y1, float x2, float y2, float x, float y) const -> void
            {
                const POINT points[]{{X(x1), Y(y1)}, {X(x2), Y(y2)}, {X(x), Y(y)}};
                ::PolyBezierTo(this->dc_, points, 3);
            }
            auto End(bool close = false) const -> void
            {
                if (close) ::CloseFigure(this->dc_);
                ::EndPath(this->dc_); ::StrokePath(this->dc_);
            }
            auto Circle(float x, float y, float radius) const -> void
            { ::Ellipse(this->dc_, X(x - radius), Y(y - radius), X(x + radius), Y(y + radius)); }
            auto RoundRect(float x, float y, float width, float height, float radius = 2) const -> void
            { ::RoundRect(this->dc_, X(x), Y(y), X(x + width), Y(y + height), Scale(radius * 2), Scale(radius * 2)); }

        private:
            HDC dc_{};
            int offset_x_{}, offset_y_{};
            auto X(float value) const -> int { return this->offset_x_ + Scale(value); }
            auto Y(float value) const -> int { return this->offset_y_ + Scale(value); }
            static auto Scale(float value) -> int { return static_cast<int>(std::lround(value * cell * supersample / 24.0f)); }
        };

        auto DrawIcon(const icon_path& p, const icon_glyph icon) -> void
        {
            switch (icon)
            {
            case icon_glyph::player:
                p.Circle(12, 7, 3.5f);
                p.Begin(4, 21); p.Line(4, 19); p.Curve(4, 15.5f, 7.5f, 14, 12, 14); p.Curve(16.5f, 14, 20, 15.5f, 20, 19); p.Line(20, 21); p.End(); break;
            case icon_glyph::target:
                p.Circle(12, 12, 7); p.Circle(12, 12, 2);
                p.Begin(12, 2); p.Line(12, 6); p.End(); p.Begin(12, 18); p.Line(12, 22); p.End();
                p.Begin(2, 12); p.Line(6, 12); p.End(); p.Begin(18, 12); p.Line(22, 12); p.End(); break;
            case icon_glyph::enemies:
                for (int row = 0; row < 3; ++row)
                {
                    const float y = 5.0f + row * 7;
                    p.Circle(4, y, 1.5f); p.Begin(9, y); p.Line(21 - row * 2.0f, y); p.End();
                }
                break;
            case icon_glyph::experience:
                p.Begin(3, 3); p.Line(3, 21); p.Line(21, 21); p.End();
                p.Begin(7, 15); p.Line(12, 10); p.Line(16, 13); p.Line(21, 6); p.End();
                p.Begin(16, 6); p.Line(21, 6); p.Line(21, 11); p.End(); break;
            case icon_glyph::notifications:
                p.Begin(5, 17); p.Curve(6.5f, 15, 6.5f, 13, 6.5f, 9); p.Curve(6.5f, 2, 17.5f, 2, 17.5f, 9);
                p.Curve(17.5f, 13, 17.5f, 15, 19, 17); p.End(true);
                p.Begin(10, 21); p.Curve(11, 22, 13, 22, 14, 21); p.End(); break;
            case icon_glyph::container:
                p.Begin(12, 2); p.Line(21, 7); p.Line(21, 17); p.Line(12, 22); p.Line(3, 17); p.Line(3, 7); p.End(true);
                p.Begin(3, 7); p.Line(12, 12); p.Line(21, 7); p.End(); p.Begin(12, 12); p.Line(12, 22); p.End();
                p.Begin(7.5f, 4.5f); p.Line(16.5f, 9.5f); p.End(); break;
            case icon_glyph::gamehud:
                p.RoundRect(3, 3, 18, 18); p.Begin(3, 9); p.Line(21, 9); p.End();
                p.Begin(9, 9); p.Line(9, 21); p.End(); p.Begin(13, 13); p.Line(17, 13); p.End();
                p.Begin(13, 17); p.Line(17, 17); p.End(); break;
            case icon_glyph::chat:
                p.Begin(21, 14); p.Curve(21, 16, 20, 17, 18, 17); p.Line(8, 17); p.Line(3, 21); p.Line(3, 6);
                p.Curve(3, 4, 4, 3, 6, 3); p.Line(18, 3); p.Curve(20, 3, 21, 4, 21, 6); p.End(true);
                p.Begin(7, 8); p.Line(17, 8); p.End(); p.Begin(7, 12); p.Line(14, 12); p.End(); break;
            case icon_glyph::chat2:
                p.Begin(8, 15); p.Line(6, 15); p.Line(2, 18); p.Line(2, 5); p.Curve(2, 3, 3, 2, 5, 2); p.Line(15, 2);
                p.Curve(17, 2, 18, 3, 18, 5); p.End();
                p.Begin(22, 12); p.Line(22, 22); p.Line(18, 19); p.Line(12, 19); p.Curve(10, 19, 9, 18, 9, 16); p.Line(9, 12);
                p.Curve(9, 10, 10, 9, 12, 9); p.Line(19, 9); p.Curve(21, 9, 22, 10, 22, 12); p.End(true); break;
            case icon_glyph::compass:
                p.Circle(12, 12, 10); p.Begin(16, 8); p.Line(14, 14); p.Line(8, 16); p.Line(10, 10); p.End(true);
                p.Begin(10, 10); p.Line(14, 14); p.End(); break;
            case icon_glyph::world:
                p.Circle(12, 12, 9); p.Begin(12, 6); p.Line(12, 12); p.Line(16, 14); p.End(); break;
            case icon_glyph::commands:
                p.RoundRect(2, 3, 20, 18); p.Begin(6, 8); p.Line(10, 12); p.Line(6, 16); p.End();
                p.Begin(13, 16); p.Line(18, 16); p.End(); break;
            case icon_glyph::reset:
                p.Begin(4, 8); p.Curve(7, 1, 20, 3, 20, 12); p.Curve(20, 21, 7, 24, 3, 16); p.End();
                p.Begin(3, 3); p.Line(3, 9); p.Line(9, 9); p.End(); break;
            case icon_glyph::general:
                for (int row = 0; row < 3; ++row)
                {
                    const float y = 5.0f + row * 7, knob = row == 1 ? 16.0f : 8.0f;
                    p.Begin(3, y); p.Line(knob - 3, y); p.End(); p.Circle(knob, y, 2);
                    p.Begin(knob + 3, y); p.Line(21, y); p.End();
                }
                break;
            case icon_glyph::party:
                p.Circle(12, 6, 3); p.Circle(4, 9, 2); p.Circle(20, 9, 2);
                p.Begin(6, 21); p.Line(6, 17); p.Curve(6, 11, 18, 11, 18, 17); p.Line(18, 21); p.End();
                p.Begin(1, 20); p.Line(1, 16); p.Curve(1, 13, 3, 12, 5, 13); p.End();
                p.Begin(23, 20); p.Line(23, 16); p.Curve(23, 13, 21, 12, 19, 13); p.End(); break;
            case icon_glyph::inventory:
                p.RoundRect(4, 7, 16, 15, 3);
                p.Begin(8, 7); p.Line(8, 5); p.Curve(8, 1, 16, 1, 16, 5); p.Line(16, 7); p.End();
                for (int row = 0; row < 2; ++row)
                    for (int column = 0; column < 3; ++column) p.Circle(8.0f + column * 4, 12.0f + row * 5, 0.7f);
                break;
            case icon_glyph::equipment:
                p.Begin(8, 3); p.Curve(9, 7, 15, 7, 16, 3); p.Line(22, 7); p.Line(19, 12); p.Line(17, 10);
                p.Line(17, 21); p.Line(7, 21); p.Line(7, 10); p.Line(5, 12); p.Line(2, 7); p.End(true); break;
            case icon_glyph::extras:
                p.RoundRect(3, 3, 7, 7); p.RoundRect(14, 3, 7, 7); p.RoundRect(3, 14, 7, 7);
                p.Begin(14, 17.5f); p.Line(21, 17.5f); p.End(); p.Begin(17.5f, 14); p.Line(17.5f, 21); p.End(); break;
            case icon_glyph::readycheck:
                p.RoundRect(4, 3, 16, 19, 2); p.Begin(8, 3); p.Line(8, 1); p.Line(16, 1); p.Line(16, 5); p.Line(8, 5); p.End(true);
                p.Begin(7, 13); p.Line(10, 16); p.Line(17, 9); p.End(); break;
            case icon_glyph::castbar:
                p.Begin(12, 2); p.Curve(12.5f, 4.5f, 13.5f, 5.5f, 16, 6);
                p.Curve(13.5f, 6.5f, 12.5f, 7.5f, 12, 10); p.Curve(11.5f, 7.5f, 10.5f, 6.5f, 8, 6);
                p.Curve(10.5f, 5.5f, 11.5f, 4.5f, 12, 2); p.End();
                p.RoundRect(3, 14, 18, 7, 2); p.Begin(6, 17.5f); p.Line(12, 17.5f); p.End(); break;
            case icon_glyph::castcost:
                p.Begin(12, 2); p.Curve(10, 5, 5, 10, 5, 14); p.Curve(5, 18, 8, 21, 12, 21);
                p.Curve(16, 21, 19, 18, 19, 14); p.Curve(19, 10, 14, 5, 12, 2); p.End();
                p.Begin(9, 14); p.Line(15, 14); p.End(); break;
            case icon_glyph::gil:
                p.Circle(12, 12, 9); p.Circle(12, 12, 6);
                p.Begin(14, 8); p.Line(10, 8); p.Line(10, 16); p.Line(14, 16); p.Line(14, 12); p.Line(12, 12); p.End(); break;
            case icon_glyph::chevron_down:
                p.Begin(5, 8); p.Line(12, 15); p.Line(19, 8); p.End(); break;
            case icon_glyph::chevron_up:
                p.Begin(5, 16); p.Line(12, 9); p.Line(19, 16); p.End(); break;
            case icon_glyph::arrow_left:
                p.Begin(19, 12); p.Line(5, 12); p.End();
                p.Begin(11, 6); p.Line(5, 12); p.Line(11, 18); p.End(); break;
            case icon_glyph::arrow_right:
                p.Begin(5, 12); p.Line(19, 12); p.End();
                p.Begin(13, 6); p.Line(19, 12); p.Line(13, 18); p.End(); break;
            case icon_glyph::add:
                p.Begin(12, 4); p.Line(12, 20); p.End(); p.Begin(4, 12); p.Line(20, 12); p.End(); break;
            case icon_glyph::close:
                p.Begin(5, 5); p.Line(19, 19); p.End(); p.Begin(19, 5); p.Line(5, 19); p.End(); break;
            case icon_glyph::more:
                p.Circle(5, 12, 1); p.Circle(12, 12, 1); p.Circle(19, 12, 1); break;
            case icon_glyph::play:
                p.Begin(7, 4); p.Line(20, 12); p.Line(7, 20); p.End(true); break;
            case icon_glyph::attribution:
                p.Circle(12, 12, 10); p.Circle(12, 7, 0.7f);
                p.Begin(10, 11); p.Line(12, 11); p.Line(12, 18); p.Line(15, 18); p.End(); break;
            case icon_glyph::eye: case icon_glyph::eye_closed:
                p.Begin(2, 12); p.Curve(7, 3, 17, 3, 22, 12); p.Curve(17, 21, 7, 21, 2, 12); p.End(true);
                p.Circle(12, 12, 3);
                if (icon == icon_glyph::eye_closed) { p.Begin(3, 3); p.Line(21, 21); p.End(); }
                break;
            case icon_glyph::github: case icon_glyph::target_arrow: break;
            default: p.RoundRect(3, 3, 18, 18); break;
            }
        }
    }

    auto icon_atlas::Shutdown(void) -> void
    {
        if (this->texture_ != nullptr) this->texture_->Release();
        this->texture_ = nullptr;
        this->device_ = nullptr;
    }

    auto icon_atlas::SetDevice(IDirect3DDevice8* device) -> void
    {
        if (this->device_ == device) return;
        this->Shutdown();
        this->device_ = device;
        if (device == nullptr) return;
        constexpr int width = columns * cell, height = rows * cell;
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), width * supersample, -height * supersample, 1, 32, BI_RGB};
        void* bits{};
        const HDC dc = ::CreateCompatibleDC(nullptr);
        const HBITMAP bitmap = ::CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        const LOGBRUSH brush{BS_SOLID, RGB(255, 255, 255), 0};
        const HPEN pen = ::ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, 2 * cell * supersample / 24, &brush, 0, nullptr);
        if (dc == nullptr || bitmap == nullptr || bits == nullptr || pen == nullptr)
        {
            if (pen != nullptr) ::DeleteObject(pen);
            if (bitmap != nullptr) ::DeleteObject(bitmap);
            if (dc != nullptr) ::DeleteDC(dc);
            return;
        }
        const auto previousBitmap = ::SelectObject(dc, bitmap), previousPen = ::SelectObject(dc, pen);
        const auto previousBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        std::memset(bits, 0, width * height * supersample * supersample * sizeof(uint32_t));
        for (size_t i = 0; i <= static_cast<size_t>(icon_glyph::unknown); ++i) DrawIcon({dc, static_cast<int>(i)}, static_cast<icon_glyph>(i));
        ::GdiFlush();
        auto* maskPixels = static_cast<uint32_t*>(bits);
        const auto bake = [&](const std::span<const uint32_t> runs, const icon_glyph glyph) {
            size_t maskPosition{};
            for (const auto run : runs)
            {
                const uint32_t value = (run & 0xFF) * 0x010101;
                for (uint32_t count = 0; count < (run >> 8) && maskPosition < cell * cell * supersample * supersample; ++count, ++maskPosition)
                {
                    const auto x = maskPosition % (cell * supersample), y = maskPosition / (cell * supersample);
                    const auto originX = (static_cast<size_t>(glyph) % columns) * cell * supersample;
                    const auto originY = (static_cast<size_t>(glyph) / columns) * cell * supersample;
                    maskPixels[(originY + y) * width * supersample + originX + x] = value;
                }
            }
        };
        bake(resources::github_icon_runs, icon_glyph::github);
        bake(resources::target_arrow_runs, icon_glyph::target_arrow);
        std::vector<uint32_t> pixels(width * height);
        const auto* source = static_cast<const uint32_t*>(bits);
        for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            unsigned coverage{};
            for (int sy = 0; sy < supersample; ++sy)
            for (int sx = 0; sx < supersample; ++sx) coverage += source[(y * supersample + sy) * width * supersample + x * supersample + sx] & 255;
            pixels[y * width + x] = 0x00FFFFFF | ((coverage / (supersample * supersample)) << 24);
        }
        ::SelectObject(dc, previousBrush); ::SelectObject(dc, previousPen); ::SelectObject(dc, previousBitmap);
        ::DeleteObject(pen); ::DeleteObject(bitmap); ::DeleteDC(dc);
        if (FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &this->texture_))) return;
        D3DLOCKED_RECT locked{};
        if (FAILED(this->texture_->LockRect(0, &locked, nullptr, 0))) { this->texture_->Release(); this->texture_ = nullptr; return; }
        for (int y = 0; y < height; ++y) std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch, pixels.data() + y * width, width * sizeof(uint32_t));
        this->texture_->UnlockRect(0);
    }

    auto icon_atlas::Draw(const design_system& design, const rect& bounds, const icon_glyph icon, const D3DCOLOR color, const int quarterTurns, const bool flipX) const -> void
    {
        if (this->texture_ == nullptr) return;
        const float size = std::max(0.0f, std::min(bounds.width, bounds.height));
        const rect at{std::round(bounds.x + (bounds.width - size) * 0.5f), std::round(bounds.y + (bounds.height - size) * 0.5f), size, size};
        const auto index = static_cast<int>(icon);
        rect uv{static_cast<float>(index % columns) / columns, static_cast<float>(index / columns) / rows, 1.0f / columns, 1.0f / rows};
        if (flipX) { uv.x += uv.width; uv.width = -uv.width; }
        const D3DCOLOR shadow = ((color >> 24) * 208 / 255) << 24;
        const auto draw = [&](const rect& box, const D3DCOLOR tint) {
            if ((quarterTurns & 3) == 0) { design.DrawTexture(this->texture_, box, tint, uv); return; }
            const std::array<point, 4> corners{{{box.x, box.y}, {box.x + box.width, box.y},
                {box.x + box.width, box.y + box.height}, {box.x, box.y + box.height}}};
            std::array<point, 4> rotated{};
            constexpr std::array<size_t, 4> order{0, 1, 3, 2};
            for (size_t i = 0; i < rotated.size(); ++i) rotated[i] = corners[(order[i] + quarterTurns) & 3];
            texture_style style; style.colors.fill(tint);
            design.DrawTextureQuad(this->texture_, rotated, uv, style);
        };
        draw({at.x + 1, at.y + 1, size, size}, shadow);
        draw(at, color);
    }
}
