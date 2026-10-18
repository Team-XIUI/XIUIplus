#pragma once

#include "Ashita.h"
#include "controller_normalizer.hpp"
#include "crossbar_normalizer.hpp"

#include <array>
#include <mutex>
#include <string>

namespace xiui
{
    class runtime_host;

    class controller_input
    {
    public:
        auto Initialize(IAshitaCore* core, runtime_host& runtime) -> void;
        auto Shutdown(void) -> void;
        auto Tick(bool runtimeReplaced = false) -> void;
        auto DirectInputButton(uint32_t field, int32_t value, bool injected, bool blocked) -> bool;
        auto DirectInputState(LPDIRECTINPUTDEVICE8A device, DWORD size, void* data) -> void;
        auto DirectInputData(LPDIRECTINPUTDEVICE8A device, DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count) -> void;
        auto XInputButton(uint8_t field, int16_t value, bool injected, bool blocked) -> bool;
        auto XInputState(DWORD user, XINPUT_STATE* state) -> void;

    private:
        struct axis_range { LONG minimum{-127}; LONG maximum{127}; LONG center{}; };
        std::recursive_mutex mutex_{};
        IAshitaCore* core_{};
        runtime_host* runtime_{};
        controller_status status_{};
        controller_normalizer direct_{};
        controller_normalizer xinput_{};
        crossbar_normalizer crossbar_direct_{};
        crossbar_normalizer crossbar_xinput_{};
        controller_crossbar_settings crossbar_settings_{};
        controller_backend crossbar_backend_{};
        uint32_t crossbar_revision_{};
        std::vector<int32_t> native_mapping_{};
        bool crossbar_configured_{};
        LPDIRECTINPUTDEVICE8A direct_device_{};
        std::array<axis_range, 8> direct_ranges_{};
        uint64_t last_state_at_{};
        uint64_t next_status_at_{};
        bool was_suspended_{};

        auto ResolveConfiguration(void) -> void;
        auto RefreshCrossbar(void) -> void;
        auto CrossbarEligible(controller_backend backend) -> bool;
        auto Foreground(void) const -> bool;
        auto Enabled(controller_backend backend) const -> bool;
        auto NativeBusy(void) const -> bool;
        auto NativeMenuOpen(void) const -> bool;
        auto Captured(void) -> bool;
        auto Observe(controller_backend backend, uint64_t now, bool state, bool meaningful) -> void;
        auto Dispatch(const controller_event& event) -> bool;
        auto PrepareDirectInput(LPDIRECTINPUTDEVICE8A device) -> void;
        auto NormalizeAxis(uint32_t field, int32_t value) const -> int32_t;
        auto DirectNeutral(uint32_t field) const -> int32_t;
    };
}
