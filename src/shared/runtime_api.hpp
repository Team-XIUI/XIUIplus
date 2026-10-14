#pragma once

#include "Ashita.h"
#include "chat_data.hpp"
#include "native_menu_data.hpp"

#include <cstdint>

namespace xiui
{
    inline constexpr uint32_t runtime_abi_version = 8;

    struct hud_snapshot
    {
        char player_name[64]{};
        char main_job[24]{};
        char sub_job[24]{};
        uint32_t hp{};
        uint32_t hp_max{1};
        uint32_t mp{};
        uint32_t mp_max{1};
        uint32_t tp{};

        bool target_valid{};
        char target_name[128]{};
        uint32_t target_hp_percent{};
        float target_distance{};

        bool player_screen_valid{};
        float player_screen_x{};
        float player_screen_y{};

        uint32_t exp_current{};
        uint32_t exp_needed{1};
        uint32_t native_hud_available_mask{};
        bool chat_capture_available{};
        chat_palette chat_colors{};
        native_ui_state native_ui{};
    };

    struct runtime_create_context
    {
        uint32_t size{};
        IAshitaCore* core{};
        const char* install_path{};
    };

    struct runtime_state
    {
        uint32_t size{};
        BOOL visible{TRUE};
        BOOL editing{FALSE};
        BOOL chat_focused{FALSE};
        uint32_t chat_caret{};
        char chat_draft[chat_draft_capacity]{};
    };

    struct runtime_api
    {
        uint32_t size{};
        uint32_t abi_version{};
        const char* (__stdcall* get_build_id)(void){};
        void* (__stdcall* create)(const runtime_create_context* context){};
        void (__stdcall* destroy)(void* instance){};
        BOOL (__stdcall* set_device)(void* instance, IDirect3DDevice8* device){};
        BOOL (__stdcall* render)(void* instance, const hud_snapshot* snapshot){};
        BOOL (__stdcall* capture_state)(void* instance, runtime_state* state){};
        BOOL (__stdcall* apply_state)(void* instance, const runtime_state* state){};
        BOOL (__stdcall* handle_mouse)(void* instance, uint32_t message, WPARAM wParam, LPARAM lParam, BOOL blocked){};
        void (__stdcall* handle_packet)(void* instance, uint16_t id, const uint8_t* data, uint32_t size){};
        BOOL (__stdcall* handle_command)(void* instance, const char* command){};
        BOOL (__stdcall* toggle_edit)(void* instance){};
        BOOL (__stdcall* exit_edit)(void* instance){};
        void (__stdcall* set_visible)(void* instance, BOOL visible){};
        BOOL (__stdcall* reload_settings)(void* instance){};
        BOOL (__stdcall* reset_settings)(void* instance){};
        BOOL (__stdcall* is_editing)(void* instance){};
        BOOL (__stdcall* is_visible)(void* instance){};
        BOOL (__stdcall* is_gpu_shader_active)(void* instance){};
        uint32_t (__stdcall* native_hud_hidden_mask)(void* instance){};
        void (__stdcall* handle_chat)(void* instance, const chat_event* event){};
        BOOL (__stdcall* handle_key)(void* instance, WPARAM key, LPARAM flags, BOOL blocked){};
        BOOL (__stdcall* chat_focused)(void* instance){};
        void (__stdcall* menu_layout)(void* instance, native_menu_layout* layout){};
    };

    using get_runtime_api_fn = BOOL(__stdcall*)(uint32_t hostAbiVersion, runtime_api* api);
} // namespace xiui
