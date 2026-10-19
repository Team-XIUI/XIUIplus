#include "runtime_host.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#if defined(XIUI_SINGLE_DLL)
extern "C" auto __stdcall XiuiGetRuntimeApi(uint32_t version, xiui::runtime_api* api) -> BOOL;
#endif

namespace xiui
{
    runtime_host::runtime_host(void) = default;

    runtime_host::~runtime_host(void)
    {
        this->Shutdown();
    }

    auto runtime_host::Initialize(IAshitaCore* core, const char* installPath, const native_macro_runner macroRunner, void* macroContext) -> bool
    {
        std::lock_guard lock(this->mutex_);
        this->Shutdown();
        this->core_ = core;
        this->macro_runner_ = macroRunner; this->macro_context_ = macroContext;
        this->install_root_ = installPath != nullptr && installPath[0] != '\0' ? installPath : ".";
        char absoluteRoot[2048]{};
        const auto rootLength = ::GetFullPathNameA(this->install_root_.c_str(), sizeof(absoluteRoot), absoluteRoot, nullptr);
        if (rootLength > 0 && rootLength < sizeof(absoluteRoot)) this->install_root_ = absoluteRoot;
#if defined(XIUI_SINGLE_DLL)
        runtime_api embedded{};
        embedded.size = sizeof(embedded);
        if (XiuiGetRuntimeApi(runtime_abi_version, &embedded) == FALSE || !ValidateApi(embedded)) return false;
        const runtime_create_context context{sizeof(runtime_create_context), core, this->install_root_.c_str(), this->macro_context_, this->macro_runner_};
        this->instance_ = embedded.create(&context);
        if (this->instance_ == nullptr) return false;
        this->api_ = embedded;
        const char* build = embedded.get_build_id();
        this->runtime_build_id_ = build != nullptr ? build : "unknown";
#else
        this->published_path_ = this->install_root_ + "\\plugins\\xiuinative-runtime.dll";

        char temporaryPath[MAX_PATH]{};
        const DWORD temporaryLength = ::GetTempPathA(MAX_PATH, temporaryPath);
        const std::string temporaryRoot = temporaryLength > 0 && temporaryLength < MAX_PATH ? temporaryPath : this->install_root_ + "\\cache\\";
        char directoryName[96]{};
        std::snprintf(directoryName, sizeof(directoryName), "xiuinative-runtime-%lu", static_cast<unsigned long>(::GetCurrentProcessId()));
        this->shadow_directory_ = temporaryRoot + directoryName;
        ::CreateDirectoryA(this->shadow_directory_.c_str(), nullptr);

        file_stamp stamp{};
        if (this->ReadPublishedStamp(stamp))
        {
            std::string message;
            this->RegisterLoadResult(this->TryLoadPublished(true, stamp, message), stamp);
        }
        this->next_poll_time_ = ::GetTickCount64() + 250;
#endif
        this->RefreshInputState();
        return this->instance_ != nullptr;
    }

    auto runtime_host::Shutdown(void) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.CrossbarSettings({});
        this->controller_mailbox_.Reset();
        this->keyboard_focused_ = false;
        if (this->instance_ != nullptr && this->api_.destroy != nullptr)
            this->api_.destroy(this->instance_);
        this->instance_ = nullptr;
        this->api_ = {};

        if (this->module_ != nullptr)
            ::FreeLibrary(this->module_);
        this->module_ = nullptr;
        if (!this->shadow_path_.empty())
            ::DeleteFileA(this->shadow_path_.c_str());
        if (!this->shadow_directory_.empty())
            ::RemoveDirectoryA(this->shadow_directory_.c_str());

