#pragma once

#include "design_system.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace xiui
{
    class cursor_renderer
    {
    public:
        enum class shape : uint8_t
        {
            arrow,
            hand,
            move,
            resize_diagonal,
            resize_horizontal,
            count,
        };

        cursor_renderer(void) = default;
        ~cursor_renderer(void);

        auto SetDevice(IDirect3DDevice8* device) -> bool;
        auto SetNativeCursorHidden(bool hidden) -> void;
        auto Shutdown(void) -> void;
        auto Draw(ui::design_system& design, shape cursorShape, float x, float y, bool mirror = false) const -> void;

    private:
        static constexpr UINT texture_size = 64;
        std::array<IDirect3DTexture8*, static_cast<size_t>(shape::count)> textures_{};
        IDirect3DDevice8* device_{};
        bool native_cursor_hidden_{};
        BOOL previous_device_cursor_visible_{};
        uint32_t cursor_hide_calls_{};

        auto LoadTexture(UINT resourceId, IDirect3DTexture8** output) const -> bool;
    };
} // namespace xiui
