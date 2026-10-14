#include "cursor_renderer.hpp"

#include <cstring>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr UINT cursor_pointer_resource = 101;
    constexpr UINT cursor_hand_resource = 102;
    constexpr UINT cursor_move_resource = 103;
    constexpr UINT cursor_resize_diagonal_resource = 104;
    constexpr UINT cursor_resize_horizontal_resource = 105;
}

namespace xiui
{
    cursor_renderer::~cursor_renderer(void)
    {
        this->Shutdown();
    }

    auto cursor_renderer::SetDevice(IDirect3DDevice8* device) -> bool
    {
        if (this->device_ == device && this->textures_[0] != nullptr)
            return true;
        this->Shutdown();
        this->device_ = device;
        if (device == nullptr)
            return true;

        constexpr UINT resources[] = {
            cursor_pointer_resource,
            cursor_hand_resource,
            cursor_move_resource,
            cursor_resize_diagonal_resource,
            cursor_resize_horizontal_resource};
        for (size_t index = 0; index < this->textures_.size(); ++index)
        {
            if (!this->LoadTexture(resources[index], &this->textures_[index]))
            {
                this->Shutdown();
                return false;
            }
        }
        return true;
    }

    auto cursor_renderer::SetNativeCursorHidden(const bool hidden) -> void
    {
        if (hidden)
        {
            if (!this->native_cursor_hidden_)
            {
                this->previous_device_cursor_visible_ = this->device_ != nullptr ? this->device_->ShowCursor(FALSE) : FALSE;
                this->native_cursor_hidden_ = true;
                do
                {
                    ++this->cursor_hide_calls_;
                } while (::ShowCursor(FALSE) >= 0);
            }
            else if (this->device_ != nullptr)
            {
                this->device_->ShowCursor(FALSE);
            }
            return;
        }
        if (!this->native_cursor_hidden_)
            return;

        if (this->device_ != nullptr)
            this->device_->ShowCursor(this->previous_device_cursor_visible_);
        // Balance only our visibility changes, preserving an already hidden cursor.
        while (this->cursor_hide_calls_ > 0)
        {
            ::ShowCursor(TRUE);
            --this->cursor_hide_calls_;
        }
        this->native_cursor_hidden_ = false;
    }

    auto cursor_renderer::Shutdown(void) -> void
    {
        this->SetNativeCursorHidden(false);
        for (auto*& texture : this->textures_)
        {
            if (texture != nullptr)
                texture->Release();
            texture = nullptr;
        }
        this->device_ = nullptr;
    }

    auto cursor_renderer::Draw(ui::design_system& design, const shape cursorShape, const float x, const float y, const bool mirror) const -> void
    {
        const size_t index = static_cast<size_t>(cursorShape);
        if (index >= this->textures_.size() || this->textures_[index] == nullptr)
            return;

        constexpr float displaySize = 34.0f;
        float hotX{};
        float hotY{};
        switch (cursorShape)
        {
        case shape::arrow:
            hotX = displaySize * 0.25f;
            hotY = displaySize * 0.1875f;
            break;
        case shape::hand:
            hotX = displaySize * 0.4375f;
            hotY = displaySize * 0.125f;
            break;
        default:
            hotX = displaySize * 0.5f;
            hotY = displaySize * 0.5f;
            break;
        }
        design.DrawTexture(this->textures_[index], {x - hotX, y - hotY, displaySize, displaySize}, 0xFFFFFFFF, mirror ? ui::rect{1, 0, -1, 1} : ui::rect{0, 0, 1, 1});
    }

    auto cursor_renderer::LoadTexture(const UINT resourceId, IDirect3DTexture8** output) const -> bool
    {
        if (this->device_ == nullptr || output == nullptr)
            return false;
        *output = nullptr;

        const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
        const HRSRC resource = ::FindResourceA(module, MAKEINTRESOURCEA(resourceId), RT_RCDATA);
        if (resource == nullptr || ::SizeofResource(module, resource) != texture_size * texture_size * 4)
            return false;
        const HGLOBAL loaded = ::LoadResource(module, resource);
        const void* source = loaded != nullptr ? ::LockResource(loaded) : nullptr;
        if (source == nullptr)
            return false;

        IDirect3DTexture8* texture{};
        if (FAILED(this->device_->CreateTexture(texture_size, texture_size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture)))
            return false;
        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        {
            texture->Release();
            return false;
        }
        constexpr size_t rowBytes = texture_size * 4;
        for (UINT row = 0; row < texture_size; ++row)
        {
            std::memcpy(
                static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
                static_cast<const uint8_t*>(source) + static_cast<size_t>(row) * rowBytes,
                rowBytes);
        }
        texture->UnlockRect(0);
        *output = texture;
        return true;
    }
} // namespace xiui