        this->device_ = nullptr;
        this->core_ = nullptr;
        this->macro_runner_ = nullptr; this->macro_context_ = nullptr;
        this->runtime_build_id_.clear();
        this->shadow_path_.clear();
        this->shadow_directory_.clear();
        this->published_path_.clear();
        this->install_root_.clear();
        this->loaded_stamp_ = {};
        this->rejected_stamp_ = {};
        this->transient_stamp_ = {};
        this->next_poll_time_ = 0;
        this->next_retry_time_ = 0;
        this->generation_ = 0;
        this->transient_failures_ = 0;
        this->has_loaded_stamp_ = false;
        this->has_rejected_stamp_ = false;
        this->has_transient_stamp_ = false;
        this->runtime_faulted_ = false;
        this->chat_history_.clear();
        this->next_chat_sequence_ = this->last_native_serial_ = 0;
    }

    auto runtime_host::PollReload(std::string& message) -> bool
    {
        std::lock_guard lock(this->mutex_);
        message.clear();
#if defined(XIUI_SINGLE_DLL)
        return false;
#else
        const uint64_t now = ::GetTickCount64();
        if (now < this->next_poll_time_)
            return false;
        this->next_poll_time_ = now + 250;

        file_stamp current{};
        if (!this->ReadPublishedStamp(current))
            return false;
        if (this->has_loaded_stamp_ && current == this->loaded_stamp_)
            return false;
        if (this->has_rejected_stamp_ && current == this->rejected_stamp_)
            return false;
        if (this->has_transient_stamp_ && current == this->transient_stamp_ && now < this->next_retry_time_)
            return false;

        const bool firstTransientAttempt = !this->has_transient_stamp_ || !(current == this->transient_stamp_);
        const load_result result = this->TryLoadPublished(false, current, message);
        this->RefreshInputState();
        this->RegisterLoadResult(result, current);
        if (result == load_result::transient_failure && !firstTransientAttempt)
            message.clear();
        return !message.empty();
#endif
    }

    auto runtime_host::SetDevice(IDirect3DDevice8* device) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->device_ = device;
        if (this->instance_ != nullptr && this->api_.set_device != nullptr && this->api_.set_device(this->instance_, device) == FALSE)
            this->runtime_faulted_ = true;
        this->RefreshInputState();
    }

    auto runtime_host::RenderWorld(const hud_snapshot& snapshot) -> bool
    {
        std::lock_guard lock(this->mutex_);
        return !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.render_world != nullptr &&
            this->api_.render_world(this->instance_, &snapshot) != FALSE;
    }

    auto runtime_host::Render(const hud_snapshot& snapshot) -> bool
    {
        std::lock_guard lock(this->mutex_);
        if (this->runtime_faulted_ || this->instance_ == nullptr || this->api_.render == nullptr)
        {
            this->RefreshInputState();
            return false;
        }
        this->DrainControllerInput();
        if (this->api_.render(this->instance_, &snapshot) != FALSE)
        {
            this->RefreshInputState();
            return true;
        }
        if (this->api_.exit_edit != nullptr)
            this->api_.exit_edit(this->instance_);
        this->runtime_faulted_ = true;
        this->RefreshInputState();
        return false;
    }

    auto runtime_host::HandleMouseMessage(const uint32_t message, const WPARAM wParam, const LPARAM lParam, const bool blocked) -> bool
    {
        std::lock_guard lock(this->mutex_);
        const bool consumed = !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.handle_mouse != nullptr &&
            this->api_.handle_mouse(this->instance_, message, wParam, lParam, blocked ? TRUE : FALSE) != FALSE;
        this->RefreshInputState();
        return consumed;
    }

    auto runtime_host::HandlePacket(const uint16_t id, const uint8_t* data, const uint32_t size) -> void
    {
        std::lock_guard lock(this->mutex_);
        if (id == 0x000A || id == 0x000B) this->controller_mailbox_.Reset();
        if (!this->runtime_faulted_ && this->instance_ != nullptr && this->api_.handle_packet != nullptr)
            this->api_.handle_packet(this->instance_, id, data, size);
        if (id == 0x000A || id == 0x000B) this->RefreshInputState();
    }

    auto runtime_host::HandleChat(chat_event event) -> void
    {
        std::lock_guard lock(this->mutex_);
        event.text[sizeof(event.text) - 1] = '\0';
        const auto serial = event.sequence;
        const bool duplicate = !this->chat_history_.empty() && serial == this->last_native_serial_ &&
            event.timestamp - this->chat_history_.back().timestamp < 100 && event.mode == this->chat_history_.back().mode &&
            std::strcmp(event.text, this->chat_history_.back().text) == 0 && (event.windows & this->chat_history_.back().windows) == 0;
        if (duplicate)
        {
            this->chat_history_.back().windows |= event.windows;
            event = this->chat_history_.back();
        }
        else
        {
            this->last_native_serial_ = serial;
            event.sequence = ++this->next_chat_sequence_;
            this->chat_history_.push_back(event);
            if (this->chat_history_.size() > chat_history_capacity) this->chat_history_.pop_front();
        }
        if (!this->runtime_faulted_ && this->instance_ != nullptr) this->api_.handle_chat(this->instance_, &event);
    }

    auto runtime_host::DescribeNpcCapture(void) const -> std::string
    {
        std::lock_guard lock(this->mutex_);
        const auto found = std::find_if(this->chat_history_.rbegin(), this->chat_history_.rend(), [](const chat_event& event) {
            return event.mode == 150 || event.mode == 151 || event.mode == 190 || event.mode == 142 || event.mode == 144;
        });
        if (found == this->chat_history_.rend()) return "[XIUI] NPC capture: no NPC text captured since load.";
        char header[160]{};
        std::snprintf(header, sizeof(header), "[XIUI] NPC capture: mode %u; sequence %llu; windows %u; tail bytes: ", found->mode, found->sequence, found->windows);
        std::string result{header};
        const auto length = std::strlen(found->text);
        for (size_t at = length > 32 ? length - 32 : 0; at < length; ++at)
        {
            char hex[4]{}; std::snprintf(hex, sizeof(hex), "%02X ", static_cast<uint8_t>(found->text[at])); result += hex;
        }
        return result;
    }

    auto runtime_host::HandleKey(const WPARAM key, const LPARAM flags, const bool blocked) -> bool
    {
        std::lock_guard lock(this->mutex_);
        const bool consumed = !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.handle_key(this->instance_, key, flags, blocked ? TRUE : FALSE) != FALSE;
        this->RefreshInputState();
        return consumed;
    }

    auto runtime_host::ChatFocused(void) const -> bool
    {
        return this->keyboard_focused_.load(std::memory_order_acquire);
    }

    auto runtime_host::CrossbarSettings(void) const -> controller_crossbar_settings
    { return this->controller_mailbox_.CrossbarSettings(); }

    auto runtime_host::HandleController(const controller_event& event, const bool nativeMenuOpen) -> bool
    {
        return this->controller_mailbox_.Submit(event, nativeMenuOpen);
    }

    auto runtime_host::ControllerCapture(const bool nativeMenuOpen) const -> controller_capture_mode
    {
        return static_cast<controller_capture_mode>(this->controller_mailbox_.State(nativeMenuOpen) & controller_capture_mask);
    }

    auto runtime_host::SuspendControllerInput(void) -> void
    {
        this->controller_mailbox_.Reset();
    }

    auto runtime_host::UpdateControllerStatus(const controller_status& status) -> void
    {
        this->controller_mailbox_.Status(status);
    }

    auto runtime_host::RefreshInputState(void) -> void
    {
        if (this->runtime_faulted_ || this->instance_ == nullptr)
        {
            this->controller_mailbox_.CrossbarSettings({});
            this->controller_mailbox_.Reset();
            this->keyboard_focused_ = false;
            return;
        }
        this->keyboard_focused_.store(this->api_.chat_focused(this->instance_) != FALSE, std::memory_order_release);
        controller_crossbar_settings crossbar{};
        if (this->api_.crossbar_settings != nullptr) this->api_.crossbar_settings(this->instance_, &crossbar);
        this->controller_mailbox_.CrossbarSettings(crossbar);
        this->controller_mailbox_.Publish(this->api_.controller_input_state(this->instance_));
    }

    auto runtime_host::DrainControllerInput(void) -> void
    {
        const auto pending = this->controller_mailbox_.Take();
        if (!this->controller_mailbox_.Current(pending.generation)) return;
        if (pending.has_status) this->api_.update_controller_status(this->instance_, &pending.status);
        const auto revision = this->api_.controller_input_state(this->instance_) & controller_revision_mask;
        const auto now = ::GetTickCount64();
        for (size_t index = 0; index < pending.count; ++index)
        {
            if (!this->controller_mailbox_.Current(pending.generation)) break;
            const auto& event = pending.events[index];
            if (now - event.timestamp > 500) continue;
            this->api_.handle_controller(this->instance_, &event);
            if ((this->api_.controller_input_state(this->instance_) & controller_revision_mask) != revision)
            {
                this->controller_mailbox_.Reset();
                break;
            }
        }
    }

    auto runtime_host::HandleCommand(const char* command) -> bool
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.Reset();
        const bool handled = !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.handle_command != nullptr &&
            this->api_.handle_command(this->instance_, command) != FALSE;
        if (handled && command != nullptr && ::_stricmp(command, "chat clear") == 0) this->chat_history_.clear();
        this->RefreshInputState();
        return handled;
    }

    auto runtime_host::ToggleEdit(void) -> bool
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.Reset();
        const bool toggled = !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.toggle_edit != nullptr && this->api_.toggle_edit(this->instance_) != FALSE;
        this->RefreshInputState();
        return toggled;
    }

    auto runtime_host::ExitEdit(void) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.Reset();
        if (this->instance_ != nullptr && this->api_.exit_edit != nullptr)
            this->api_.exit_edit(this->instance_);
        this->RefreshInputState();
    }

    auto runtime_host::SetVisible(const bool visible) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.Reset();
        if (this->instance_ != nullptr && this->api_.set_visible != nullptr)
            this->api_.set_visible(this->instance_, visible ? TRUE : FALSE);
        this->RefreshInputState();
    }

    auto runtime_host::ReloadSettings(void) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.Reset();
        if (this->instance_ != nullptr && this->api_.reload_settings != nullptr && this->api_.reload_settings(this->instance_) == FALSE)
            this->runtime_faulted_ = true;
        this->RefreshInputState();
    }

    auto runtime_host::ResetSettings(void) -> void
    {
        std::lock_guard lock(this->mutex_);
        this->controller_mailbox_.Reset();
        if (this->instance_ != nullptr && this->api_.reset_settings != nullptr && this->api_.reset_settings(this->instance_) == FALSE)
            this->runtime_faulted_ = true;
        this->RefreshInputState();
    }

    auto runtime_host::IsLoaded(void) const -> bool
    {
        std::lock_guard lock(this->mutex_);
        return this->instance_ != nullptr;
    }

    auto runtime_host::IsEditing(void) const -> bool
    {
        std::lock_guard lock(this->mutex_);
        return !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.is_editing != nullptr && this->api_.is_editing(this->instance_) != FALSE;
    }

    auto runtime_host::IsVisible(void) const -> bool
    {
        std::lock_guard lock(this->mutex_);
        return !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.is_visible != nullptr && this->api_.is_visible(this->instance_) != FALSE;
    }

    auto runtime_host::IsGpuShaderActive(void) const -> bool
    {
        std::lock_guard lock(this->mutex_);
        return !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.is_gpu_shader_active != nullptr && this->api_.is_gpu_shader_active(this->instance_) != FALSE;
    }

    auto runtime_host::NativeHudHiddenMask(void) const -> uint32_t
    {
        std::lock_guard lock(this->mutex_);
        return !this->runtime_faulted_ && this->instance_ != nullptr && this->api_.native_hud_hidden_mask != nullptr
            ? this->api_.native_hud_hidden_mask(this->instance_) : 0;
    }

    auto runtime_host::MenuLayout(void) const -> native_menu_layout
    {
        std::lock_guard lock(this->mutex_);
        native_menu_layout layout{};
        if (!this->runtime_faulted_ && this->instance_ != nullptr && this->api_.menu_layout != nullptr)
            this->api_.menu_layout(this->instance_, &layout);
        return layout;
    }

    auto runtime_host::HasFaulted(void) const -> bool
    {
        std::lock_guard lock(this->mutex_);
        return this->runtime_faulted_;
    }

    auto runtime_host::GetBuildId(void) const -> std::string
    {
        std::lock_guard lock(this->mutex_);
        return this->runtime_build_id_;
    }

    auto runtime_host::GetPublishedPath(void) const -> std::string
    {
        std::lock_guard lock(this->mutex_);
        return this->published_path_;
    }

