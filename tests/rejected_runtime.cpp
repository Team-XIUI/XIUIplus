#include "runtime_api.hpp"

namespace
{
    auto __stdcall get_build_id(void) -> const char* { return "rejected-test-runtime"; }
    auto __stdcall create(const xiui::runtime_create_context* context) -> void*
    {
        return context != nullptr && context->size == sizeof(xiui::runtime_create_context) ? reinterpret_cast<void*>(1) : nullptr;
    }
    auto __stdcall destroy(void*) -> void {}
    auto __stdcall set_device(void*, IDirect3DDevice8*) -> BOOL { return FALSE; }
    auto __stdcall render(void*, const xiui::hud_snapshot*) -> BOOL { return TRUE; }
    auto __stdcall capture_state(void*, xiui::runtime_state* state) -> BOOL
    {
        if (state == nullptr || state->size != sizeof(xiui::runtime_state))
            return FALSE;
        state->visible = TRUE;
        state->editing = FALSE;
        return TRUE;
    }
    auto __stdcall apply_state(void*, const xiui::runtime_state*) -> BOOL { return TRUE; }
    auto __stdcall handle_mouse(void*, uint32_t, WPARAM, LPARAM, BOOL) -> BOOL { return FALSE; }
    auto __stdcall handle_packet(void*, uint16_t, const uint8_t*, uint32_t) -> BOOL { return FALSE; }
    auto __stdcall handle_outgoing_packet(void*, uint16_t, const uint8_t*, uint32_t, BOOL) -> void {}
    auto __stdcall handle_command(void*, const char*) -> BOOL { return FALSE; }
    auto __stdcall handle_chat(void*, const xiui::chat_event*) -> void {}
    auto __stdcall handle_key(void*, WPARAM, LPARAM, BOOL) -> BOOL { return FALSE; }
    auto __stdcall chat_focused(void*) -> BOOL { return FALSE; }
    auto __stdcall keyboard_input_state(void*) -> uint32_t { return 0; }
    auto __stdcall crossbar_settings(void*, xiui::controller_crossbar_settings* settings) -> void { if (settings) *settings = {}; }
    auto __stdcall handle_controller(void*, const xiui::controller_event*) -> BOOL { return FALSE; }
    auto __stdcall controller_input_state(void*) -> uint32_t { return 0; }
    auto __stdcall update_controller_status(void*, const xiui::controller_status*) -> void {}
    auto __stdcall toggle_edit(void*) -> BOOL { return FALSE; }
    auto __stdcall exit_edit(void*) -> BOOL { return TRUE; }
    auto __stdcall set_visible(void*, BOOL) -> void {}
    auto __stdcall reload_settings(void*) -> BOOL { return TRUE; }
    auto __stdcall reset_settings(void*) -> BOOL { return TRUE; }
    auto __stdcall is_editing(void*) -> BOOL { return FALSE; }
    auto __stdcall is_visible(void*) -> BOOL { return TRUE; }
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
    result.crossbar_settings = &crossbar_settings;
    result.handle_controller = &handle_controller;
    result.controller_input_state = &controller_input_state;
    result.update_controller_status = &update_controller_status;
    *api = result;
    return TRUE;
}
