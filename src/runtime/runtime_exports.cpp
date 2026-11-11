#include "runtime_api.hpp"
#include "hud_runtime.hpp"

#include <memory>
#include <new>

namespace
{
    constexpr char runtime_build_id[] = __DATE__ " " __TIME__;

    auto as_runtime(void* instance) -> xiui::hud_runtime*
    {
        return static_cast<xiui::hud_runtime*>(instance);
    }

    template<typename Action>
    auto guard_void(void* instance, Action&& action) noexcept -> void
    {
        try
        {
            if (auto* runtime = as_runtime(instance); runtime != nullptr)
            {
                const xiui::i18n::scope locale{runtime->Language()};
                action(*runtime);
            }
        }
        catch (...)
        {
        }
    }

    template<typename Action>
    auto guard_bool(void* instance, Action&& action) noexcept -> BOOL
    {
        try
        {
            if (auto* runtime = as_runtime(instance); runtime != nullptr)
            {
                const xiui::i18n::scope locale{runtime->Language()};
                return action(*runtime) ? TRUE : FALSE;
            }
        }
        catch (...)
        {
        }
        return FALSE;
    }

    auto __stdcall get_build_id(void) -> const char*
    {
        return runtime_build_id;
    }

    auto __stdcall create_runtime(const xiui::runtime_create_context* context) -> void*
    {
        try
        {
            if (context == nullptr || context->size != sizeof(xiui::runtime_create_context))
                return nullptr;
            auto runtime = std::unique_ptr<xiui::hud_runtime>{new (std::nothrow) xiui::hud_runtime{}};
            if (runtime == nullptr)
                return nullptr;
            runtime->SetNativeMacroRunner(context->run_native_macro, context->native_macro_context);
            runtime->Initialize(context->core, context->install_path, context->ashita_interface);
            return runtime.release();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    auto __stdcall destroy_runtime(void* instance) -> void
    {
        try
        {
            delete as_runtime(instance);
        }
        catch (...)
        {
        }
    }

    auto __stdcall set_device(void* instance, IDirect3DDevice8* device) -> BOOL
    {
        return guard_bool(instance, [device](xiui::hud_runtime& runtime) { return runtime.SetDevice(device); });
    }

    auto __stdcall render_world(void* instance, const xiui::hud_snapshot* snapshot) -> BOOL
    {
        if (!snapshot) return FALSE;
        return guard_bool(instance,[snapshot](xiui::hud_runtime& runtime) { runtime.RenderWorld(*snapshot); return true; });
    }

    auto __stdcall render(void* instance, const xiui::hud_snapshot* snapshot) -> BOOL
    {
        if (snapshot == nullptr)
            return FALSE;
        return guard_bool(instance, [snapshot](xiui::hud_runtime& runtime) {
            runtime.Render(*snapshot);
            return true;
        });
    }

    auto __stdcall capture_state(void* instance, xiui::runtime_state* state) -> BOOL
    {
        if (state == nullptr || state->size != sizeof(xiui::runtime_state))
            return FALSE;
        return guard_bool(instance, [state](xiui::hud_runtime& runtime) {
            runtime.FlushSettings();
            state->visible = runtime.IsVisible() ? TRUE : FALSE;
            state->editing = runtime.IsEditing() ? TRUE : FALSE;
            runtime.CaptureChat(*state);
            return true;
        });
    }

    auto __stdcall apply_state(void* instance, const xiui::runtime_state* state) -> BOOL
    {
        if (state == nullptr || state->size != sizeof(xiui::runtime_state))
            return FALSE;
        return guard_bool(instance, [state](xiui::hud_runtime& runtime) {
            runtime.SetVisible(state->visible != FALSE);
            if (!runtime.RestoreEditing(state->editing != FALSE)) return false;
            runtime.RestoreChat(*state);
            return runtime.IsVisible() == (state->visible != FALSE) && runtime.IsEditing() == (state->editing != FALSE);
        });
    }

    auto __stdcall handle_mouse(void* instance, const uint32_t message, const WPARAM wParam, const LPARAM lParam, const BOOL blocked) -> BOOL
    {
        return guard_bool(instance, [=](xiui::hud_runtime& runtime) {
            return runtime.HandleMouseMessage(message, wParam, lParam, blocked != FALSE);
        });
    }

    auto __stdcall handle_packet(void* instance, const uint16_t id, const uint8_t* data, const uint32_t size) -> BOOL
    {
        return guard_bool(instance, [=](xiui::hud_runtime& runtime) { return runtime.HandlePacket(id, data, size); });
    }

    auto __stdcall handle_outgoing_packet(void* instance, const uint16_t id, const uint8_t* data, const uint32_t size, const BOOL injected) -> void
    {
        guard_void(instance, [=](xiui::hud_runtime& runtime) { runtime.HandleOutgoingPacket(id, data, size, injected != FALSE); });
    }

    auto __stdcall handle_chat(void* instance, const xiui::chat_event* event) -> void
    {
        if (event != nullptr) guard_void(instance, [event](xiui::hud_runtime& runtime) { runtime.HandleChat(*event); });
    }

    auto __stdcall handle_key(void* instance, const WPARAM key, const LPARAM flags, const BOOL blocked) -> BOOL
    {
        return guard_bool(instance, [=](xiui::hud_runtime& runtime) { return runtime.HandleKey(key, flags, blocked != FALSE); });
    }

    auto __stdcall chat_focused(void* instance) -> BOOL
    {
        // Keep the existing host ABI while reporting every XIUI keyboard input owner.
        return guard_bool(instance, [](xiui::hud_runtime& runtime) { return runtime.KeyboardFocused(); });
    }

    auto __stdcall keyboard_input_state(void* instance) -> uint32_t
    {
        uint32_t state{};
        guard_void(instance, [&](xiui::hud_runtime& runtime) { state = runtime.KeyboardInputState(); });
        return state;
    }

    auto __stdcall handle_controller(void* instance, const xiui::controller_event* event) -> BOOL
    {
        if (event == nullptr) return FALSE;
        return guard_bool(instance, [event](xiui::hud_runtime& runtime) { return runtime.HandleController(*event); });
    }

    auto __stdcall controller_input_state(void* instance) -> uint32_t
    {
        uint32_t state{};
        guard_void(instance, [&](xiui::hud_runtime& runtime) { state = runtime.ControllerInputState(); });
        return state;
    }

    auto __stdcall update_controller_status(void* instance, const xiui::controller_status* status) -> void
    {
        if (status != nullptr) guard_void(instance, [status](xiui::hud_runtime& runtime) { runtime.UpdateControllerStatus(*status); });
    }

    auto __stdcall crossbar_settings(void* instance, xiui::controller_crossbar_settings* settings) -> void
    {
        if (settings == nullptr) return;
        *settings = {};
        guard_void(instance, [&](xiui::hud_runtime& runtime) { *settings = runtime.CrossbarSettings(); });
    }

    auto __stdcall handle_command(void* instance, const char* command) -> BOOL
    {
        return guard_bool(instance, [=](xiui::hud_runtime& runtime) { return runtime.HandleCommand(command); });
    }

    auto __stdcall toggle_edit(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) { return runtime.ToggleEdit(); });
    }