#if !defined(XIUI_SINGLE_DLL)
    auto runtime_host::TryLoadPublished(const bool initial, const file_stamp& stamp, std::string& message) -> load_result
    {
        const std::string candidatePath = this->MakeShadowPath();
        if (::CopyFileA(this->published_path_.c_str(), candidatePath.c_str(), FALSE) == FALSE)
        {
            message = LastErrorText("Could not shadow-copy the runtime");
            ::DeleteFileA(candidatePath.c_str());
            return load_result::transient_failure;
        }

        HMODULE candidateModule = ::LoadLibraryA(candidatePath.c_str());
        if (candidateModule == nullptr)
        {
            message = LastErrorText("Could not load the runtime");
            ::DeleteFileA(candidatePath.c_str());
            return load_result::transient_failure;
        }

        const auto rejectCandidate = [&candidateModule, &candidatePath]() {
            ::FreeLibrary(candidateModule);
            ::DeleteFileA(candidatePath.c_str());
        };
        const auto getApi = reinterpret_cast<get_runtime_api_fn>(::GetProcAddress(candidateModule, "XiuiGetRuntimeApi"));
        runtime_api candidateApi{};
        candidateApi.size = sizeof(candidateApi);
        if (getApi == nullptr || getApi(runtime_abi_version, &candidateApi) == FALSE || !ValidateApi(candidateApi))
        {
            message = "[XIUI] Runtime ABI validation failed; previous runtime retained.";
            rejectCandidate();
            return load_result::rejected;
        }

        runtime_state previousState{sizeof(runtime_state), TRUE, FALSE};
        if (this->instance_ != nullptr && this->api_.capture_state(this->instance_, &previousState) == FALSE)
        {
            message = "[XIUI] Could not capture runtime state; previous runtime retained.";
            rejectCandidate();
            return load_result::rejected;
        }

        const runtime_create_context context{sizeof(runtime_create_context), this->core_, this->install_root_.c_str(), this->macro_context_, this->macro_runner_};
        void* candidateInstance = candidateApi.create(&context);
        if (candidateInstance == nullptr)
        {
            message = "[XIUI] Runtime initialization failed; previous runtime retained.";
            rejectCandidate();
            return load_result::rejected;
        }
        const auto destroyCandidate = [&candidateApi, &candidateInstance, &candidateModule, &candidatePath]() {
            candidateApi.destroy(candidateInstance);
            ::FreeLibrary(candidateModule);
            ::DeleteFileA(candidatePath.c_str());
        };

        if (candidateApi.set_device(candidateInstance, this->device_) == FALSE ||
            (this->instance_ != nullptr && candidateApi.reload_settings(candidateInstance) == FALSE))
        {
            message = "[XIUI] Runtime candidate validation failed; previous runtime retained.";
            destroyCandidate();
            return load_result::rejected;
        }

        runtime_state inactiveState = previousState;
        inactiveState.editing = FALSE;
        inactiveState.chat_focused = FALSE;
        if (candidateApi.apply_state(candidateInstance, &inactiveState) == FALSE)
        {
            message = "[XIUI] Runtime candidate state validation failed; previous runtime retained.";
            destroyCandidate();
            return load_result::rejected;
        }

        bool previousInputReleased = false;
        if ((previousState.editing != FALSE || previousState.chat_focused != FALSE) && this->instance_ != nullptr)
        {
            const bool inputReleased = this->api_.exit_edit(this->instance_) != FALSE &&
                this->api_.is_editing(this->instance_) == FALSE;
            if (!inputReleased)
            {
                const bool previousStateRestored = this->api_.apply_state(this->instance_, &previousState) != FALSE;
                message = previousStateRestored
                    ? "[XIUI] Previous runtime could not exit edit mode; candidate rejected."
                    : "[XIUI] Previous runtime edit-state recovery failed; candidate rejected.";
                destroyCandidate();
                return load_result::rejected;
            }
            previousInputReleased = true;
        }
        for (const auto& event : this->chat_history_) candidateApi.handle_chat(candidateInstance, &event);
        if (candidateApi.apply_state(candidateInstance, &previousState) == FALSE)
        {
            bool previousStateRestored = true;
            if (previousInputReleased)
                previousStateRestored = this->api_.apply_state(this->instance_, &previousState) != FALSE;
            message = previousStateRestored
                ? "[XIUI] Runtime activation failed; previous runtime retained."
                : "[XIUI] Runtime activation failed and edit mode could not be restored.";
            destroyCandidate();
            return load_result::rejected;
        }

        HMODULE previousModule = this->module_;
        runtime_api previousApi = this->api_;
        void* previousInstance = this->instance_;
        std::string previousShadowPath = std::move(this->shadow_path_);

        this->module_ = candidateModule;
        this->api_ = candidateApi;
        this->instance_ = candidateInstance;
        this->shadow_path_ = candidatePath;
        const char* buildId = candidateApi.get_build_id();
        this->runtime_build_id_ = buildId != nullptr ? buildId : "unknown";
        this->runtime_faulted_ = false;

        this->controller_mailbox_.Reset();
        if (previousInstance != nullptr)
            previousApi.destroy(previousInstance);
        if (previousModule != nullptr)
            ::FreeLibrary(previousModule);
        if (!previousShadowPath.empty())
            ::DeleteFileA(previousShadowPath.c_str());

        if (initial)
            message = "[XIUI] Runtime loaded.";
        else
            message = "[XIUI] Runtime hot-reloaded: " + this->runtime_build_id_ + ".";
        UNREFERENCED_PARAMETER(stamp);
        return load_result::loaded;
    }

    auto runtime_host::ReadPublishedStamp(file_stamp& stamp) const -> bool
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (this->published_path_.empty() || ::GetFileAttributesExA(this->published_path_.c_str(), GetFileExInfoStandard, &attributes) == FALSE)
            return false;
        ULARGE_INTEGER writeTime{};
        writeTime.HighPart = attributes.ftLastWriteTime.dwHighDateTime;
        writeTime.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
        ULARGE_INTEGER size{};
        size.HighPart = attributes.nFileSizeHigh;
        size.LowPart = attributes.nFileSizeLow;
        stamp.write_time = writeTime.QuadPart;
        stamp.size = size.QuadPart;
        return true;
    }

    auto runtime_host::MakeShadowPath(void) -> std::string
    {
        char name[96]{};
        std::snprintf(
            name,
            sizeof(name),
            "\\runtime-%llu-%u.dll",
            static_cast<unsigned long long>(::GetTickCount64()),
            ++this->generation_);
        return this->shadow_directory_ + name;
    }

