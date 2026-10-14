#include "color_picker.hpp"
#include "config_controls.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr float pi = 3.14159265358979323846f;
    auto Contains(const xiui::ui::rect& r, const float x, const float y) -> bool { return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height; }
}

namespace xiui::ui
{
    auto color_picker::FromColor(const D3DCOLOR color, const float previousHue) -> hsva_color
    {
        const float r = ((color >> 16) & 255) / 255.0f, g = ((color >> 8) & 255) / 255.0f, b = (color & 255) / 255.0f;
        const float high = std::max({r, g, b}), low = std::min({r, g, b}), delta = high - low;
        float hue = previousHue;
        if (delta > 0)
        {
            hue = high == r ? (g - b) / delta : high == g ? (b - r) / delta + 2 : (r - g) / delta + 4;
            hue /= 6;
            if (hue < 0) hue += 1;
        }
        return {hue, high > 0 ? delta / high : 0, high, (color >> 24) / 255.0f};
    }

    auto color_picker::ToColor(const hsva_color& color) -> D3DCOLOR
    {
        const float hue = (color.hue - std::floor(color.hue)) * 6;
        const auto sector = static_cast<int>(std::floor(hue));
        const float s = std::clamp(color.saturation, 0.0f, 1.0f), v = std::clamp(color.value, 0.0f, 1.0f);
        const float p = v * (1 - s), q = v * (1 - s * (hue - sector)), t = v * (1 - s * (1 - (hue - sector)));
        const std::array<std::array<float, 3>, 6> rgb{{{v, t, p}, {q, v, p}, {p, v, t}, {p, q, v}, {t, p, v}, {v, p, q}}};
        const auto byte = [](const float value) { return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255)); };
        return D3DCOLOR_ARGB(byte(color.alpha), byte(rgb[sector][0]), byte(rgb[sector][1]), byte(rgb[sector][2]));
    }

    auto color_picker::Open(const D3DCOLOR color) -> void
    {
        this->original_ = color;
        this->color_ = FromColor(color);
        this->EndDrag();
    }

    auto color_picker::HitTest(const rect& panel, const float x, const float y) -> part
    {
        const color_picker_layout layout{panel};
        const auto wheel = layout.Wheel();
        const float radius = wheel.width * 0.5f, dx = x - wheel.x - radius, dy = y - wheel.y - radius;
        if (dx * dx + dy * dy <= radius * radius) return part::wheel;
        if (Contains(layout.Value(), x, y)) return part::value;
        if (Contains(layout.Alpha(), x, y)) return part::alpha;
        if (Contains(layout.Original(), x, y)) return part::restore;
        if (Contains(layout.Hex(), x, y)) return part::copy;
        if (Contains(layout.Close(), x, y)) return part::close;
        return part::none;
    }

    auto color_picker::Update(const rect& panel, const float x, const float y) -> void
    {
        const color_picker_layout layout{panel};
        if (this->drag_ == part::wheel)
        {
            const auto wheel = layout.Wheel();
            const float radius = wheel.width * 0.5f, dx = x - wheel.x - radius, dy = y - wheel.y - radius;
            const float distance = std::sqrt(dx * dx + dy * dy);
            this->color_.saturation = std::clamp(distance / std::max(1.0f, radius), 0.0f, 1.0f);
            if (distance > 0.001f)
            {
                this->color_.hue = std::atan2(dy, dx) / (2 * pi);
                if (this->color_.hue < 0) this->color_.hue += 1;
            }
        }
        else if (this->drag_ == part::value)
            this->color_.value = 1 - std::clamp((y - layout.Value().y) / std::max(1.0f, layout.Value().height), 0.0f, 1.0f);
        else if (this->drag_ == part::alpha)
            this->color_.alpha = std::clamp((x - layout.Alpha().x) / std::max(1.0f, layout.Alpha().width), 0.0f, 1.0f);
    }

    auto color_picker::Hex(void) const -> std::string
    {
        const auto color = this->Color();
        char text[10]{};
        if ((color >> 24) == 255) std::snprintf(text, sizeof(text), "#%06X", color & 0xFFFFFF);
        else std::snprintf(text, sizeof(text), "#%06X%02X", color & 0xFFFFFF, color >> 24);
        return text;
    }

    auto color_picker::CopyHex(const HWND window) const -> void
    {
        const auto text = this->Hex();
        const std::wstring wide{text.begin(), text.end()};
        if (!::OpenClipboard(window)) return;
        const auto memory = ::GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
        if (memory != nullptr)
        {
            bool copied{};
            if (auto* output = ::GlobalLock(memory); output != nullptr)
            {
                std::memcpy(output, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
                ::GlobalUnlock(memory);
                copied = ::EmptyClipboard() && ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!copied) ::GlobalFree(memory);
        }
        ::CloseClipboard();
    }

    auto color_picker::SetDevice(IDirect3DDevice8* device) -> void
    {
        if (this->device_ == device && this->wheel_ != nullptr && this->checker_ != nullptr) return;
        this->Shutdown();
        this->device_ = device;
        if (device == nullptr || device->TestCooperativeLevel() != D3D_OK) return;
        constexpr uint32_t size = 256;
        if (SUCCEEDED(device->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &this->wheel_)))
        {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(this->wheel_->LockRect(0, &locked, nullptr, 0)))
            {
                for (uint32_t y = 0; y < size; ++y)
                for (uint32_t x = 0; x < size; ++x)
                {
                    const float dx = (x + 0.5f) / (size * 0.5f) - 1, dy = (y + 0.5f) / (size * 0.5f) - 1;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    const float hue = std::atan2(dy, dx) / (2 * pi);
                    auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch);
                    row[x] = ToColor({hue, std::min(distance, 1.0f), 1, std::clamp((1 - distance) * size / 3, 0.0f, 1.0f)});
                }
                this->wheel_->UnlockRect(0);
            }
            else { this->wheel_->Release(); this->wheel_ = nullptr; }
        }
        if (SUCCEEDED(device->CreateTexture(16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &this->checker_)))
        {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(this->checker_->LockRect(0, &locked, nullptr, 0)))
            {
                for (uint32_t y = 0; y < 16; ++y)
                for (uint32_t x = 0; x < 16; ++x)
                    reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch)[x] = ((x / 8 + y / 8) % 2) == 0 ? 0xFF60646C : 0xFF343840;
                this->checker_->UnlockRect(0);
            }
            else { this->checker_->Release(); this->checker_ = nullptr; }
        }
    }

    auto color_picker::Shutdown(void) -> void
    {
        if (this->wheel_ != nullptr) this->wheel_->Release();
        if (this->checker_ != nullptr) this->checker_->Release();
        this->wheel_ = this->checker_ = nullptr;
        this->device_ = nullptr;
    }

    auto color_picker::Checker(const design_system& design, const rect& bounds) const -> void
    {
        if (this->checker_ != nullptr) design.DrawTexture(this->checker_, bounds, 0xFFFFFFFF, {0, 0, bounds.width / 12, bounds.height / 12}, true, true);
        else design.FillRect(bounds, design.GetTheme().colors.surface_raised);
    }

    auto color_picker::Draw(const design_system& design, const config_controls& controls, const rect& panel, const float x, const float y) const -> void
    {
        const color_picker_layout layout{panel};
        const auto& theme = design.GetTheme();
        const auto hovered = HitTest(panel, x, y);
        const float scale = layout.Scale();
        controls.Panel(panel);
        auto title = theme.title_text; title.scale = 0.9f * scale;
        controls.Text("Color", layout.Place({16, 10, 180, 24}), title);
        auto label = theme.caption_text; label.scale = 0.65f * scale;
        controls.Text("Hue & saturation", layout.Place({16, 33, 170, 16}), label);
        controls.IconButton(layout.Close(), icon_glyph::close, hovered == part::close);
        const auto wheel = layout.Wheel();
        design.FillRoundedRect({wheel.x - scale * 2, wheel.y - scale * 2, wheel.width + scale * 4, wheel.height + scale * 4}, theme.colors.border, wheel.width * 0.5f + scale * 2);
        if (this->wheel_ != nullptr) design.DrawTexture(this->wheel_, wheel);
        const float angle = this->color_.hue * 2 * pi, radius = wheel.width * 0.5f;
        const float cx = wheel.x + radius + std::cos(angle) * radius * this->color_.saturation;
        const float cy = wheel.y + radius + std::sin(angle) * radius * this->color_.saturation;
        design.FillRoundedRect({cx - 8 * scale, cy - 8 * scale, 16 * scale, 16 * scale}, 0xD0000000, 8 * scale);
        design.FillRoundedRect({cx - 6 * scale, cy - 6 * scale, 12 * scale, 12 * scale}, 0xFFFFFFFF, 6 * scale);
        design.FillRoundedRect({cx - 4 * scale, cy - 4 * scale, 8 * scale, 8 * scale}, this->Color() | 0xFF000000, 4 * scale);

        const auto value = layout.Value();
        const auto full = ToColor({this->color_.hue, this->color_.saturation, 1, 1});
        design.GradientRect(value, {full, full, 0xFF000000, 0xFF000000});
        design.BorderRect(value, theme.colors.border);
        const float valueY = value.y + (1 - this->color_.value) * value.height;
        design.FillRoundedRect({value.x - 4 * scale, valueY - 3 * scale, value.width + 8 * scale, 6 * scale}, 0xFF000000, 2 * scale);
        design.FillRoundedRect({value.x - 3 * scale, valueY - scale, value.width + 6 * scale, 2 * scale}, 0xFFFFFFFF, scale);
        char number[16]{}; std::snprintf(number, sizeof(number), "V %d", static_cast<int>(std::lround(this->color_.value * 100)));
        auto centered = label; centered.alignment = text_alignment::center;
        controls.Text(number, layout.Place({242, 33, 46, 16}), centered);

        controls.Text("OPACITY", layout.Place({16, 288, 130, 18}), label);
        std::snprintf(number, sizeof(number), "%d%%", static_cast<int>(std::lround(this->color_.alpha * 100)));
        auto right = label; right.alignment = text_alignment::right;
        controls.Text(number, layout.Place({210, 288, 64, 18}), right);
        const auto alpha = layout.Alpha();
        this->Checker(design, alpha);
        const auto opaque = this->Color() | 0xFF000000;
        design.GradientRect(alpha, opaque & 0xFFFFFF, opaque);
        design.BorderRect(alpha, theme.colors.border);
        const float alphaX = alpha.x + this->color_.alpha * alpha.width;
        design.FillRoundedRect({alphaX - 3 * scale, alpha.y - 3 * scale, 6 * scale, alpha.height + 6 * scale}, 0xFF000000, 2 * scale);
        design.FillRoundedRect({alphaX - scale, alpha.y - 2 * scale, 2 * scale, alpha.height + 4 * scale}, 0xFFFFFFFF, scale);

        controls.Text("Current", layout.Place({16, 340, 50, 16}), label);
        controls.Text("Original", layout.Place({74, 340, 50, 16}), label);
        controls.Text("HEX / COPY", layout.Place({134, 338, 140, 16}), label);
        const auto swatch = [&](const rect& bounds, const D3DCOLOR color, const bool active) {
            this->Checker(design, bounds); design.FillRect(bounds, color);
            design.BorderRect(bounds, active ? theme.colors.control_selected : theme.colors.border);
        };
        swatch(layout.Current(), this->Color(), false);
        swatch(layout.Original(), this->original_, hovered == part::restore);
        auto hex = theme.body_text; hex.scale = 0.78f * scale; hex.alignment = text_alignment::center;
        controls.Button(layout.Hex(), this->Hex().c_str(), hex, hovered == part::copy);
    }
}
