#include "plugin.hpp"
#include "world_projection.hpp"
#include "xiui_version.hpp"

#include <cmath>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr char native_build_id[] = __DATE__ " " __TIME__;

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
        return "XIUI Native";
    }

    auto plugin::GetAuthor(void) const -> const char*
    {
        return "XIUI";
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
        return 1000;
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
        const char* installPath = core != nullptr ? core->GetInstallPath() : ".";
        this->native_hud_.Initialize(core);
        const bool runtimeLoaded = this->runtime_.Initialize(core, installPath);
#if defined(XIUI_SINGLE_DLL)
        if (!runtimeLoaded)
        {
            this->native_hud_.Shutdown();
            this->WriteChat("[XIUI Native] Could not initialize the UI.");
            return false;
        }
#endif
        const bool chatAvailable = this->native_chat_.Initialize(this->runtime_);
        callback_gate_.Set(this);

        auto* input = core != nullptr ? core->GetInputManager() : nullptr;
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
            "[XIUI Native] Loaded %s. Use /xiuin edit to configure.",
            release_version);
#else
            "[XIUI Native] Stable host %s; runtime %s. Use /xiuin edit to configure.",
            native_build_id,
            runtimeLoaded ? this->runtime_.GetBuildId().c_str() : "waiting for first publish");
