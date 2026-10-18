#include "cursor_renderer.hpp"
#include "resources/embedded_resource.hpp"

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
            cursor_resize_horizontal_resource,
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
                const auto cursor = ::GetCursor();
                this->previous_device_cursor_visible_ = this->device_ != nullptr ? this->device_->ShowCursor(FALSE) : FALSE;
                this->cleared_native_cursor_ = cursor != nullptr && ::GetCursor() == nullptr ? cursor : nullptr;
                this->cursor_owner_thread_ = ::GetCurrentThreadId();
                this->native_cursor_hidden_ = true;
                do
                {
                    ++this->cursor_hide_calls_;
                } while (::ShowCursor(FALSE) >= 0);
            }
            else if (this->device_ != nullptr)
            {
                const auto cursor = ::GetCursor();
                this->device_->ShowCursor(FALSE);
                if (cursor != nullptr && ::GetCursor() == nullptr) this->cleared_native_cursor_ = cursor;
            }
            return;
        }
        if (!this->native_cursor_hidden_)
            return;

        const auto cursor = ::GetCursor();
        if (this->device_ != nullptr)
            this->device_->ShowCursor(this->previous_device_cursor_visible_);
        if (cursor != nullptr && ::GetCursor() == nullptr) this->cleared_native_cursor_ = cursor;
        // Balance only our visibility changes, preserving an already hidden cursor.
        while (this->cursor_hide_calls_ > 0)
        {
            ::ShowCursor(TRUE);
            --this->cursor_hide_calls_;
        }
        this->native_cursor_hidden_ = false;
        if (this->cleared_native_cursor_ != nullptr && ::GetCursor() == nullptr)
            ::SetCursor(this->cleared_native_cursor_);
        this->cleared_native_cursor_ = nullptr;
        this->cursor_owner_thread_ = 0;
        if (::GetCursor() == nullptr && this->native_window_ != nullptr && ::IsWindow(this->native_window_) &&
            ::GetForegroundWindow() == this->native_window_)
        {
            POINT cursorPosition{};
            RECT client{};
            if (::GetCursorPos(&cursorPosition) && ::ScreenToClient(this->native_window_, &cursorPosition) &&
                ::GetClientRect(this->native_window_, &client) && ::PtInRect(&client, cursorPosition))
            {
                // Ask the game to choose its cursor without reentering its window procedure.
                ::PostMessageW(this->native_window_, WM_SETCURSOR, reinterpret_cast<WPARAM>(this->native_window_),
                    MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            }
        }
    }

    auto cursor_renderer::ReleaseNativeCursorOnOwnerThread(void) -> void
    {
        // Foreign-thread input callbacks leave restoration to the next render frame.
        if (this->cursor_owner_thread_ == ::GetCurrentThreadId()) this->SetNativeCursorHidden(false);
    }

    auto cursor_renderer::Shutdown(void) -> void
    {
        this->SetNativeCursorHidden(false);
        this->native_window_ = nullptr;
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
        if (cursorShape == shape::resize_vertical)
        {
            const float left = x - hotX, top = y - hotY;
            const std::array<ui::point, 4> points{{{left + displaySize, top}, {left + displaySize, top + displaySize},
                {left, top}, {left, top + displaySize}}};
            design.DrawTextureQuad(this->textures_[index], points, {0, 0, 1, 1}, {});
        }
        else design.DrawTexture(this->textures_[index], {x - hotX, y - hotY, displaySize, displaySize}, 0xFFFFFFFF, mirror ? ui::rect{1, 0, -1, 1} : ui::rect{0, 0, 1, 1});
    }

    auto cursor_renderer::LoadTexture(const UINT resourceId, IDirect3DTexture8** output) const -> bool
    {
        if (this->device_ == nullptr || output == nullptr)
            return false;
        *output = nullptr;

        const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
        const auto bytes = resources::LoadEmbeddedResource(module, static_cast<uint16_t>(resourceId), texture_size * texture_size * 4);
        if (bytes.size() != texture_size * texture_size * 4)
            return false;
        const void* source = bytes.data();

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
