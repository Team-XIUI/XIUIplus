#pragma once

#include "Ashita.h"
#include "callback_gate.hpp"
#include "runtime_host.hpp"
#include "native_hud_controller.hpp"
#include "native_chat_source.hpp"
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
        auto Direct3DSetRenderState(D3DRENDERSTATETYPE state, DWORD* value) -> bool override;
        auto Direct3DDrawPrimitive(D3DPRIMITIVETYPE primitiveType, UINT startVertex, UINT primitiveCount) -> bool override;
        auto Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT startIndex, UINT primitiveCount) -> bool override;
        auto Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT primitiveCount, const void* vertexData, UINT vertexStride) -> bool override;
        auto Direct3DDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT primitiveCount, const void* indexData, D3DFORMAT indexFormat, const void* vertexData, UINT vertexStride) -> bool override;

    private:
        IAshitaCore* core_{};
        ILogManager* log_{};
        IDirect3DDevice8* device_{};
        uint32_t id_{};
        IMouse* registered_mouse_{};
        bool mouse_callback_registered_{};
        bool runtime_fault_reported_{};
        runtime_host runtime_{};
        native_hud_controller native_hud_{};
        native_chat_source native_chat_{};
        IKeyboard* registered_keyboard_{};
        bool keyboard_block_saved_{};
        bool previous_keyboard_blocked_{};
        std::array<std::atomic_bool, 256> consumed_keys_{};
        chat_palette chat_colors_{};
        uint64_t next_palette_update_{};
        D3DMATRIX scene_view_{};
        D3DMATRIX scene_projection_{};
        D3DVIEWPORT8 scene_viewport_{};
        bool scene_camera_valid_{};

        static callback_gate<plugin> callback_gate_;
        static auto __stdcall MouseCallback(uint32_t message, WPARAM wParam, LPARAM lParam, bool blocked) -> BOOL;
        static auto __stdcall KeyboardCallback(WPARAM key, LPARAM flags, bool blocked) -> BOOL;
        static auto __stdcall KeyboardStateCallback(LPDIRECTINPUTDEVICE8A device, DWORD size, LPVOID data) -> HRESULT;
        static auto __stdcall KeyboardDataCallback(LPDIRECTINPUTDEVICE8A device, DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags, DWORD result) -> HRESULT;
        auto ShouldBlockScanCode(uint32_t scanCode, bool focused) const -> bool;
        auto UpdateKeyboardCapture(void) -> void;

        auto CaptureSceneCamera(void) -> void;
        auto IsLoggedIn(void) const -> bool;
        auto BuildSnapshot(hud_snapshot& snapshot) const -> bool;
        auto WriteChat(const char* message) const -> void;
    };
} // namespace xiui
