#include "runtime_api.hpp"

#include <new>
#include <cstring>
#include <cstdio>

#ifndef XIUI_SYNTHETIC_MODE
#define XIUI_SYNTHETIC_MODE 0
#endif

namespace
{
    struct synthetic_state
    {
        BOOL visible{TRUE};
        BOOL editing{FALSE};
        BOOL chat_focused{FALSE};
        uint32_t chat_count{};
        uint64_t chat_sequence{};
        char chat_draft[xiui::chat_draft_capacity]{};
    };

    auto __stdcall get_build_id(void) -> const char*
    {
#if XIUI_SYNTHETIC_MODE == 1
        return "activation-failure-runtime";
#elif XIUI_SYNTHETIC_MODE == 2
        return "render-failure-runtime";
#elif XIUI_SYNTHETIC_MODE == 3
        return "exit-failure-runtime";
#else
        return "stateful-test-runtime";
#endif
    }

    auto as_state(void* instance) -> synthetic_state*
    {
        return static_cast<synthetic_state*>(instance);
    }

    auto __stdcall create(const xiui::runtime_create_context* context) -> void*
    {
        return context != nullptr && context->size == sizeof(xiui::runtime_create_context) &&
            (context->ashita_interface == 4.16 || context->ashita_interface == 4.30)
            ? new (std::nothrow) synthetic_state{}
            : nullptr;
    }
    auto __stdcall destroy(void* instance) -> void { delete as_state(instance); }
    auto __stdcall set_device(void*, IDirect3DDevice8*) -> BOOL { return TRUE; }
    auto __stdcall render(void*, const xiui::hud_snapshot*) -> BOOL
    {
#if XIUI_SYNTHETIC_MODE == 2
        return FALSE;
#else
        return TRUE;
#endif
    }
    auto __stdcall capture_state(void* instance, xiui::runtime_state* state) -> BOOL
    {
        if (instance == nullptr || state == nullptr || state->size != sizeof(xiui::runtime_state))
            return FALSE;
        state->visible = as_state(instance)->visible;
        state->editing = as_state(instance)->editing;
        state->chat_focused = as_state(instance)->chat_focused;
        strcpy_s(state->chat_draft, as_state(instance)->chat_draft);
        return TRUE;
    }
    auto __stdcall apply_state(void* instance, const xiui::runtime_state* state) -> BOOL
    {
        if (instance == nullptr || state == nullptr || state->size != sizeof(xiui::runtime_state))
            return FALSE;
#if XIUI_SYNTHETIC_MODE == 1
        if (state->editing != FALSE)
            return FALSE;
#endif
        as_state(instance)->visible = state->visible;
        as_state(instance)->editing = state->editing;
        as_state(instance)->chat_focused = state->chat_focused;
        strcpy_s(as_state(instance)->chat_draft, state->chat_draft);
        return TRUE;
    }
    auto __stdcall handle_mouse(void*, uint32_t, WPARAM, LPARAM, BOOL) -> BOOL { return FALSE; }
    auto __stdcall handle_packet(void*, uint16_t, const uint8_t*, uint32_t) -> BOOL { return FALSE; }
    auto __stdcall handle_outgoing_packet(void*, uint16_t, const uint8_t*, uint32_t, BOOL) -> void {}
    auto __stdcall handle_chat(void* instance, const xiui::chat_event* event) -> void
    {
        if (event->sequence != as_state(instance)->chat_sequence) ++as_state(instance)->chat_count;
        as_state(instance)->chat_sequence = event->sequence;
    }
    auto __stdcall handle_key(void* instance, WPARAM key, LPARAM, BOOL) -> BOOL
    {
        if (key == VK_RETURN) { as_state(instance)->chat_focused = TRUE; strcpy_s(as_state(instance)->chat_draft, "draft"); return TRUE; }
        return FALSE;
    }
    auto __stdcall chat_focused(void* instance) -> BOOL { return as_state(instance)->chat_focused; }
    auto __stdcall keyboard_input_state(void* instance) -> uint32_t
    { return as_state(instance)->chat_focused ? xiui::keyboard_capture_all : 0; }
    auto __stdcall handle_controller(void*, const xiui::controller_event*) -> BOOL { return FALSE; }
    auto __stdcall controller_input_state(void*) -> uint32_t { return 0; }
    auto __stdcall update_controller_status(void*, const xiui::controller_status*) -> void {}
    auto __stdcall crossbar_settings(void*, xiui::controller_crossbar_settings* settings) -> void { if (settings) *settings = {}; }
    auto __stdcall handle_command(void* instance, const char* command) -> BOOL
    {
        char count[40]{}; sprintf_s(count, "chatcount %u", as_state(instance)->chat_count);
        return strcmp(command, count) == 0 || (strcmp(command, "draft preserved") == 0 && strcmp(as_state(instance)->chat_draft, "draft") == 0);
    }
    auto __stdcall toggle_edit(void* instance) -> BOOL
    {
        if (instance == nullptr)
            return FALSE;
        as_state(instance)->editing = as_state(instance)->editing == FALSE ? TRUE : FALSE;
        return TRUE;
    }
    auto __stdcall exit_edit(void* instance) -> BOOL
    {
        if (instance == nullptr)
            return FALSE;
#if XIUI_SYNTHETIC_MODE == 3
        return FALSE;
#else
        as_state(instance)->editing = FALSE;
        as_state(instance)->chat_focused = FALSE;
        return TRUE;
#endif
    }
    auto __stdcall set_visible(void* instance, const BOOL visible) -> void
    {
        if (instance != nullptr)
            as_state(instance)->visible = visible;
    }
    auto __stdcall reload_settings(void*) -> BOOL { return TRUE; }
    auto __stdcall reset_settings(void*) -> BOOL { return TRUE; }
    auto __stdcall is_editing(void* instance) -> BOOL { return instance != nullptr ? as_state(instance)->editing : FALSE; }
    auto __stdcall is_visible(void* instance) -> BOOL { return instance != nullptr ? as_state(instance)->visible : FALSE; }
    auto __stdcall native_hud_hidden_mask(void*) -> uint32_t { return 0; }
    auto __stdcall menu_layout(void*, xiui::native_menu_layout* layout) -> void { if (layout) *layout = {}; }
    auto __stdcall is_gpu_shader_active(void*) -> BOOL { return FALSE; }
}

extern "C" __declspec(dllexport) auto __stdcall XiuiGetRuntimeApi(const uint32_t hostAbiVersion, xiui::runtime_api* api) -> BOOL
{
    if (hostAbiVersion != xiui::runtime_abi_version || api == nullptr || api->size != sizeof(xiui::runtime_api))
        return FALSE;
    xiui::runtime_api result{};
    result.size = sizeof(result);
    result.abi_version = xiui::runtime_abi_version;
    result.get_build_id = &get_build_id;
    result.create = &create;
    result.destroy = &destroy;
    result.set_device = &set_device;
    result.render = &render;
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
    result.handle_controller = &handle_controller;
    result.controller_input_state = &controller_input_state;
    result.update_controller_status = &update_controller_status;
    result.crossbar_settings = &crossbar_settings;
    *api = result;
    return TRUE;
}
