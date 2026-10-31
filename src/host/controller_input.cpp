#include "controller_input.hpp"
#include "native_target.hpp"
#include "runtime_host.hpp"

#include <algorithm>
#include <cstring>
#include <optional>

#pragma comment(lib, "Advapi32.lib")

namespace
{
    auto registry_value(const char* path, const char* name) -> std::optional<std::string>
    {
        HKEY key{};
        if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return {};
        char buffer[4096]{};
        DWORD size = sizeof(buffer) - 1;
        DWORD type{};
        const auto result = ::RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size);
        ::RegCloseKey(key);
        if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
        buffer[std::min<size_t>(size, sizeof(buffer) - 1)] = '\0';
        return std::string{buffer};
    }

    auto boot_value(IAshitaCore* core, const char* name) -> std::optional<std::string>
    {
        auto* config = core != nullptr ? core->GetConfigurationManager() : nullptr;
        const auto* value = config != nullptr ? config->GetString("boot", "ffxi.registry", name) : nullptr;
        if (value == nullptr || *value == '\0' || std::strcmp(value, "-1") == 0) return {};
        return std::string{value};
    }

    auto effective_values(IAshitaCore* core, const char* name, const char* registryPath) -> std::optional<std::string>
    {
        if (auto value = boot_value(core, name)) return value;
        return registryPath != nullptr ? registry_value(registryPath, name) : std::nullopt;
    }

    constexpr std::array<WORD, 16> xinput_masks{
        XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
        XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0,
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y};
}

