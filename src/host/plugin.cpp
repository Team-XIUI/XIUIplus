#include "plugin.hpp"
#include "world_projection.hpp"
#include "xiui_version.hpp"

#include <cmath>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <MinHook.h>

namespace
{
    constexpr char native_build_id[] = __DATE__ " " __TIME__;
    using set_scene_target_fn=HRESULT(__stdcall*)(IDirect3DDevice8*,IDirect3DSurface8*,IDirect3DSurface8*);
    set_scene_target_fn set_scene_target_original{};

    auto is_command(const char* command, const char* name) -> bool
    {
        if (command == nullptr || name == nullptr)
            return false;

        const auto length = std::strlen(name);
        if (::_strnicmp(command, name, length) != 0)
            return false;

        const auto next = command[length];
        return next == '\0' || std::isspace(static_cast<unsigned char>(next)) != 0;
    }

    auto job_name(const uint8_t job) -> const char*
    {
        static constexpr const char* jobs[] = {
            "NON", "WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG", "SAM",
            "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN"};
        return job < (sizeof(jobs) / sizeof(jobs[0])) ? jobs[job] : "UNK";
    }

    auto copy_text(char* destination, const size_t destinationSize, const char* source, const char* fallback) -> void
    {
        const char* value = source != nullptr && source[0] != '\0' ? source : fallback;
        ::strncpy_s(destination, destinationSize, value, _TRUNCATE);
    }
}

namespace xiui
{
    callback_gate<plugin> plugin::callback_gate_{};

    plugin::plugin(void) = default;
    plugin::~plugin(void) = default;

    auto plugin::GetName(void) const -> const char*
    {
        return "XIUI";
    }

    auto plugin::GetAuthor(void) const -> const char*
    {
        return "Team XIUI";
    }

    auto plugin::GetDescription(void) const -> const char*
    {
        return "In-client custom Direct3D8 HUD renderer for Final Fantasy XI.";
    }

    auto plugin::GetLink(void) const -> const char*
    {
        return "https://github.com/tirem/XIUI";
    }

    auto plugin::GetVersion(void) const -> double
    {
        return plugin_version;
    }

    auto plugin::GetInterfaceVersion(void) const -> double
    {
        return ASHITA_INTERFACE_VERSION;
    }

    auto plugin::GetPriority(void) const -> int32_t
    {
        // Observe native world draws before replacement renderers consume them.
        return 50;
    }

    auto plugin::GetFlags(void) const -> uint32_t
    {
        return static_cast<uint32_t>(Ashita::PluginFlags::UseCommands) |
               static_cast<uint32_t>(Ashita::PluginFlags::UseDirect3D) |
               static_cast<uint32_t>(Ashita::PluginFlags::UsePackets);
    }

