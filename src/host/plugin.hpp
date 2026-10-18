#pragma once

#include "Ashita.h"
#include "callback_gate.hpp"
#include "runtime_host.hpp"
#include <wrl/client.h>
#include "native_hud_controller.hpp"
#include "native_chat_source.hpp"
#include "controller_input.hpp"
#include "keyboard_suppression.hpp"
#include <array>
#include <atomic>

#include <cstdint>

namespace xiui
{
    class plugin final : public IPlugin
    {
    public:
        plugin(void);
        ~plugin(void) override;

        auto GetName(void) const -> const char* override;
        auto GetAuthor(void) const -> const char* override;
        auto GetDescription(void) const -> const char* override;
        auto GetLink(void) const -> const char* override;
        auto GetVersion(void) const -> double override;
        auto GetInterfaceVersion(void) const -> double override;
        auto GetPriority(void) const -> int32_t override;
        auto GetFlags(void) const -> uint32_t override;

        auto Initialize(IAshitaCore* core, ILogManager* logger, uint32_t id) -> bool override;
        auto Release(void) -> void override;

        auto HandleEvent(const char* eventName, const void* eventData, uint32_t eventSize) -> void override;
        auto HandleCommand(int32_t mode, const char* command, bool injected) -> bool override;
        auto HandleIncomingText(int32_t mode, bool indent, const char* message, int32_t* modifiedMode, bool* modifiedIndent, char* modifiedMessage, bool injected, bool blocked) -> bool override;
        auto HandleOutgoingText(int32_t mode, const char* message, int32_t* modifiedMode, char* modifiedMessage, bool injected, bool blocked) -> bool override;
        auto HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked) -> bool override;
        auto HandleOutgoingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked) -> bool override;

        auto Direct3DInitialize(IDirect3DDevice8* device) -> bool override;
        auto Direct3DBeginScene(bool isRenderingBackBuffer) -> void override;
        auto Direct3DEndScene(bool isRenderingBackBuffer) -> void override;
        auto Direct3DPresent(const RECT* sourceRect, const RECT* destinationRect, HWND destinationWindow, const RGNDATA* dirtyRegion) -> void override;
        auto Direct3DDrawPrimitive(D3DPRIMITIVETYPE primitiveType, UINT startVertex, UINT primitiveCount) -> bool override;
        auto Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT startIndex, UINT primitiveCount) -> bool override;
        auto Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT primitiveCount, const void* vertexData, UINT vertexStride) -> bool override;
        auto Direct3DDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT primitiveCount, const void* indexData, D3DFORMAT indexFormat, const void* vertexData, UINT vertexStride) -> bool override;

    private:
        IAshitaCore* core_{};
        ILogManager* log_{};
        IDirect3DDevice8* device_{};
        std::atomic<IDirect3DDevice8*> pending_device_{};
        uint32_t id_{};
        IMouse* registered_mouse_{};
        bool mouse_callback_registered_{};
        bool runtime_fault_reported_{};
        runtime_host runtime_{};
        native_hud_controller native_hud_{};
        native_chat_source native_chat_{};
        controller_input controller_{};
        IController* registered_controller_{};
        IXInput* registered_xinput_{};
        IKeyboard* registered_keyboard_{};
        bool keyboard_block_saved_{};
        bool previous_keyboard_blocked_{};
        keyboard_suppression keyboard_suppression_{};
        chat_palette chat_colors_{};
        uint64_t next_palette_update_{};
        D3DMATRIX scene_view_{};
        D3DMATRIX scene_projection_{};
        D3DVIEWPORT8 scene_viewport_{};
        bool scene_camera_valid_{};
        bool scene_world_camera_valid_{};
        bool world_overlay_drawn_{};
        bool world_overlay_at_end_{};
        bool world_overlay_at_switch_{};
        void* scene_target_hook_{};
        bool scene_hook_minhook_owned_{};
        const char* scene_hook_status_{"waiting"};
        uint32_t scene_opaque_draws_{}, scene_opaque_after_overlay_{};
        uint32_t last_scene_opaque_draws_{}, last_scene_opaque_after_overlay_{};
        uint64_t world_overlay_frames_{}, world_overlay_late_frames_{};
        D3DFORMAT last_scene_depth_format_{D3DFMT_UNKNOWN};
        DWORD scene_depth_mode_{}, scene_depth_function_{}, scene_depth_bias_{};
        UINT scene_target_width_{}, scene_target_height_{};
        const char* last_scene_camera_{"missing"};
        const char* last_world_overlay_{"none"};
        Microsoft::WRL::ComPtr<IDirect3DSurface8> scene_target_{}, scene_depth_{};
        struct ring_scene_probe
        {
            uint32_t frames{}, camera_checks{}, camera_changes{}, target_moves{}, aspect_mismatches{}, overlay_misses{}, scene_end_misses{};
            float camera_delta{}, target_delta{};
        };
        ring_scene_probe ring_probe_{}, last_ring_probe_{};
        std::array<float,3> scene_target_position_{};
        uint32_t scene_target_position_id_{};
        bool scene_end_seen_{};

        static callback_gate<plugin> callback_gate_;
        static auto __stdcall SceneTargetCallback(IDirect3DDevice8* device, IDirect3DSurface8* color, IDirect3DSurface8* depth) -> HRESULT;
        static auto __stdcall MouseCallback(uint32_t message, WPARAM wParam, LPARAM lParam, bool blocked) -> BOOL;
        static auto __stdcall KeyboardCallback(WPARAM key, LPARAM flags, bool blocked) -> BOOL;
        static auto __stdcall KeyboardStateCallback(LPDIRECTINPUTDEVICE8A device, DWORD size, LPVOID data) -> HRESULT;
        static auto __stdcall KeyboardDataCallback(LPDIRECTINPUTDEVICE8A device, DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags, DWORD result) -> HRESULT;
        static auto __stdcall ControllerButtonCallback(uint32_t* field, int32_t* state, bool injected, bool blocked) -> BOOL;
        static auto __stdcall ControllerStateCallback(LPDIRECTINPUTDEVICE8A device, DWORD size, LPVOID data) -> HRESULT;
        static auto __stdcall ControllerDataCallback(LPDIRECTINPUTDEVICE8A device, DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags, DWORD result) -> HRESULT;
        static auto __stdcall XInputButtonCallback(uint8_t* field, int16_t* state, bool injected, bool blocked) -> BOOL;
        static auto __stdcall XInputStateCallback(DWORD user, XINPUT_STATE* state) -> DWORD;
        auto KeyboardForeground(void) const -> bool;
        auto UpdateKeyboardCapture(void) -> void;

        auto ApplyPendingDevice(void) -> void;
        auto InstallSceneTargetHook(void) -> void;
        auto CaptureSceneCamera(void) -> void;
        auto ReadRingTargetPosition(std::array<float,3>& position) const -> uint32_t;
        auto DrawWorldOverlay(bool sceneEnd = false) -> void;
        auto IsLoggedIn(void) const -> bool;
        auto BuildSnapshot(hud_snapshot& snapshot) const -> bool;
        auto WriteChat(const char* message) const -> void;
    };
} // namespace xiui