    auto __stdcall exit_edit(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) {
            runtime.ExitEdit();
            return !runtime.IsEditing();
        });
    }

    auto __stdcall set_visible(void* instance, const BOOL visible) -> void
    {
        guard_void(instance, [visible](xiui::hud_runtime& runtime) { runtime.SetVisible(visible != FALSE); });
    }

    auto __stdcall reload_settings(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) {
            runtime.ReloadSettings();
            return true;
        });
    }

    auto __stdcall reset_settings(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) {
            runtime.ResetSettings();
            return true;
        });
    }

    auto __stdcall is_editing(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) { return runtime.IsEditing(); });
    }

    auto __stdcall is_visible(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) { return runtime.IsVisible(); });
    }

    auto __stdcall native_hud_hidden_mask(void* instance) -> uint32_t
    {
        try
        {
            auto* runtime = as_runtime(instance);
            return runtime != nullptr ? runtime->NativeHudHiddenMask() : 0;
        }
        catch (...) { return 0; }
    }

    auto __stdcall menu_layout(void* instance, xiui::native_menu_layout* layout) -> void
    {
        if (layout != nullptr) guard_void(instance, [layout](xiui::hud_runtime& runtime) { *layout = runtime.MenuLayout(); });
    }

    auto __stdcall is_gpu_shader_active(void* instance) -> BOOL
    {
        return guard_bool(instance, [](xiui::hud_runtime& runtime) { return runtime.IsGpuShaderActive(); });
    }
}

extern "C" __declspec(dllexport) auto __stdcall XiuiGetRuntimeApi(const uint32_t hostAbiVersion, xiui::runtime_api* api) -> BOOL
{
    if (hostAbiVersion != xiui::runtime_abi_version || api == nullptr || api->size != sizeof(xiui::runtime_api))
        return FALSE;

    xiui::runtime_api result{};
    result.size = sizeof(result);
    result.abi_version = xiui::runtime_abi_version;
    result.get_build_id = &get_build_id;
    result.handle_controller = &handle_controller;
    result.controller_input_state = &controller_input_state;
    result.update_controller_status = &update_controller_status;
    result.crossbar_settings = &crossbar_settings;
    result.create = &create_runtime;
    result.destroy = &destroy_runtime;
    result.set_device = &set_device;
    result.render = &render;
    result.render_world = &render_world;
    result.capture_state = &capture_state;
    result.apply_state = &apply_state;
    result.handle_mouse = &handle_mouse;
    result.handle_packet = &handle_packet;
    result.handle_outgoing_packet = &handle_outgoing_packet;
    result.handle_command = &handle_command;
    result.toggle_edit = &toggle_edit;
    result.exit_edit = &exit_edit;
    result.set_visible = &set_visible;
    result.reload_settings = &reload_settings;
    result.reset_settings = &reset_settings;
    result.is_editing = &is_editing;
    result.is_visible = &is_visible;
    result.is_gpu_shader_active = &is_gpu_shader_active;
    result.native_hud_hidden_mask = &native_hud_hidden_mask;
    result.menu_layout = &menu_layout;
    result.handle_chat = &handle_chat;
    result.handle_key = &handle_key;
    result.chat_focused = &chat_focused;
    result.keyboard_input_state = &keyboard_input_state;
    *api = result;
    return TRUE;
}