#endif
        this->WriteChat(message);
        if (!chatAvailable) this->WriteChat("[XIUI Native] Native chat capture unavailable for this client; keep the game chat enabled.");
        return true;
    }

    auto plugin::Release(void) -> void
    {
        callback_gate_.Close();
        if (this->mouse_callback_registered_ && this->registered_mouse_ != nullptr)
            this->registered_mouse_->RemoveCallback("xiuinative");
        if (this->registered_keyboard_ != nullptr) this->registered_keyboard_->RemoveCallback("xiuinative");
        callback_gate_.Wait();
        if (this->keyboard_block_saved_ && this->registered_keyboard_ != nullptr)
            this->registered_keyboard_->SetBlockInput(this->previous_keyboard_blocked_);
        this->keyboard_block_saved_ = false;
        this->registered_keyboard_ = nullptr;
        this->native_hud_.Shutdown();
        this->native_chat_.Shutdown();
        this->runtime_.Shutdown();

        this->mouse_callback_registered_ = false;
        this->registered_mouse_ = nullptr;
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

        if (is_command(command, "/readycheck"))
        {
            const char* action = command + 11;
            while (std::isspace(static_cast<unsigned char>(*action)) != 0) ++action;
            const auto request = std::string{"readycheck "} + (*action == '\0' ? "start" : action);
            this->runtime_.SetVisible(true);
            if (!this->runtime_.HandleCommand(request.c_str())) this->WriteChat("[XIUI Native] Ready Check is unavailable.");
            return true;
        }

        if (!is_command(command, "/xiuin") && !is_command(command, "/xiuinative") && !is_command(command, "/xiui-native"))
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
                this->WriteChat("[XIUI Native] Edit mode is unavailable because Ashita mouse input could not be captured.");
            else
                this->WriteChat(this->runtime_.IsEditing()
                    ? "[XIUI Native] Edit mode enabled. Drag modules or adjust their settings in Configuration."
                    : "[XIUI Native] Edit mode disabled; layout saved.");
            return true;
        }

        if (is_command(argument, "show") || is_command(argument, "start"))
        {
            this->runtime_.SetVisible(true);
            this->WriteChat("[XIUI Native] In-client HUD shown.");
            return true;
        }

        if (is_command(argument, "hide") || is_command(argument, "stop"))
        {
            this->runtime_.SetVisible(false);
            this->WriteChat("[XIUI Native] In-client HUD hidden.");
            return true;
        }

        if (is_command(argument, "reload") || is_command(argument, "restart"))
        {
            this->runtime_.ReloadSettings();
            this->WriteChat("[XIUI Native] Native settings reloaded.");
            return true;
        }

        if (is_command(argument, "reset"))
        {
            this->runtime_.ResetSettings();
            this->WriteChat("[XIUI Native] Native settings and panel layout reset.");
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
                "[XIUI Native] Host %s; runtime %s; text %s; HUD %s; edit mode %s.",
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
            this->WriteChat("[XIUI Native] /xiuin edit|save|show|hide|reload|reset|status");
            this->WriteChat("[XIUI Native] /xiuin notify test|clear|status; /xiuin container preview|hide; /xiuin gamehud");
            this->WriteChat("[XIUI Native] /xiuin commands; /xiuin commands status; /xiuin theme status");
            this->WriteChat("[XIUI Native] /readycheck [yes|no|close|status]; /xiuin readycheck for settings");
            return true;
        }

        if (::_stricmp(argument, "gamehud status") == 0)
        {
            this->WriteChat(this->native_hud_.Status().c_str());
            return true;
        }
        if (this->runtime_.HandleCommand(argument))
            return true;
        this->WriteChat("[XIUI Native] Unknown command. Use /xiuin help.");
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
        if (!injected && !blocked && (id == 0x000A || id == 0x000B || id == 0x0017 || id == 0x001D || id == 0x0021 ||
            id == 0x0028 || id == 0x0029 || id == 0x002D || id == 0x0055 || id == 0x00D2 || id == 0x00D3 || id == 0x00DC))
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
        this->device_ = device;
        this->runtime_.SetDevice(device);
        return true;
    }

    auto plugin::Direct3DBeginScene(const bool isRenderingBackBuffer) -> void
    {
        UNREFERENCED_PARAMETER(isRenderingBackBuffer);
    }

    auto plugin::Direct3DEndScene(const bool isRenderingBackBuffer) -> void
    {
        if (!isRenderingBackBuffer)
            return;

        std::string reloadMessage;
        if (this->runtime_.PollReload(reloadMessage))
        {
            this->WriteChat(reloadMessage.c_str());
            if (!this->runtime_.HasFaulted())
                this->runtime_fault_reported_ = false;
        }

        if (!this->IsLoggedIn())
        {
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
            this->WriteChat("[XIUI Native] Runtime faulted and was quarantined; publish a new runtime generation to recover.");
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
        this->scene_camera_valid_ = false;
    }

    auto plugin::Direct3DSetRenderState(D3DRENDERSTATETYPE state, DWORD* value) -> bool
    {
        // Earlier render plugins can consume world draws before our callbacks see them.
        this->CaptureSceneCamera();
        UNREFERENCED_PARAMETER(state);
        UNREFERENCED_PARAMETER(value);
        return false;
    }

    auto plugin::Direct3DDrawPrimitive(D3DPRIMITIVETYPE primitiveType, UINT startVertex, UINT primitiveCount) -> bool
    {
        this->CaptureSceneCamera();
        UNREFERENCED_PARAMETER(primitiveType);
        UNREFERENCED_PARAMETER(startVertex);
        UNREFERENCED_PARAMETER(primitiveCount);
        return false;
    }

    auto plugin::Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT startIndex, UINT primitiveCount) -> bool
    {
        this->CaptureSceneCamera();
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
        UNREFERENCED_PARAMETER(primitiveType);
        UNREFERENCED_PARAMETER(primitiveCount);
        UNREFERENCED_PARAMETER(vertexData);
        UNREFERENCED_PARAMETER(vertexStride);
        return false;
    }

    auto plugin::Direct3DDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT minIndex, UINT vertexCount, UINT primitiveCount, const void* indexData, D3DFORMAT indexFormat, const void* vertexData, UINT vertexStride) -> bool
    {
        this->CaptureSceneCamera();
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

    auto plugin::UpdateKeyboardCapture(void) -> void
    {
        if (this->registered_keyboard_ == nullptr) return;
        if (this->runtime_.ChatFocused())
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

    auto plugin::ShouldBlockScanCode(const uint32_t scanCode, const bool focused) const -> bool
    {
        if (focused) return true;
        const auto key = this->registered_keyboard_ != nullptr ? this->registered_keyboard_->D2V(scanCode) : 256u;
        const auto generic = key == VK_LSHIFT || key == VK_RSHIFT ? VK_SHIFT : key == VK_LCONTROL || key == VK_RCONTROL ? VK_CONTROL : key == VK_LMENU || key == VK_RMENU ? VK_MENU : key;
        return key < this->consumed_keys_.size() && (this->consumed_keys_[key].load(std::memory_order_acquire) || this->consumed_keys_[generic].load(std::memory_order_acquire));
    }

    auto __stdcall plugin::KeyboardStateCallback(LPDIRECTINPUTDEVICE8A, const DWORD size, LPVOID data) -> HRESULT
    {
        auto callback = callback_gate_.Enter();
        if (!callback || data == nullptr || size != 256) return DI_OK;
        const bool focused = callback->runtime_.ChatFocused();
        auto* keys = static_cast<uint8_t*>(data);
        for (uint32_t scan = 0; scan < 256; ++scan)
            if (callback->ShouldBlockScanCode(scan, focused)) keys[scan] = 0;
        return DI_OK;
    }

    auto __stdcall plugin::KeyboardDataCallback(LPDIRECTINPUTDEVICE8A, const DWORD objectSize, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD, DWORD) -> HRESULT
    {
        auto callback = callback_gate_.Enter();
        if (!callback || data == nullptr || count == nullptr || *count > 4096 || objectSize < 8 || objectSize > sizeof(DIDEVICEOBJECTDATA)) return DI_OK;
        const bool focused = callback->runtime_.ChatFocused();
        for (DWORD index = 0; index < *count; ++index)
        {
            auto* entry = reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(data) + index * objectSize);
            if (callback->ShouldBlockScanCode(entry[0], focused)) entry[1] = 0;
        }
        return DI_OK;
    }

    auto __stdcall plugin::KeyboardCallback(const WPARAM key, const LPARAM flags, const bool blocked) -> BOOL
    {
        auto callback = callback_gate_.Enter();
        if (!callback || key >= 256) return FALSE;
        try
        {
            const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
            const bool wasConsumed = callback->consumed_keys_[key].load(std::memory_order_acquire);
            const bool consumed = callback->runtime_.HandleKey(key, flags, blocked);
            if (released) callback->consumed_keys_[key] = false;
            else if (consumed) callback->consumed_keys_[key] = true;
            callback->UpdateKeyboardCapture();
            return consumed || wasConsumed ? TRUE : FALSE;
        }
        catch (...) { callback->UpdateKeyboardCapture(); return FALSE; }
    }

    auto plugin::CaptureSceneCamera(void) -> void
    {
        if (this->scene_camera_valid_ || this->device_ == nullptr)
            return;
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
        IDirect3DSurface8* target{};
        IDirect3DSurface8* backbuffer{};
        const bool mainTarget = SUCCEEDED(this->device_->GetRenderTarget(&target)) &&
            SUCCEEDED(this->device_->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer)) && target == backbuffer;
        if (target != nullptr)
            target->Release();
        if (backbuffer != nullptr)
            backbuffer->Release();
        if (!mainTarget)
            return;
        this->scene_view_ = view;
        this->scene_projection_ = projection;
        this->scene_viewport_ = viewport;
        this->scene_camera_valid_ = true;
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
            snapshot.player_screen_valid = ProjectWorldPoint(bodyCenter, this->scene_view_, this->scene_projection_, this->scene_viewport_,
                snapshot.player_screen_x, snapshot.player_screen_y);
        }
        snapshot.native_hud_available_mask = this->native_hud_.AvailableMask();
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