#endif

    auto runtime_host::ValidateApi(const runtime_api& api) -> bool
    {
        return api.size == sizeof(runtime_api) &&
            api.abi_version == runtime_abi_version &&
            api.get_build_id != nullptr &&
            api.create != nullptr &&
            api.destroy != nullptr &&
            api.set_device != nullptr &&
            api.render != nullptr &&
            api.capture_state != nullptr &&
            api.apply_state != nullptr &&
            api.handle_mouse != nullptr &&
            api.handle_packet != nullptr &&
            api.handle_command != nullptr &&
            api.toggle_edit != nullptr &&
            api.exit_edit != nullptr &&
            api.set_visible != nullptr &&
            api.reload_settings != nullptr &&
            api.reset_settings != nullptr &&
            api.is_editing != nullptr &&
            api.is_visible != nullptr &&
            api.is_gpu_shader_active != nullptr &&
            api.native_hud_hidden_mask != nullptr && api.menu_layout != nullptr &&
            api.handle_chat != nullptr && api.handle_key != nullptr && api.chat_focused != nullptr &&
            api.handle_controller != nullptr && api.controller_input_state != nullptr && api.update_controller_status != nullptr &&
            api.crossbar_settings != nullptr;
    }

#if !defined(XIUI_SINGLE_DLL)
    auto runtime_host::RegisterLoadResult(const load_result result, const file_stamp& stamp) -> void
    {
        if (result == load_result::loaded)
        {
            this->loaded_stamp_ = stamp;
            this->has_loaded_stamp_ = true;
            this->has_rejected_stamp_ = false;
            this->has_transient_stamp_ = false;
            this->transient_failures_ = 0;
            this->next_retry_time_ = 0;
            return;
        }
        if (result == load_result::rejected)
        {
            this->rejected_stamp_ = stamp;
            this->has_rejected_stamp_ = true;
            this->has_transient_stamp_ = false;
            return;
        }

        if (!this->has_transient_stamp_ || !(this->transient_stamp_ == stamp))
            this->transient_failures_ = 0;
        this->transient_stamp_ = stamp;
        this->has_transient_stamp_ = true;
        ++this->transient_failures_;
        const uint32_t exponent = std::min(this->transient_failures_, 4u);
        this->next_retry_time_ = ::GetTickCount64() + (250u << exponent);
    }

    auto runtime_host::LastErrorText(const char* action) -> std::string
    {
        char message[256]{};
        std::snprintf(message, sizeof(message), "[XIUI] %s (Windows error %lu); previous runtime retained.", action, static_cast<unsigned long>(::GetLastError()));
        return message;
    }
#endif
} // namespace xiui
