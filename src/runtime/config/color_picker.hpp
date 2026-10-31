#pragma once

#include "design_system.hpp"
#include "text_input.hpp"

#include <algorithm>
#include <optional>

namespace xiui::ui
{
    class config_controls;

    struct hsva_color { float hue{}, saturation{}, value{}, alpha{1}; };

    class color_picker_layout
    {
    public:
        explicit color_picker_layout(const rect& bounds) : bounds_(bounds), scale_(std::min(bounds.width / 300, bounds.height / 404)) {}
        auto Wheel(void) const -> rect { return this->Place({16, 52, 224, 224}); }
        auto Value(void) const -> rect { return this->Place({256, 52, 18, 224}); }
        auto Alpha(void) const -> rect { return this->Place({16, 310, 258, 16}); }
        auto Close(void) const -> rect { return this->Place({268, 10, 20, 22}); }
        auto Current(void) const -> rect { return this->Place({16, 360, 50, 28}); }
        auto Original(void) const -> rect { return this->Place({74, 360, 50, 28}); }
        auto Hex(void) const -> rect { return this->Place({134, 358, 94, 30}); }
        auto Copy(void) const -> rect { return this->Place({232, 358, 42, 30}); }
        auto Place(const rect& at) const -> rect { return {this->bounds_.x + at.x * this->scale_, this->bounds_.y + at.y * this->scale_, at.width * this->scale_, at.height * this->scale_}; }
        auto Scale(void) const -> float { return this->scale_; }

    private:
        rect bounds_{};
        float scale_{};
    };

    class color_picker
    {
    public:
        enum class part : uint8_t { none, wheel, value, alpha, restore, hex, copy, close };
        color_picker(void) = default;
        ~color_picker(void) { this->Shutdown(); }
        color_picker(const color_picker&) = delete;
        auto operator=(const color_picker&) -> color_picker& = delete;

        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Shutdown(void) -> void;
        auto Open(D3DCOLOR color) -> void;
        auto SetColor(D3DCOLOR color) -> void { this->color_ = FromColor(color, this->color_.hue); this->CancelHex(); }
        auto Restore(void) -> void { this->SetColor(this->original_); }
        auto Color(void) const -> D3DCOLOR { return ToColor(this->color_); }
        auto Original(void) const -> D3DCOLOR { return this->original_; }
        auto Value(void) const -> hsva_color { return this->color_; }
        auto Hex(void) const -> std::string;
        auto CopyHex(HWND window) const -> void;
        auto BeginHex(void) -> void { this->hex_.Open(this->Hex(), 9); this->invalid_hex_ = false; }
        auto CancelHex(void) -> void { this->hex_.Open(this->Hex(), 9); this->hex_.Cancel(); this->invalid_hex_ = false; }
        auto EditingHex(void) const -> bool { return this->hex_.Focused(); }
        auto HexInput(void) -> text_input& { return this->hex_; }
        auto CommitHex(void) -> bool;
        static auto ParseHex(std::string_view text) -> std::optional<D3DCOLOR>;
        auto BeginDrag(part control) -> void { this->drag_ = control; }
        auto EndDrag(void) -> void { this->drag_ = part::none; }
        auto Dragging(void) const -> part { return this->drag_; }
        auto Update(const rect& panel, float x, float y) -> void;
        auto Draw(const design_system& design, const config_controls& controls, const rect& panel, float x, float y) const -> void;
        static auto HitTest(const rect& panel, float x, float y) -> part;
        static auto FromColor(D3DCOLOR color, float previousHue = 0) -> hsva_color;
        static auto ToColor(const hsva_color& color) -> D3DCOLOR;

    private:
        hsva_color color_{};
        D3DCOLOR original_{};
        part drag_{};
        text_input hex_{};
        bool invalid_hex_{};
        IDirect3DDevice8* device_{};
        IDirect3DTexture8* wheel_{}, *checker_{};
        auto Checker(const design_system& design, const rect& bounds) const -> void;
    };
}