namespace xiui
{
    auto controller_input::Initialize(IAshitaCore* core, runtime_host& runtime) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->core_ = core;
        this->runtime_ = &runtime;
        this->ResolveConfiguration();
        this->Tick();
    }

    auto controller_input::Shutdown(void) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->status_.suspended = true;
        if (this->runtime_ != nullptr) this->runtime_->UpdateControllerStatus(this->status_);
        this->core_ = nullptr;
        this->runtime_ = nullptr;
        this->status_ = {};
        this->direct_ = {};
        this->xinput_ = {};
        this->crossbar_direct_ = {};
        this->crossbar_xinput_ = {};
        this->crossbar_settings_ = {};
        this->crossbar_backend_ = controller_backend::none;
        this->crossbar_revision_ = 0;
        this->native_mapping_.clear();
        this->crossbar_configured_ = false;
        this->direct_device_ = nullptr;
        this->last_state_at_ = this->next_status_at_ = 0;
        this->was_suspended_ = false;
    }

    auto controller_input::ResolveConfiguration(void) -> void
    {
        // These reads stay in-process so Ashita's registry virtualization applies.
        constexpr const char* paths[]{
            "SOFTWARE\\PlayOnlineUS\\SquareEnix\\FinalFantasyXI",
            "SOFTWARE\\PlayOnline\\SquareEnix\\FinalFantasyXI",
            "SOFTWARE\\PlayOnlineEU\\SquareEnix\\FinalFantasyXI"};
        const char* registryPath = nullptr;
        for (const auto* path : paths)
            if (registry_value(path, "padmode000")) { registryPath = path; break; }
        std::vector<int32_t> mode;
        if (const auto value = effective_values(this->core_, "padmode000", registryPath);
            value && ParseControllerValues(*value, mode) && mode.size() >= 6 &&
            (mode[0] == 0 || mode[0] == 1) && (mode[5] == 0 || mode[5] == 1))
        {
            this->status_.configuration_known = true;
            this->status_.gamepad_enabled = mode[0] != 0;
            this->status_.backend = mode[5] != 0 ? controller_backend::xinput : controller_backend::direct_input;
        }
        std::vector<int32_t> mapping;
        if (const auto value = effective_values(this->core_, "padsin000", registryPath);
            value && ParseControllerValues(*value, mapping))
            this->status_.bindings_known = this->status_.configuration_known && mapping.size() >= 27;
        else mapping.clear();
        this->native_mapping_ = mapping;
        this->direct_.Configure(controller_backend::direct_input,
            this->status_.backend == controller_backend::direct_input ? mapping : std::vector<int32_t>{});
        this->xinput_.Configure(controller_backend::xinput,
            this->status_.backend == controller_backend::xinput ? mapping : std::vector<int32_t>{});
        this->crossbar_configured_ = false;
        this->RefreshCrossbar();
    }

    auto controller_input::RefreshCrossbar(void) -> void
    {
        const auto settings = this->runtime_ != nullptr ? this->runtime_->CrossbarSettings() : controller_crossbar_settings{};
        const auto revision = this->runtime_ != nullptr ? this->runtime_->ControllerRevision() : 0;
        if (!this->crossbar_configured_ || settings != this->crossbar_settings_ || revision != this->crossbar_revision_ || this->status_.backend != this->crossbar_backend_)
        {
            this->crossbar_settings_ = settings;
            this->crossbar_revision_ = revision;
            this->crossbar_backend_ = this->status_.backend;
            this->crossbar_direct_.Configure(controller_backend::direct_input,
                this->status_.backend == controller_backend::direct_input ? this->native_mapping_ : std::vector<int32_t>{}, settings);
            this->crossbar_xinput_.Configure(controller_backend::xinput,
                this->status_.backend == controller_backend::xinput ? this->native_mapping_ : std::vector<int32_t>{}, settings);
            this->crossbar_configured_ = true;
            this->next_status_at_ = 0;
        }
        const auto& crossbar = this->status_.backend == controller_backend::direct_input ? this->crossbar_direct_ : this->crossbar_xinput_;
        this->status_.crossbar_mapping = crossbar.Mapping();
        this->status_.crossbar_mapping_known = this->status_.backend != controller_backend::none && crossbar.Ready();
        this->status_.crossbar_ready = this->status_.crossbar_mapping_known && settings.enabled && settings.replace_native &&
            !this->status_.suspended && !this->status_.enumeration_disabled &&
            (!this->status_.configuration_known || this->status_.gamepad_enabled);
    }

    auto controller_input::CrossbarEligible(const controller_backend backend) -> bool
    {
        return this->runtime_ != nullptr && this->Enabled(backend) && !this->NativeBusy() && !this->NativeMenuOpen() &&
            this->runtime_->ControllerCapture() == controller_capture_mode::none;
    }

    auto controller_input::Foreground(void) const -> bool
    {
        auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        const auto window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
        return window != nullptr && ::GetForegroundWindow() == window;
    }

    auto controller_input::Enabled(const controller_backend backend) const -> bool
    {
        return !this->status_.suspended && !this->status_.enumeration_disabled &&
            (!this->status_.configuration_known || (this->status_.gamepad_enabled && this->status_.backend == backend)) &&
            (this->status_.backend == controller_backend::none || this->status_.backend == backend);
    }

    auto controller_input::NativeBusy(void) const -> bool
    {
        if (!this->Foreground() || this->status_.suspended) return true;
        const auto* chat = this->core_ != nullptr ? this->core_->GetChatManager() : nullptr;
        if (chat != nullptr && chat->IsInputOpen() != 0) return true;
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        if (player != nullptr && player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn)) return true;
        return NativeTargetSelectionActive(memory);
    }

    auto controller_input::NativeMenuOpen(void) const -> bool
    {
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        return target != nullptr && target->GetIsMenuOpen() != 0;
    }

    auto controller_input::Captured(void) -> bool
    {
        if (this->runtime_ != nullptr && this->NativeBusy()) this->runtime_->SuspendControllerInput();
        const auto mode = this->runtime_ != nullptr ? this->runtime_->ControllerCapture(this->NativeMenuOpen()) : controller_capture_mode::none;
        const bool nativeCycle = this->runtime_ != nullptr && this->runtime_->NativeWindowCycle();
        this->direct_.SetNativeWindowCycle(nativeCycle); this->xinput_.SetNativeWindowCycle(nativeCycle);
        this->direct_.SetNavigationOnly(mode == controller_capture_mode::navigation);
        this->xinput_.SetNavigationOnly(mode == controller_capture_mode::navigation);
        return mode != controller_capture_mode::none;
    }

    auto controller_input::Observe(const controller_backend backend, const uint64_t now, const bool state, const bool meaningful) -> void
    {
        if (this->status_.backend == controller_backend::none) this->status_.backend = backend;
        if (state)
        {
            this->status_.connected = this->status_.connection_known = true;
            this->last_state_at_ = now;
        }
        if (meaningful)
        {
            this->status_.received_input = true;
            this->status_.last_input_at = now;
        }
    }

    auto controller_input::Dispatch(const controller_event& event) -> bool
    {
        if (this->runtime_ == nullptr || !this->Enabled(event.backend) || this->NativeBusy()) return false;
        if (event.pressed && !event.repeated) this->Observe(event.backend, event.timestamp, false, true);
        return this->runtime_->HandleController(event, this->NativeMenuOpen());
    }

    auto controller_input::Tick(const bool runtimeReplaced) -> void
    {
        std::lock_guard lock(this->mutex_);
        if (this->runtime_ == nullptr) return;
        const auto now = ::GetTickCount64();
        const auto* input = this->core_ != nullptr ? this->core_->GetInputManager() : nullptr;
        this->status_.enumeration_disabled = input != nullptr && input->GetDisableGamepad();
        const bool stale = this->last_state_at_ != 0 && now - this->last_state_at_ > 3000;
        this->status_.suspended = runtimeReplaced || !this->Foreground() || this->status_.enumeration_disabled ||
            (this->status_.configuration_known && !this->status_.gamepad_enabled) || stale;
        if (stale)
        {
            // No callbacks is not proof of an unplugged device.
            this->status_.connection_known = false;
            this->status_.connected = false;
        }
        if (this->status_.suspended && !this->was_suspended_)
        {
            this->direct_.Suspend();
            this->xinput_.Suspend();
        }
        this->was_suspended_ = this->status_.suspended;
        this->RefreshCrossbar();
        if (this->status_.suspended || now >= this->next_status_at_ || runtimeReplaced)
        {
            this->runtime_->UpdateControllerStatus(this->status_);
            this->next_status_at_ = now + 250;
        }
        const auto dispatch = [this](const controller_event& event) { return this->Dispatch(event); };
        if (!this->CrossbarEligible(controller_backend::direct_input)) this->crossbar_direct_.Suspend(now, dispatch);
        if (!this->CrossbarEligible(controller_backend::xinput)) this->crossbar_xinput_.Suspend(now, dispatch);
        this->direct_.Repeat(now, this->Captured() && this->status_.backend == controller_backend::direct_input, dispatch);
        this->xinput_.Repeat(now, this->Captured() && this->status_.backend == controller_backend::xinput, dispatch);
    }

    auto controller_input::NormalizeAxis(const uint32_t field, const int32_t value) const -> int32_t
    {
        if (field >= 32 || field % 4 != 0) return value;
        const auto& range = this->direct_ranges_[field / 4];
        const auto extent = value >= range.center ? range.maximum - range.center : range.center - range.minimum;
        if (extent <= 0) return 0;
        return static_cast<int32_t>(std::clamp<int64_t>((static_cast<int64_t>(value) - range.center) * 127 / extent, -127, 127));
    }

    auto controller_input::DirectNeutral(const uint32_t field) const -> int32_t
    {
        return field < 32 && field % 4 == 0 ? this->direct_ranges_[field / 4].center : field >= 32 && field < 48 ? -1 : 0;
    }

    auto controller_input::PrepareDirectInput(LPDIRECTINPUTDEVICE8A device) -> void
    {
        if (device == nullptr || device == this->direct_device_) return;
        if (this->direct_device_ != nullptr)
        {
            this->crossbar_direct_.ResetDevice();
            this->direct_.Configure(controller_backend::direct_input, this->status_.bindings_known ? this->native_mapping_ : std::vector<int32_t>{});
            if (this->runtime_ != nullptr)
            {
                this->runtime_->SuspendControllerInput();
                auto suspended = this->status_;
                suspended.suspended = true;
                this->runtime_->UpdateControllerStatus(suspended);
            }
            this->next_status_at_ = 0;
        }
        this->direct_device_ = device;
        for (uint32_t index = 0; index < this->direct_ranges_.size(); ++index)
        {
            DIPROPRANGE range{};
            range.diph.dwSize = sizeof(range);
            range.diph.dwHeaderSize = sizeof(range.diph);
            range.diph.dwObj = index * 4;
            range.diph.dwHow = DIPH_BYOFFSET;
            this->direct_ranges_[index] = {};
            if (SUCCEEDED(device->GetProperty(DIPROP_RANGE, &range.diph)) && range.lMax > range.lMin)
                this->direct_ranges_[index] = {range.lMin, range.lMax, static_cast<LONG>((static_cast<int64_t>(range.lMin) + range.lMax) / 2)};
        }
    }

    auto controller_input::DirectInputButton(const uint32_t field, const int32_t value, const bool injected, const bool blocked) -> bool
    {
        std::lock_guard lock(this->mutex_);
        const auto now = ::GetTickCount64();
        this->Observe(controller_backend::direct_input, now, false, false);
        this->RefreshCrossbar();
        const bool captured = this->Captured();
        const auto normalized = this->NormalizeAxis(field, value);
        const bool unavailable = blocked || !this->Enabled(controller_backend::direct_input) || !this->Foreground();
        const bool crossbarEligible = this->CrossbarEligible(controller_backend::direct_input);
        const bool crossbarConsumed = this->crossbar_direct_.Raw(field, normalized, now, injected, unavailable, crossbarEligible,
            [this](const controller_event& event) { return this->Dispatch(event); });
        const bool consumed = this->direct_.Raw(field, normalized, ::GetTickCount64(), injected,
            unavailable || crossbarConsumed, captured,
            [this](const controller_event& event) { return this->Dispatch(event); });
        return this->crossbar_direct_.Suppress(field, normalized, !blocked && crossbarEligible) || crossbarConsumed ||
            this->direct_.Suppress(field, normalized, captured || this->Captured()) || consumed;
    }

    auto controller_input::DirectInputState(LPDIRECTINPUTDEVICE8A device, const DWORD size, void* data) -> void
    {
        std::lock_guard lock(this->mutex_);
        if (data == nullptr || (size != sizeof(DIJOYSTATE) && size != sizeof(DIJOYSTATE2))) return;
        if (this->status_.configuration_known && this->status_.backend != controller_backend::direct_input) return;
        this->PrepareDirectInput(device);
        this->Observe(controller_backend::direct_input, ::GetTickCount64(), true, false);
        this->status_.suspended = !this->Foreground() || this->status_.enumeration_disabled;
        this->RefreshCrossbar();
        const bool wasCaptured = this->Captured();
        const auto now = ::GetTickCount64();
        const bool blocked = !this->Enabled(controller_backend::direct_input) || !this->Foreground();
        const auto dispatch = [this](const controller_event& event) { return this->Dispatch(event); };
        auto* bytes = static_cast<uint8_t*>(data);
        const bool crossbarEligible = this->CrossbarEligible(controller_backend::direct_input);
        auto crossbarValues = this->crossbar_direct_.Values();
        for (uint32_t field = 0; field < 48; field += 4)
            crossbarValues[field] = this->NormalizeAxis(field, *reinterpret_cast<LONG*>(bytes + field));
        const uint32_t buttons = size == sizeof(DIJOYSTATE2) ? 128 : 32;
        for (uint32_t index = 0; index < buttons; ++index) crossbarValues[48 + index] = bytes[48 + index];
        this->crossbar_direct_.State(crossbarValues, now, blocked, crossbarEligible, dispatch);
        for (uint32_t field = 0; field < 48; field += 4)
        {
            auto* value = reinterpret_cast<LONG*>(bytes + field);
            const auto normalized = this->NormalizeAxis(field, *value);
            this->direct_.State(field, normalized, now,
                blocked || this->crossbar_direct_.Suppress(field, normalized, crossbarEligible), wasCaptured, dispatch);
        }
        for (uint32_t index = 0; index < buttons; ++index)
        {
            this->direct_.State(48 + index, bytes[48 + index], now,
                blocked || this->crossbar_direct_.Suppress(48 + index, bytes[48 + index], crossbarEligible), wasCaptured, dispatch);
        }
        const bool captured = wasCaptured || this->Captured();
        for (uint32_t field = 0; field < 48; field += 4)
        {
            auto* value = reinterpret_cast<LONG*>(bytes + field);
            const auto normalized = this->NormalizeAxis(field, *value);
            if (this->crossbar_direct_.Suppress(field, normalized, crossbarEligible) || this->direct_.Suppress(field, normalized, captured))
                *value = this->DirectNeutral(field);
        }
        for (uint32_t index = 0; index < buttons; ++index)
        {
            if (this->crossbar_direct_.Suppress(48 + index, bytes[48 + index], crossbarEligible) ||
                this->direct_.Suppress(48 + index, bytes[48 + index], captured)) bytes[48 + index] = 0;
        }
        if (captured && !this->direct_.NavigationOnly() && size == sizeof(DIJOYSTATE2))
            std::memset(bytes + sizeof(DIJOYSTATE), 0, size - sizeof(DIJOYSTATE));
    }

    auto controller_input::DirectInputData(LPDIRECTINPUTDEVICE8A device, const DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count) -> void
    {
        std::lock_guard lock(this->mutex_);
        if (data == nullptr || count == nullptr || *count > 4096 || objectSize < 8 || objectSize > sizeof(DIDEVICEOBJECTDATA)) return;
        if (this->status_.configuration_known && this->status_.backend != controller_backend::direct_input) return;
        this->PrepareDirectInput(device);
        this->Observe(controller_backend::direct_input, ::GetTickCount64(), true, false);
        this->status_.suspended = !this->Foreground() || this->status_.enumeration_disabled;
        this->RefreshCrossbar();
        const bool wasCaptured = this->Captured();
        const auto now = ::GetTickCount64();
        const bool blocked = !this->Enabled(controller_backend::direct_input) || !this->Foreground();
        const bool crossbarEligible = this->CrossbarEligible(controller_backend::direct_input);
        std::vector<uint8_t> crossbarConsumed(*count);
        for (DWORD index = 0; index < *count; ++index)
        {
            auto* entry = reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(data) + index * objectSize);
            const auto normalized = this->NormalizeAxis(entry[0], static_cast<int32_t>(entry[1]));
            crossbarConsumed[index] = this->crossbar_direct_.State(entry[0], normalized, now, blocked, crossbarEligible,
                [this](const controller_event& event) { return this->Dispatch(event); });
            this->direct_.State(entry[0], normalized, now,
                blocked || crossbarConsumed[index] != 0, wasCaptured,
                [this](const controller_event& event) { return this->Dispatch(event); });
        }
        const bool captured = wasCaptured || this->Captured();
        for (DWORD index = 0; index < *count; ++index)
        {
            auto* entry = reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(data) + index * objectSize);
            const auto normalized = this->NormalizeAxis(entry[0], static_cast<int32_t>(entry[1]));
            if (crossbarConsumed[index] != 0 || this->crossbar_direct_.Suppress(entry[0], normalized, crossbarEligible) ||
                this->direct_.Suppress(entry[0], normalized, captured))
                entry[1] = static_cast<DWORD>(this->DirectNeutral(entry[0]));
        }
    }

    auto controller_input::XInputButton(const uint8_t field, const int16_t value, const bool injected, const bool blocked) -> bool
    {
        std::lock_guard lock(this->mutex_);
        const auto now = ::GetTickCount64();
        this->Observe(controller_backend::xinput, now, false, false);
        this->RefreshCrossbar();
        const bool captured = this->Captured();
        const bool unavailable = blocked || !this->Enabled(controller_backend::xinput) || !this->Foreground();
        const bool crossbarEligible = this->CrossbarEligible(controller_backend::xinput);
        const bool crossbarConsumed = this->crossbar_xinput_.Raw(field, value, now, injected, unavailable, crossbarEligible,
            [this](const controller_event& event) { return this->Dispatch(event); });
        const bool consumed = this->xinput_.Raw(field, value, ::GetTickCount64(), injected,
            unavailable || crossbarConsumed, captured,
            [this](const controller_event& event) { return this->Dispatch(event); });
        return this->crossbar_xinput_.Suppress(field, value, !blocked && crossbarEligible) || crossbarConsumed ||
            this->xinput_.Suppress(field, value, captured || this->Captured()) || consumed;
    }

    auto controller_input::XInputState(const DWORD user, XINPUT_STATE* state) -> void
    {
        std::lock_guard lock(this->mutex_);
        if (state == nullptr || user >= XUSER_MAX_COUNT) return;
        if (this->status_.configuration_known && this->status_.backend != controller_backend::xinput) return;
        if (this->status_.connection_known && this->status_.backend == controller_backend::xinput && user != this->status_.user_index) return;
        this->status_.user_index = static_cast<uint8_t>(user);
        this->xinput_.SetUserIndex(static_cast<uint8_t>(user));
        this->crossbar_xinput_.SetUserIndex(static_cast<uint8_t>(user));
        this->Observe(controller_backend::xinput, ::GetTickCount64(), true, false);
        this->status_.suspended = !this->Foreground() || this->status_.enumeration_disabled;
        this->RefreshCrossbar();
        const bool wasCaptured = this->Captured();
        const auto now = ::GetTickCount64();
        const bool blocked = !this->Enabled(controller_backend::xinput) || !this->Foreground();
        const auto dispatch = [this](const controller_event& event) { return this->Dispatch(event); };
        auto& gamepad = state->Gamepad;
        const bool crossbarEligible = this->CrossbarEligible(controller_backend::xinput);
        auto crossbarValues = this->crossbar_xinput_.Values();
        for (uint32_t field = 0; field < xinput_masks.size(); ++field) crossbarValues[field] = (gamepad.wButtons & xinput_masks[field]) != 0;
        crossbarValues[16] = gamepad.bLeftTrigger;
        crossbarValues[17] = gamepad.bRightTrigger;
        this->crossbar_xinput_.State(crossbarValues, now, blocked, crossbarEligible, dispatch);
        const std::array<SHORT*, 4> axes{&gamepad.sThumbLX, &gamepad.sThumbLY, &gamepad.sThumbRX, &gamepad.sThumbRY};
        for (uint32_t index = 0; index < 4; ++index)
        {
            this->xinput_.State(18 + index, *axes[index], now, blocked, wasCaptured, dispatch);
        }
        for (uint32_t field = 0; field < xinput_masks.size(); ++field)
        {
            const auto value = (gamepad.wButtons & xinput_masks[field]) != 0;
            this->xinput_.State(field, value, now,
                blocked || this->crossbar_xinput_.Suppress(field, value, crossbarEligible), wasCaptured, dispatch);
        }
        this->xinput_.State(16, gamepad.bLeftTrigger, now,
            blocked || this->crossbar_xinput_.Suppress(16, gamepad.bLeftTrigger, crossbarEligible), wasCaptured, dispatch);
        this->xinput_.State(17, gamepad.bRightTrigger, now,
            blocked || this->crossbar_xinput_.Suppress(17, gamepad.bRightTrigger, crossbarEligible), wasCaptured, dispatch);
        const bool captured = wasCaptured || this->Captured();
        for (uint32_t index = 0; index < 4; ++index)
            if (this->xinput_.Suppress(18 + index, *axes[index], captured)) *axes[index] = 0;
        for (uint32_t field = 0; field < xinput_masks.size(); ++field)
            if (this->crossbar_xinput_.Suppress(field, gamepad.wButtons & xinput_masks[field], crossbarEligible) ||
                this->xinput_.Suppress(field, gamepad.wButtons & xinput_masks[field], captured)) gamepad.wButtons &= ~xinput_masks[field];
        if (this->crossbar_xinput_.Suppress(16, gamepad.bLeftTrigger, crossbarEligible) || this->xinput_.Suppress(16, gamepad.bLeftTrigger, captured)) gamepad.bLeftTrigger = 0;
        if (this->crossbar_xinput_.Suppress(17, gamepad.bRightTrigger, crossbarEligible) || this->xinput_.Suppress(17, gamepad.bRightTrigger, captured)) gamepad.bRightTrigger = 0;
    }
}