    auto plugin::Initialize(IAshitaCore* core, ILogManager* logger, const uint32_t id) -> bool
    {
        this->core_ = core;
        this->log_ = logger;
        this->id_ = id;
        this->keyboard_suppression_.Reset();
        const char* installPath = core != nullptr ? core->GetInstallPath() : ".";
        this->native_hud_.Initialize(core);
        const bool runtimeLoaded = this->runtime_.Initialize(core, installPath, &native_hud_controller::InvokeMacro, &this->native_hud_);
#if defined(XIUI_SINGLE_DLL)
        if (!runtimeLoaded)
        {
            this->native_hud_.Shutdown();
            this->WriteChat("[XIUI] Could not initialize the UI.");
            return false;
        }
#endif
        const bool chatAvailable = this->native_chat_.Initialize(this->runtime_);
        callback_gate_.Set(this);

        auto* input = core != nullptr ? core->GetInputManager() : nullptr;
        this->controller_.Initialize(core, this->runtime_);
        this->registered_controller_ = input != nullptr ? input->GetController() : nullptr;
        this->registered_xinput_ = input != nullptr ? input->GetXInput() : nullptr;
        if (this->registered_controller_ != nullptr)
            this->registered_controller_->AddCallback("xiuinative", &plugin::ControllerDataCallback, &plugin::ControllerStateCallback, &plugin::ControllerButtonCallback);
        if (this->registered_xinput_ != nullptr)
            this->registered_xinput_->AddCallback("xiuinative", &plugin::XInputStateCallback, &plugin::XInputButtonCallback);
        this->registered_mouse_ = input != nullptr ? input->GetMouse() : nullptr;
        this->registered_keyboard_ = input != nullptr ? input->GetKeyboard() : nullptr;
        if (this->registered_keyboard_ != nullptr)
            this->registered_keyboard_->AddCallback("xiuinative", &plugin::KeyboardDataCallback, &plugin::KeyboardStateCallback, &plugin::KeyboardCallback);
        if (this->registered_mouse_ != nullptr)
        {
            this->registered_mouse_->AddCallback("xiuinative", &plugin::MouseCallback);
            this->mouse_callback_registered_ = true;
        }

        char message[320]{};
        std::snprintf(
            message,
            sizeof(message),
#if defined(XIUI_SINGLE_DLL)
            "[XIUI] Loaded %s. Use /xiui edit to configure.",
            release_version);
#else
            "[XIUI] Stable host %s; runtime %s. Use /xiui edit to configure.",
            native_build_id,
            runtimeLoaded ? this->runtime_.GetBuildId().c_str() : "waiting for first publish");
#endif
        this->WriteChat(message);
        if (!chatAvailable) this->WriteChat("[XIUI] Native chat capture unavailable for this client; keep the game chat enabled.");
        return true;
    }

    auto plugin::Release(void) -> void
    {
        callback_gate_.Close();
        if (this->scene_target_hook_) ::MH_DisableHook(this->scene_target_hook_);
        if (this->mouse_callback_registered_ && this->registered_mouse_ != nullptr)
            this->registered_mouse_->RemoveCallback("xiuinative");
        if (this->registered_keyboard_ != nullptr) this->registered_keyboard_->RemoveCallback("xiuinative");
        if (this->registered_controller_ != nullptr) this->registered_controller_->RemoveCallback("xiuinative");
        if (this->registered_xinput_ != nullptr) this->registered_xinput_->RemoveCallback("xiuinative");
        callback_gate_.Wait();
        this->keyboard_suppression_.Reset();
        if (this->scene_target_hook_) ::MH_RemoveHook(this->scene_target_hook_);
        this->scene_target_hook_=nullptr;
        this->controller_.Shutdown();
        this->registered_controller_ = nullptr;
        this->registered_xinput_ = nullptr;
        if (this->keyboard_block_saved_ && this->registered_keyboard_ != nullptr)
            this->registered_keyboard_->SetBlockInput(this->previous_keyboard_blocked_);
        this->keyboard_block_saved_ = false;
        this->registered_keyboard_ = nullptr;
        this->native_hud_.Shutdown();
        this->native_chat_.Shutdown();
        if (this->scene_hook_minhook_owned_) ::MH_Uninitialize();
        this->scene_hook_minhook_owned_=false;
        this->scene_target_.Reset(); this->scene_depth_.Reset();
        this->runtime_.Shutdown();

        this->mouse_callback_registered_ = false;
        this->registered_mouse_ = nullptr;
        this->pending_device_.store(nullptr, std::memory_order_release);
        this->device_ = nullptr;
        this->core_ = nullptr;
        this->log_ = nullptr;
        this->id_ = 0;
        this->runtime_fault_reported_ = false;
    }

    auto plugin::HandleEvent(const char* eventName, const void* eventData, const uint32_t eventSize) -> void
    {
        UNREFERENCED_PARAMETER(eventName);
        UNREFERENCED_PARAMETER(eventData);
        UNREFERENCED_PARAMETER(eventSize);
    }

    auto plugin::HandleCommand(int32_t mode, const char* command, bool injected) -> bool
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(injected);

        if (is_command(command, "/satchel") || is_command(command, "/satch"))
            return this->runtime_.HandleCommand(command);

        if (is_command(command, "/readycheck"))
        {
            const char* action = command + 11;
            while (std::isspace(static_cast<unsigned char>(*action)) != 0) ++action;
            const auto request = std::string{"readycheck "} + (*action == '\0' ? "start" : action);
            this->runtime_.SetVisible(true);
            if (!this->runtime_.HandleCommand(request.c_str())) this->WriteChat("[XIUI] Ready Check is unavailable.");
            return true;
        }

        if (!is_command(command, "/xiui") && !is_command(command, "/xiuin") && !is_command(command, "/xiuinative") && !is_command(command, "/xiui-native"))
            return false;

        const auto* argument = command;
        while (*argument != '\0' && std::isspace(static_cast<unsigned char>(*argument)) == 0)
            ++argument;
        while (std::isspace(static_cast<unsigned char>(*argument)) != 0)
            ++argument;

        if (*argument == '\0' || is_command(argument, "edit"))
        {
            this->runtime_.SetVisible(true);
            if (!this->runtime_.ToggleEdit())
                this->WriteChat("[XIUI] Edit mode is unavailable because Ashita mouse input could not be captured.");
            else
                this->WriteChat(this->runtime_.IsEditing()
                    ? "[XIUI] Edit mode enabled. Drag modules or adjust their settings in Configuration."
                    : "[XIUI] Edit mode disabled; layout saved.");
            return true;
        }

        if (is_command(argument, "show") || is_command(argument, "start"))
        {
            this->runtime_.SetVisible(true);
            this->WriteChat("[XIUI] In-client HUD shown.");
            return true;
        }

        if (is_command(argument, "hide") || is_command(argument, "stop"))
        {
            this->runtime_.SetVisible(false);
            this->WriteChat("[XIUI] In-client HUD hidden.");
            return true;
        }

        if (is_command(argument, "reload") || is_command(argument, "restart"))
        {
            this->runtime_.ReloadSettings();
            this->WriteChat("[XIUI] Settings reloaded.");
            return true;
        }

        if (is_command(argument, "reset"))
        {
            this->runtime_.ResetSettings();
            this->WriteChat("[XIUI] Settings and panel layout reset.");
            return true;
        }

        if (is_command(argument, "status") || is_command(argument, "debug"))
        {
            const bool runtimeLoaded = this->runtime_.IsLoaded();
            const std::string runtimeBuild = this->runtime_.GetBuildId();
            char message[384]{};
            std::snprintf(
                message,
                sizeof(message),
                "[XIUI] Host %s; runtime %s; text %s; HUD %s; edit mode %s.",
                native_build_id,
                runtimeLoaded ? runtimeBuild.c_str() : "unavailable",
                runtimeLoaded ? (this->runtime_.IsGpuShaderActive() ? "GPU shader" : "fixed-function fallback") : "unavailable",
                this->runtime_.IsVisible() ? "visible" : "hidden",
                this->runtime_.IsEditing() ? "enabled" : "disabled");
            this->WriteChat(message);
            return true;
        }

        if (is_command(argument, "help"))
        {
            this->WriteChat("[XIUI] /xiui edit|save|show|hide|reload|reset|status");
            this->WriteChat("[XIUI] /xiui notify test|clear|status; /xiui container preview|hide; /xiui gamehud");
            this->WriteChat("[XIUI] /xiui commands; /xiui commands status; /xiui theme status");
            this->WriteChat("[XIUI] /readycheck [yes|no|close|status]; /xiui readycheck for settings");
            return true;
        }

        if (::_stricmp(argument, "gamehud status") == 0)
        {
            this->WriteChat(this->native_hud_.Status().c_str());
            return true;
        }

        if (::_stricmp(argument, "chat capture") == 0)
        {
            this->WriteChat(this->runtime_.DescribeNpcCapture().c_str());
            return true;
        }
        if (::_stricmp(argument, "rings host") == 0)
        {
            char message[320]{};
            std::snprintf(message,sizeof(message),
                "[XIUI] Target rings host: camera %s; overlay %s; depth format %u; opaque draws %u; opaque after overlay %u; late frames %llu/%llu; boundary hook %s.",
                this->last_scene_camera_,this->last_world_overlay_,static_cast<unsigned>(this->last_scene_depth_format_),this->last_scene_opaque_draws_,this->last_scene_opaque_after_overlay_,
                static_cast<unsigned long long>(this->world_overlay_late_frames_),static_cast<unsigned long long>(this->world_overlay_frames_),this->scene_hook_status_);
            this->WriteChat(message);
            const auto& probe=this->last_ring_probe_.frames ? this->last_ring_probe_ : this->ring_probe_;
            std::snprintf(message,sizeof(message),
                "[XIUI] Ring probe %u frames: camera changes %u/%u (max %.6g); target moves %u (max %.6f); aspect mismatches %u; missing overlay %u; missing scene end %u.",
                probe.frames,probe.camera_changes,probe.camera_checks,static_cast<double>(probe.camera_delta),
                probe.target_moves,static_cast<double>(probe.target_delta),probe.aspect_mismatches,probe.overlay_misses,probe.scene_end_misses);
            this->WriteChat(message);
            std::snprintf(message,sizeof(message),
                "[XIUI] Ring depth: mode %lu; function %lu; bias %lu; projection %.9g %.9g %.9g %.9g; viewport %lux%lu; scene %ux%u.",
                static_cast<unsigned long>(this->scene_depth_mode_),static_cast<unsigned long>(this->scene_depth_function_),
                static_cast<unsigned long>(this->scene_depth_bias_),static_cast<double>(this->scene_projection_._33),
                static_cast<double>(this->scene_projection_._34),static_cast<double>(this->scene_projection_._43),static_cast<double>(this->scene_projection_._44),
                static_cast<unsigned long>(this->scene_viewport_.Width),static_cast<unsigned long>(this->scene_viewport_.Height),
                this->scene_target_width_,this->scene_target_height_);
            this->WriteChat(message);
            return true;
        }
        if (this->runtime_.HandleCommand(argument))
            return true;
        this->WriteChat("[XIUI] Unknown command. Use /xiui help.");
        return true;
    }

    auto plugin::HandleIncomingText(int32_t mode, bool indent, const char* message, int32_t* modifiedMode, bool* modifiedIndent, char* modifiedMessage, bool injected, bool blocked) -> bool
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(indent);
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(modifiedMode);
        UNREFERENCED_PARAMETER(modifiedIndent);
        UNREFERENCED_PARAMETER(modifiedMessage);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
        return false;
    }

    auto plugin::HandleOutgoingText(int32_t mode, const char* message, int32_t* modifiedMode, char* modifiedMessage, bool injected, bool blocked) -> bool
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(modifiedMode);
        UNREFERENCED_PARAMETER(modifiedMessage);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
        return false;
    }

    auto plugin::HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked) -> bool
    {
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        if (!injected && !blocked && (id == 0x000A || id == 0x000B || id == 0x0017 || id == 0x001D || id == 0x001E || id == 0x001F || id == 0x0020 || id == 0x0021 || id == 0x0096 || id == 0x0097 ||
            id == 0x0028 || id == 0x0029 || id == 0x002D || id == 0x0050 || id == 0x0055 || id == 0x00A0 || id == 0x00D2 || id == 0x00D3 || id == 0x00DC))
            this->runtime_.HandlePacket(id, data, size);
        return false;
    }

    auto plugin::HandleOutgoingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked) -> bool
    {
        UNREFERENCED_PARAMETER(id);
        UNREFERENCED_PARAMETER(size);
        UNREFERENCED_PARAMETER(data);
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
        return false;
    }

    auto plugin::Direct3DInitialize(IDirect3DDevice8* device) -> bool
    {
        // Ashita can initialize plugins on its loader thread.
        this->pending_device_.store(device, std::memory_order_release);
        return true;
    }

    auto plugin::ApplyPendingDevice(void) -> void
    {
        if (auto* device = this->pending_device_.exchange(nullptr, std::memory_order_acq_rel))
        {
            this->device_ = device;
            this->runtime_.SetDevice(device);
            this->InstallSceneTargetHook();
        }
    }

    auto plugin::InstallSceneTargetHook(void) -> void
    {
        if (!this->device_ || this->scene_target_hook_) return;
        auto* site=(*reinterpret_cast<void***>(this->device_))[31];
        HMODULE owner{};
        if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(site),&owner) || owner!=::GetModuleHandleW(L"Ashita.dll"))
        { this->scene_hook_status_="outer wrapper unavailable"; return; }
        const auto initialized=::MH_Initialize();
        if (initialized!=MH_OK && initialized!=MH_ERROR_ALREADY_INITIALIZED)
        { this->scene_hook_status_="initialization failed"; this->WriteChat("[XIUI] Target ring boundary hook initialization failed."); return; }
        this->scene_hook_minhook_owned_=initialized==MH_OK;
        if (::MH_CreateHook(site,&SceneTargetCallback,reinterpret_cast<void**>(&set_scene_target_original))!=MH_OK)
        { this->scene_hook_status_="creation failed"; this->WriteChat("[XIUI] Target ring boundary hook creation failed."); return; }
        if (::MH_EnableHook(site)!=MH_OK)
        { ::MH_RemoveHook(site); this->scene_hook_status_="enable failed"; this->WriteChat("[XIUI] Target ring boundary hook enable failed."); return; }
        this->scene_target_hook_=site;
        this->scene_hook_status_="ready";
        this->WriteChat("[XIUI] Target ring boundary hook ready (Ashita.dll).");
    }

    auto __stdcall plugin::SceneTargetCallback(IDirect3DDevice8* device, IDirect3DSurface8* color, IDirect3DSurface8* depth) -> HRESULT
    {
        auto callback=callback_gate_.Enter();
        if (callback && device==callback->device_ && color!=callback->scene_target_.Get())
        {
            try
            {
                const bool drawn=callback->world_overlay_drawn_;
                callback->DrawWorldOverlay(true);
                if (!drawn && callback->world_overlay_drawn_) callback->world_overlay_at_switch_=true;
            }
            catch (...) {}
        }
        return set_scene_target_original(device,color,depth);
    }

    auto plugin::Direct3DBeginScene(const bool isRenderingBackBuffer) -> void
    {
        UNREFERENCED_PARAMETER(isRenderingBackBuffer);
        this->ApplyPendingDevice();
    }

    auto plugin::Direct3DEndScene(const bool isRenderingBackBuffer) -> void
    {
        this->ApplyPendingDevice();
        this->DrawWorldOverlay(true);
        if (!isRenderingBackBuffer)
            return;

        std::string reloadMessage;
        const bool runtimeChanged = this->runtime_.PollReload(reloadMessage);
        this->controller_.Tick(runtimeChanged);
        if (runtimeChanged)
        {
            this->keyboard_suppression_.Suspend();
            this->WriteChat(reloadMessage.c_str());
            if (!this->runtime_.HasFaulted())
                this->runtime_fault_reported_ = false;
        }

        if (!this->IsLoggedIn())
        {
            this->keyboard_suppression_.Suspend();
            this->runtime_.ExitEdit();
            this->UpdateKeyboardCapture();
            return;
        }

        hud_snapshot snapshot{};
        if (::GetTickCount64() >= this->next_palette_update_)
        {
            this->chat_colors_ = this->native_chat_.Palette();
            this->next_palette_update_ = ::GetTickCount64() + 500;
        }
        snapshot.chat_colors = this->chat_colors_;
        snapshot.chat_capture_available = this->native_chat_.IsAvailable();
        snapshot.native_ui = this->native_hud_.UiState();
        if (this->BuildSnapshot(snapshot) && !this->runtime_.Render(snapshot) && this->runtime_.HasFaulted() && !this->runtime_fault_reported_)
        {
            this->runtime_fault_reported_ = true;
            this->WriteChat("[XIUI] Runtime faulted and was quarantined; publish a new runtime generation to recover.");
        }
        this->UpdateKeyboardCapture();
    }

    auto plugin::Direct3DPresent(const RECT* sourceRect, const RECT* destinationRect, HWND destinationWindow, const RGNDATA* dirtyRegion) -> void
    {
        UNREFERENCED_PARAMETER(sourceRect);
        UNREFERENCED_PARAMETER(destinationRect);
        UNREFERENCED_PARAMETER(destinationWindow);
        UNREFERENCED_PARAMETER(dirtyRegion);
        this->native_hud_.Update(this->runtime_.NativeHudHiddenMask(), this->runtime_.MenuLayout());
        D3DSURFACE_DESC depthDesc{};
        this->last_scene_depth_format_=this->scene_depth_ && SUCCEEDED(this->scene_depth_->GetDesc(&depthDesc)) ? depthDesc.Format : D3DFMT_UNKNOWN;
        this->last_scene_camera_=this->scene_world_camera_valid_ ? "terrain" : this->scene_camera_valid_ ? "fixed-function" : "missing";
        this->last_world_overlay_=this->world_overlay_drawn_ ? (this->world_overlay_at_switch_ ? "target boundary" : "scene end") : "none";
        this->last_scene_opaque_draws_=this->scene_opaque_draws_;
        this->last_scene_opaque_after_overlay_=this->scene_opaque_after_overlay_;
        if (this->scene_world_camera_valid_)
        {
            ++this->ring_probe_.frames;
            this->ring_probe_.overlay_misses+=!this->world_overlay_drawn_;
            this->ring_probe_.scene_end_misses+=!this->scene_end_seen_;
            if (this->ring_probe_.frames==256)
            {
                this->last_ring_probe_=this->ring_probe_;
                this->ring_probe_={};
            }
        }
        if (this->world_overlay_drawn_)
        {
            ++this->world_overlay_frames_;
            if (this->scene_opaque_after_overlay_!=0) ++this->world_overlay_late_frames_;
        }
        this->scene_camera_valid_ = false;
        this->scene_world_camera_valid_ = false;
        this->scene_end_seen_=false;
        this->scene_target_position_id_=0;
        this->world_overlay_drawn_ = false;
        this->world_overlay_at_end_ = false;
        this->world_overlay_at_switch_ = false;
        this->scene_opaque_draws_=this->scene_opaque_after_overlay_=0;
        this->scene_target_.Reset(); this->scene_depth_.Reset();
    }

    auto plugin::Direct3DDrawPrimitive(D3DPRIMITIVETYPE primitiveType, UINT startVertex, UINT primitiveCount) -> bool
    {
        this->CaptureSceneCamera();
        this->DrawWorldOverlay();
        UNREFERENCED_PARAMETER(primitiveType);
        UNREFERENCED_PARAMETER(startVertex);
        UNREFERENCED_PARAMETER(primitiveCount);
        return false;
    }

    auto plugin::Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT startIndex, UINT primitiveCount) -> bool
    {
        this->CaptureSceneCamera();
        this->DrawWorldOverlay();
        UNREFERENCED_PARAMETER(primitiveType);
        UNREFERENCED_PARAMETER(minIndex);
        UNREFERENCED_PARAMETER(vertexCount);
        UNREFERENCED_PARAMETER(startIndex);
        UNREFERENCED_PARAMETER(primitiveCount);
        return false;
    }

    auto plugin::Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT primitiveCount, const void* vertexData, UINT vertexStride) -> bool
    {
        this->CaptureSceneCamera();
        this->DrawWorldOverlay();
        UNREFERENCED_PARAMETER(primitiveType);
        UNREFERENCED_PARAMETER(primitiveCount);
        UNREFERENCED_PARAMETER(vertexData);
        UNREFERENCED_PARAMETER(vertexStride);
        return false;
    }

    auto plugin::Direct3DDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT primitiveCount, const void* indexData, D3DFORMAT indexFormat, const void* vertexData, UINT vertexStride) -> bool
    {
        this->CaptureSceneCamera();
        this->DrawWorldOverlay();
        UNREFERENCED_PARAMETER(primitiveType);
        UNREFERENCED_PARAMETER(minIndex);
        UNREFERENCED_PARAMETER(vertexCount);
        UNREFERENCED_PARAMETER(primitiveCount);
        UNREFERENCED_PARAMETER(indexData);
        UNREFERENCED_PARAMETER(indexFormat);
        UNREFERENCED_PARAMETER(vertexData);
        UNREFERENCED_PARAMETER(vertexStride);
        return false;
    }

    auto __stdcall plugin::MouseCallback(const uint32_t message, WPARAM wParam, LPARAM lParam, const bool blocked) -> BOOL
    {
        auto callback = callback_gate_.Enter();
        if (!callback)
            return FALSE;
        try
        {
            const bool consumed = callback->runtime_.HandleMouseMessage(message, wParam, lParam, blocked);
            callback->UpdateKeyboardCapture();
            return consumed ? TRUE : FALSE;
        }
        catch (...)
        {
            return FALSE;
        }
    }

    auto __stdcall plugin::ControllerButtonCallback(uint32_t* field, int32_t* state, const bool injected, const bool blocked) -> BOOL
    {
        auto callback = callback_gate_.Enter();
        if (!callback || field == nullptr || state == nullptr) return FALSE;
        try { return callback->controller_.DirectInputButton(*field, *state, injected, blocked) ? TRUE : FALSE; }
        catch (...) { return FALSE; }
    }

    auto __stdcall plugin::ControllerStateCallback(LPDIRECTINPUTDEVICE8A device, const DWORD size, LPVOID data) -> HRESULT
    {
        auto callback = callback_gate_.Enter();
        if (callback) try { callback->controller_.DirectInputState(device, size, data); } catch (...) {}
        return DI_OK;
    }

    auto __stdcall plugin::ControllerDataCallback(LPDIRECTINPUTDEVICE8A device, const DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD, const DWORD result) -> HRESULT
    {
        auto callback = callback_gate_.Enter();
        if (callback && SUCCEEDED(static_cast<HRESULT>(result)))
            try { callback->controller_.DirectInputData(device, objectSize, data, count); } catch (...) {}
        return DI_OK;
    }

    auto __stdcall plugin::XInputButtonCallback(uint8_t* field, int16_t* state, const bool injected, const bool blocked) -> BOOL
    {
        auto callback = callback_gate_.Enter();
        if (!callback || field == nullptr || state == nullptr) return FALSE;
        try { return callback->controller_.XInputButton(*field, *state, injected, blocked) ? TRUE : FALSE; }
        catch (...) { return FALSE; }
    }

    auto __stdcall plugin::XInputStateCallback(const DWORD user, XINPUT_STATE* state) -> DWORD
    {
        auto callback = callback_gate_.Enter();
        if (callback) try { callback->controller_.XInputState(user, state); } catch (...) {}
        return ERROR_SUCCESS;
    }

    auto plugin::UpdateKeyboardCapture(void) -> void
    {
        if (this->registered_keyboard_ == nullptr) return;
        const bool foreground = this->KeyboardForeground();
        if (!foreground) this->keyboard_suppression_.Suspend();
        if (foreground && this->runtime_.ChatFocused())
        {
            if (!this->keyboard_block_saved_)
            {
                this->previous_keyboard_blocked_ = this->registered_keyboard_->GetBlockInput();
                this->keyboard_block_saved_ = true;
            }
            this->registered_keyboard_->SetBlockInput(true);
        }
        else if (this->keyboard_block_saved_)
        {
            this->registered_keyboard_->SetBlockInput(this->previous_keyboard_blocked_);
            this->keyboard_block_saved_ = false;
        }
    }

    auto plugin::KeyboardForeground(void) const -> bool
    {
        auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        const auto window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
        return window != nullptr && ::GetForegroundWindow() == window;
    }

    auto __stdcall plugin::KeyboardStateCallback(LPDIRECTINPUTDEVICE8A, const DWORD size, LPVOID data) -> HRESULT
    {
        auto callback = callback_gate_.Enter();
        if (!callback || data == nullptr || size != 256) return DI_OK;
        const bool foreground = callback->KeyboardForeground();
        if (!foreground) callback->keyboard_suppression_.Suspend();
        callback->keyboard_suppression_.FilterState(static_cast<uint8_t*>(data), size, foreground && callback->runtime_.ChatFocused());
        return DI_OK;
    }

    auto __stdcall plugin::KeyboardDataCallback(LPDIRECTINPUTDEVICE8A, const DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count, const DWORD flags, const DWORD result) -> HRESULT
    {
        auto callback = callback_gate_.Enter();
        if (!callback || FAILED(static_cast<HRESULT>(result)) || data == nullptr || count == nullptr || *count > 4096 || objectSize < 8 || objectSize > sizeof(DIDEVICEOBJECTDATA)) return DI_OK;
        const bool foreground = callback->KeyboardForeground();
        if (!foreground) callback->keyboard_suppression_.Suspend();
        callback->keyboard_suppression_.FilterData(data, objectSize, *count, foreground && callback->runtime_.ChatFocused(),
            (flags & DIGDD_PEEK) != 0, static_cast<int32_t>(result));
        return DI_OK;
    }

    auto __stdcall plugin::KeyboardCallback(const WPARAM key, const LPARAM flags, const bool blocked) -> BOOL
    {
        auto callback = callback_gate_.Enter();
        if (!callback || key >= 256) return FALSE;
        try
        {
            const auto bits = static_cast<uint32_t>(flags);
            const bool released = (bits & 0x80000000u) != 0;
            const bool repeated = (bits & 0x40000000u) != 0;
            const bool foreground = callback->KeyboardForeground();
            if (!foreground) callback->keyboard_suppression_.Suspend();
            bool consumed = false;
            try { if (foreground) consumed = callback->runtime_.HandleKey(key, flags, blocked); }
            catch (...) {}
            const auto fallback = callback->registered_keyboard_ != nullptr ? callback->registered_keyboard_->V2D(static_cast<uint32_t>(key)) : 256u;
            const auto scan = keyboard_suppression::ScanCode(bits, fallback);
            const bool suppress = callback->keyboard_suppression_.Observe(scan, !released, repeated, consumed);
            callback->UpdateKeyboardCapture();
            return suppress ? TRUE : FALSE;
        }
        catch (...) { callback->UpdateKeyboardCapture(); return FALSE; }
    }

    auto plugin::CaptureSceneCamera(void) -> void
    {
        if (this->device_ == nullptr) return;
        DWORD shader{},depth{},write{},blend{};
        if (FAILED(this->device_->GetVertexShader(&shader)) || FAILED(this->device_->GetRenderState(D3DRS_ZENABLE,&depth)) ||
            FAILED(this->device_->GetRenderState(D3DRS_ZWRITEENABLE,&write)) || FAILED(this->device_->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend)) ||
            !depth || !write || blend) return;
        bool world = false;
        if ((shader & 1) != 0)
        {
            static constexpr DWORD worldDeclaration[]{536870912,1073872896,1073872899,1074003973,1073807367,1073872911,4294967295};
            DWORD declaration[16]{}, bytes=sizeof(declaration);
            world = SUCCEEDED(this->device_->GetVertexShaderDeclaration(shader,declaration,&bytes)) &&
                bytes==sizeof(worldDeclaration) && std::memcmp(declaration,worldDeclaration,bytes)==0;
            if (!world) return;
        }
        else if ((shader & D3DFVF_POSITION_MASK)!=D3DFVF_XYZ || (shader & D3DFVF_NORMAL)==0) return;
        Microsoft::WRL::ComPtr<IDirect3DSurface8> target;
        if (FAILED(this->device_->GetRenderTarget(&target))) return;
        if (this->scene_target_ && target==this->scene_target_)
        {
            ++this->scene_opaque_draws_;
            if (this->world_overlay_drawn_) ++this->scene_opaque_after_overlay_;
        }
        if (this->scene_world_camera_valid_)
        {
            if (world && target==this->scene_target_)
            {
                D3DMATRIX view{},projection{};
                if (SUCCEEDED(this->device_->GetTransform(D3DTS_VIEW,&view)) && SUCCEEDED(this->device_->GetTransform(D3DTS_PROJECTION,&projection)))
                {
                    float difference{};
                    for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c)
                    {
                        difference=std::max(difference,std::abs(view.m[r][c]-this->scene_view_.m[r][c]));
                        difference=std::max(difference,std::abs(projection.m[r][c]-this->scene_projection_.m[r][c]));
                    }
                    ++this->ring_probe_.camera_checks;
                    this->ring_probe_.camera_changes+=difference>0.000001f;
                    this->ring_probe_.camera_delta=std::max(this->ring_probe_.camera_delta,difference);
                }
            }
            return;
        }
        if (!world && this->scene_target_ && target!=this->scene_target_) return;
        D3DMATRIX projection{};
        if (FAILED(this->device_->GetTransform(D3DTS_PROJECTION, &projection)) ||
            std::abs(projection._44) > 0.001f || std::abs(projection._34) < 0.001f)
            return;
        D3DMATRIX view{};
        D3DVIEWPORT8 viewport{};
        if (FAILED(this->device_->GetTransform(D3DTS_VIEW, &view)) || FAILED(this->device_->GetViewport(&viewport)))
            return;
        // The sky uses a rotation-only view before the translated world camera.
        if (std::abs(view._41) + std::abs(view._42) + std::abs(view._43) < 0.001f)
            return;
        // Fixed-function draws are provisional; the actual terrain program owns the scene camera.
        if (!this->scene_target_ || target!=this->scene_target_) ++this->scene_opaque_draws_;
        this->scene_target_=target;
        this->device_->GetDepthStencilSurface(this->scene_depth_.ReleaseAndGetAddressOf());
        this->scene_view_ = view;
        this->scene_projection_ = projection;
        this->scene_viewport_ = viewport;
        this->scene_camera_valid_ = true;
        this->scene_world_camera_valid_ = world;
        if (world)
        {
            this->scene_depth_mode_=depth;
            this->device_->GetRenderState(D3DRS_ZFUNC,&this->scene_depth_function_);
            this->device_->GetRenderState(D3DRS_ZBIAS,&this->scene_depth_bias_);
            D3DSURFACE_DESC scene{};
            if (SUCCEEDED(target->GetDesc(&scene)))
            {
                this->scene_target_width_=scene.Width;
                this->scene_target_height_=scene.Height;
            }
            this->scene_target_position_id_=this->ReadRingTargetPosition(this->scene_target_position_);
            Microsoft::WRL::ComPtr<IDirect3DSurface8> backbuffer;
            D3DSURFACE_DESC back{};
            if (SUCCEEDED(this->device_->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&backbuffer)) && SUCCEEDED(backbuffer->GetDesc(&back)) && viewport.Height && back.Height)
            {
                const double aspect=static_cast<double>(viewport.Width)/viewport.Height;
                const double expected=static_cast<double>(back.Width)/back.Height;
                this->ring_probe_.aspect_mismatches+=std::abs(aspect-expected)>expected*0.03;
            }
        }
    }

    auto plugin::ReadRingTargetPosition(std::array<float,3>& position) const -> uint32_t
    {
        auto* memory=this->core_ ? this->core_->GetMemoryManager() : nullptr;
        auto* target=memory ? memory->GetTarget() : nullptr;
        auto* entity=memory ? memory->GetEntity() : nullptr;
        if (!target || !entity) return 0;
        const auto index=target->GetTargetIndex(0);
        if (!index || index>=entity->GetEntityMapSize()) return 0;
        position={entity->GetLocalPositionX(index),entity->GetLocalPositionZ(index),entity->GetLocalPositionY(index)};
        for (float value : position) if (!std::isfinite(value)) return 0;
        return entity->GetServerId(index);
    }

    auto plugin::DrawWorldOverlay(bool sceneEnd) -> void
    {
        if ((this->world_overlay_drawn_ && !sceneEnd) || !this->scene_world_camera_valid_ || !this->device_) return;
        DWORD shader{};
        if (!sceneEnd && (FAILED(this->device_->GetVertexShader(&shader)) || (shader & 1)!=0 ||
            (shader & D3DFVF_POSITION_MASK)!=D3DFVF_XYZRHW)) return;
        Microsoft::WRL::ComPtr<IDirect3DSurface8> current, depth, source;
        Microsoft::WRL::ComPtr<IDirect3DBaseTexture8> texture;
        if (!this->scene_target_ || !this->scene_depth_ || FAILED(this->device_->GetRenderTarget(&current))) return;
        if (sceneEnd) {
            if (current!=this->scene_target_ || FAILED(this->device_->GetDepthStencilSurface(&depth)) || depth!=this->scene_depth_) return;
            this->scene_end_seen_=true;
        }
        if (this->world_overlay_drawn_) return;
        this->device_->GetTexture(0,&texture);
        bool renderTexture=false;
        if (texture && texture->GetType()==D3DRTYPE_TEXTURE)
        {
            D3DSURFACE_DESC desc{};
            auto* image=static_cast<IDirect3DTexture8*>(texture.Get());
            renderTexture=SUCCEEDED(image->GetLevelDesc(0,&desc)) && (desc.Usage&D3DUSAGE_RENDERTARGET)!=0;
            if (renderTexture) image->GetSurfaceLevel(0,&source);
        }
        // The verified scene copy follows all world passes; early screen labels do not.
        if (!sceneEnd && (current==this->scene_target_ || source!=this->scene_target_)) return;
        std::array<float,3> position{};
        if (this->scene_target_position_id_ && this->ReadRingTargetPosition(position)==this->scene_target_position_id_)
        {
            float squared{};
            for (unsigned c=0;c<3;++c) squared+=(position[c]-this->scene_target_position_[c])*(position[c]-this->scene_target_position_[c]);
            const float delta=std::sqrt(squared);
            this->ring_probe_.target_moves+=delta>0.000001f;
            this->ring_probe_.target_delta=std::max(this->ring_probe_.target_delta,delta);
        }
        this->world_overlay_drawn_=true;
        this->world_overlay_at_end_=sceneEnd;
        if (!this->IsLoggedIn()) return;
        hud_snapshot snapshot{}; snapshot.native_ui=this->native_hud_.UiState();
        if (this->BuildSnapshot(snapshot)) this->runtime_.RenderWorld(snapshot);
    }

    auto plugin::IsLoggedIn(void) const -> bool
    {
        if (this->core_ == nullptr)
            return false;
        auto* memory = this->core_->GetMemoryManager();
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        return player != nullptr && player->GetLoginStatus() == static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn);
    }

    auto plugin::BuildSnapshot(hud_snapshot& snapshot) const -> bool
    {
        snapshot.camera_valid = this->scene_camera_valid_;
        snapshot.scene_view = this->scene_view_;
        snapshot.scene_projection = this->scene_projection_;
        snapshot.scene_viewport = this->scene_viewport_;
        snapshot.scene_target = this->scene_target_.Get(); snapshot.scene_depth = this->scene_depth_.Get();
        auto hudViewport=this->scene_viewport_;
        if (this->device_) this->device_->GetViewport(&hudViewport);
        if (this->core_ == nullptr)
            return false;

        auto* memory = this->core_->GetMemoryManager();
        if (memory == nullptr)
            return false;

        auto* party = memory->GetParty();
        auto* player = memory->GetPlayer();
        auto* target = memory->GetTarget();
        auto* entity = memory->GetEntity();
        if (party == nullptr || player == nullptr || target == nullptr || entity == nullptr)
            return false;

        copy_text(snapshot.player_name, sizeof(snapshot.player_name), party->GetMemberName(0), "Player");
        std::snprintf(snapshot.main_job, sizeof(snapshot.main_job), "%s %u", job_name(player->GetMainJob()), static_cast<unsigned int>(player->GetMainJobLevel()));
        std::snprintf(snapshot.sub_job, sizeof(snapshot.sub_job), "%s %u", job_name(player->GetSubJob()), static_cast<unsigned int>(player->GetSubJobLevel()));
        snapshot.hp = party->GetMemberHP(0);
        snapshot.mp = party->GetMemberMP(0);
        snapshot.tp = party->GetMemberTP(0);
        snapshot.hp_max = std::max(snapshot.hp, player->GetHPMax());
        snapshot.mp_max = std::max(snapshot.mp, player->GetMPMax());
        const auto playerIndex = party->GetMemberTargetIndex(0);
        if (this->scene_camera_valid_ && playerIndex > 0 && playerIndex < entity->GetEntityMapSize())
        {
            const std::array<float, 3> bodyCenter{
                entity->GetLocalPositionX(playerIndex),
                entity->GetLocalPositionZ(playerIndex) - 0.75f,
                entity->GetLocalPositionY(playerIndex)};
            snapshot.player_screen_valid = ProjectWorldPoint(bodyCenter, this->scene_view_, this->scene_projection_, hudViewport,
                snapshot.player_screen_x, snapshot.player_screen_y);
        }
        if (this->scene_camera_valid_ && !player->GetIsZoning())
            for (uint32_t slot = 0; slot < snapshot.party_anchors.size(); ++slot)
            {
                const auto id = party->GetMemberServerId(slot), index = party->GetMemberTargetIndex(slot);
                if (!party->GetMemberIsActive(slot) || id == 0 || party->GetMemberZone(slot) != party->GetMemberZone(0) ||
                    index == 0 || index >= entity->GetEntityMapSize() || entity->GetServerId(index) != id) continue;
                const auto flags = entity->GetRenderFlags0(index);
                if ((flags & 0x200) == 0 || (flags & 0x4000) != 0) continue;
                auto& anchor = snapshot.party_anchors[slot]; anchor.id = id;
                const std::array<float, 3> ground{entity->GetLocalPositionX(index), entity->GetLocalPositionZ(index), entity->GetLocalPositionY(index)};
                anchor.valid = ProjectWorldPoint(ground, this->scene_view_, this->scene_projection_, hudViewport, anchor.x, anchor.y);
            }
        snapshot.native_hud_available_mask = this->native_hud_.AvailableMask();
        if (party->GetMemberIsActive(0) && !player->GetIsZoning())
            snapshot.fishing_stamina_percent = this->native_hud_.FishingStaminaPercent();
        snapshot.exp_current = player->GetExpCurrent();
        snapshot.exp_needed = std::max<uint32_t>(1, player->GetExpNeeded());

        const auto targetIndex = target->GetTargetIndex(0);
        const bool validIndex = targetIndex > 0 && targetIndex < entity->GetEntityMapSize();
        const char* targetName = validIndex ? entity->GetName(targetIndex) : nullptr;
        snapshot.target_valid = targetName != nullptr && targetName[0] != '\0';
        if (snapshot.target_valid)
        {
            copy_text(snapshot.target_name, sizeof(snapshot.target_name), targetName, "Target");
            snapshot.target_hp_percent = std::min<uint8_t>(100, entity->GetHPPercent(targetIndex));
            snapshot.target_distance = std::sqrt(std::max(0.0f, entity->GetDistance(targetIndex)));
            snapshot.target_server_id = entity->GetServerId(targetIndex);
            const auto spawnFlags = entity->GetSpawnFlags(targetIndex), renderFlags = entity->GetRenderFlags0(targetIndex);
            if (this->scene_camera_valid_ && !player->GetIsZoning() && snapshot.target_server_id != 0 &&
                (spawnFlags & 0x02) == 0 && (spawnFlags & 0x11) != 0 && (renderFlags & 0x200) != 0 && (renderFlags & 0x4000) == 0)
            {
                const std::array<float, 3> ground{entity->GetLocalPositionX(targetIndex), entity->GetLocalPositionZ(targetIndex), entity->GetLocalPositionY(targetIndex)};
                snapshot.target_screen_valid = ProjectWorldPoint(ground, this->scene_view_, this->scene_projection_, hudViewport,
                    snapshot.target_screen_x, snapshot.target_screen_y);
            }
        }
        else
        {
            copy_text(snapshot.target_name, sizeof(snapshot.target_name), nullptr, "No Target");
            snapshot.target_hp_percent = 0;
            snapshot.target_distance = 0.0f;
        }
        return true;
    }

    auto plugin::WriteChat(const char* message) const -> void
    {
        if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr && message != nullptr)
            this->core_->GetChatManager()->Write(1, false, message);
    }
} // namespace xiui

__declspec(dllexport) auto __stdcall expCreatePlugin(const char* arguments) -> IPlugin*
{
    UNREFERENCED_PARAMETER(arguments);
    return new xiui::plugin();
}

__declspec(dllexport) auto __stdcall expDestroyPlugin(void* instance) -> void
{
    delete static_cast<xiui::plugin*>(instance);
}

__declspec(dllexport) auto __stdcall expGetInterfaceVersion(void) -> double
{
    return ASHITA_INTERFACE_VERSION;
}
