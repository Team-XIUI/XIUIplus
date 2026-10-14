cmake_minimum_required(VERSION 3.22)

set(ASHITA_PROJECT_NAME "xiuinative")
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" XIUI_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
project(${ASHITA_PROJECT_NAME} VERSION ${XIUI_VERSION} LANGUAGES C CXX)

if (MSVC)
    string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if (NOT WIN32)
    message(FATAL_ERROR "Ashita plugins are Windows DLLs. Configure this project with a Windows toolchain.")
endif()

if (CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Ashita v4 plugins must be built as 32-bit/x86 DLLs. Reconfigure with a Win32 toolchain.")
endif()

set(ASHITA4_SDK_PATH "$ENV{ASHITA4_SDK_PATH}" CACHE PATH "Path to Ashita v4 plugins/sdk directory")
if (NOT ASHITA4_SDK_PATH)
    message(FATAL_ERROR "Set ASHITA4_SDK_PATH to your Ashita v4 plugins/sdk directory.")
endif()
file(TO_CMAKE_PATH "${ASHITA4_SDK_PATH}" ASHITA4_SDK_PATH)

if (NOT EXISTS "${ASHITA4_SDK_PATH}/Ashita.h")
    message(FATAL_ERROR "ASHITA4_SDK_PATH must point at the SDK directory that contains Ashita.h. Current: ${ASHITA4_SDK_PATH}")
endif()

set(ASHITA_D3D8_LIB_DIR "${ASHITA4_SDK_PATH}/d3d8/lib")
if (NOT EXISTS "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
    message(FATAL_ERROR "The Ashita Direct3D8 import library was not found: ${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
endif()

function(configure_xiui_target target)
    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/src/host"
        "${CMAKE_SOURCE_DIR}/src/runtime"
        "${CMAKE_SOURCE_DIR}/src/shared"
        "${CMAKE_BINARY_DIR}/generated"
        "${ASHITA4_SDK_PATH}"
        "${ASHITA4_SDK_PATH}/d3d8/includes"
    )
    target_compile_definitions(${target} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()
endfunction()

set(XIUI_MINHOOK_SOURCES
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde32.c
)
add_library(xiui_minhook STATIC ${XIUI_MINHOOK_SOURCES})
target_include_directories(xiui_minhook PUBLIC third_party/minhook/include)

set(XIUI_HOST_SOURCES
    src/host/exports.def
    src/host/callback_gate.hpp
    src/host/plugin.cpp
    src/host/plugin.hpp
    src/host/native_hud_controller.cpp
    src/host/native_hud_controller.hpp
    src/host/native_menu_controller.cpp
    src/host/native_chat_source.cpp
    src/host/native_chat_source.hpp
    src/shared/native_chat_palette.cpp
    src/shared/native_chat_palette.hpp
    src/shared/native_hud_options.hpp
    src/shared/world_projection.hpp
    src/host/runtime_host.cpp
    src/host/runtime_host.hpp
    src/shared/runtime_api.hpp
)
add_library(${ASHITA_PROJECT_NAME} MODULE ${XIUI_HOST_SOURCES})
configure_xiui_target(${ASHITA_PROJECT_NAME})
target_link_libraries(${ASHITA_PROJECT_NAME} PRIVATE xiui_minhook)
set_target_properties(${ASHITA_PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${ASHITA_PROJECT_NAME}
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
if (MSVC)
    target_link_options(${ASHITA_PROJECT_NAME} PRIVATE /SAFESEH:NO)
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file("${CMAKE_SOURCE_DIR}/src/shared/version.hpp.in" "${CMAKE_BINARY_DIR}/generated/xiui_version.hpp" @ONLY)
configure_file("${CMAKE_SOURCE_DIR}/config/version.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-version.rc" @ONLY)
configure_file(
    "${CMAKE_SOURCE_DIR}/assets/cursors/cursors.rc.in"
    "${CMAKE_BINARY_DIR}/generated/xiui-cursors.rc"
    @ONLY
)
set(XIUI_CURSOR_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-cursors.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/jobs/jobs.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-jobs.rc" @ONLY)
set(XIUI_JOB_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-jobs.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/notifications/notifications.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-notifications.rc" @ONLY)
set(XIUI_NOTIFICATION_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-notifications.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/recast/recast.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-recast.rc" @ONLY)
set(XIUI_RECAST_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-recast.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/fonts/fonts.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-fonts.rc" @ONLY)
set(XIUI_FONT_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-fonts.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/chatmon/chatmon.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-chatmon.rc" @ONLY)
set(XIUI_CHATMON_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-chatmon.rc")
configure_file("${CMAKE_SOURCE_DIR}/config/defaults.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-defaults.rc" @ONLY)
set(XIUI_DEFAULTS_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-defaults.rc")
set_property(SOURCE "${XIUI_DEFAULTS_RESOURCE}" PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/config/xiuinative-defaults.ini;${CMAKE_SOURCE_DIR}/config/xiuinative-theme.ini;${CMAKE_SOURCE_DIR}/assets/cursors/LICENSE.txt;${CMAKE_SOURCE_DIR}/third_party/minhook/LICENSE.txt;${CMAKE_SOURCE_DIR}/assets/profiles/share-v2.ini")

set(XIUI_HUD_SOURCES
    src/shared/native_chat_palette.cpp
    src/shared/native_chat_palette.hpp
    src/runtime/world/world_source.cpp
    src/runtime/modules/compass/compass_module.cpp
    src/runtime/modules/world/world_info_module.cpp
    src/runtime/modules/commands/command_menu_module.cpp
    src/runtime/ui/native_ui_assets.cpp
    src/runtime/ui/native_ui_sounds.cpp
    src/runtime/ui/native_ui_sounds.hpp
    src/runtime/ui/embedded_sound_player.cpp
    src/runtime/ui/embedded_sound_player.hpp
    src/runtime/ui/native_window_config.cpp
    src/runtime/ui/native_window_config.hpp
    src/runtime/ui/button_renderer.hpp
    src/runtime/chat/chat_model.cpp
    src/runtime/chat/chat_alerts.cpp
    src/runtime/chat/chat_alerts.hpp
    src/runtime/chat/chat_colors.cpp
    src/runtime/chat/chat_colors.hpp
    src/runtime/chat/chat_editor.cpp
    src/runtime/chat/native_chat_input.cpp
    src/runtime/chat/native_chat_input.hpp
    src/runtime/chat/chat_encoding.cpp
    src/runtime/modules/chat/chat_module.cpp
    src/runtime/ui/glyph_atlas.cpp
    src/runtime/ui/text_rasterizer.cpp
    src/runtime/ui/font_catalog.cpp
    src/runtime/entities/entity_name_colors.hpp
    src/runtime/combat/action_packet.hpp
    src/runtime/combat/timing_model.cpp
    src/runtime/combat/combat_timing_source.cpp
    src/runtime/ui/combat_timing_display.cpp
    src/runtime/combat/combat_source.cpp
    src/runtime/config/config_controls.cpp
    src/runtime/config/config_text.cpp
    src/runtime/config/global_text.cpp
    src/runtime/config/global_text.hpp
    src/runtime/modules/chat/chat_history.cpp
    src/runtime/config/text_input.cpp
    src/runtime/config/text_input.hpp
    src/runtime/config/profile_layout.hpp
    src/runtime/config/share_key_view.hpp
    src/runtime/config/experience_display_editor.cpp
    src/runtime/config/experience_display_editor.hpp
    src/runtime/config/slider_value_input.cpp
    src/runtime/config/slider_value_input.hpp
    src/runtime/config/color_picker.cpp
    src/runtime/ui/icon_atlas.cpp
    src/runtime/config/config_controls.hpp
    src/runtime/config/config_layout.hpp
    src/runtime/design_system.cpp
    src/runtime/design_system.hpp
    src/runtime/hud_runtime.cpp
    src/runtime/hud_runtime_layout.cpp
    src/runtime/hud_runtime_elements.cpp
    src/runtime/hud_runtime_profiles.cpp
    src/runtime/hud_runtime_choices.cpp
    src/runtime/config/choice_dropdown.hpp
    src/runtime/config/movement_toolbar.hpp
    src/runtime/hud_runtime.hpp
    src/runtime/input/cursor_renderer.cpp
    src/runtime/input/cursor_renderer.hpp
    src/runtime/modules/module.cpp
    src/runtime/modules/module.hpp
    src/runtime/modules/bar_config.hpp
    src/runtime/modules/module_registry.hpp
    src/runtime/modules/player/player_module.cpp
    src/runtime/modules/player/player_module.hpp
    src/runtime/modules/castbar/player_cast_source.cpp
    src/runtime/modules/castbar/cast_bar_module.cpp
    src/runtime/modules/castcost/cast_cost_source.cpp
    src/runtime/modules/castcost/cast_cost_module.cpp
    src/runtime/modules/recast/recast_tracker.cpp
    src/runtime/modules/recast/recast_source.cpp
    src/runtime/modules/recast/recast_module.cpp
    src/runtime/modules/target/target_module.cpp
    src/runtime/modules/target/target_of_target_settings.cpp
    src/runtime/modules/target/target_of_target_settings.hpp
    src/runtime/modules/target/target_module.hpp
    src/runtime/modules/enemylist/enemy_list_module.cpp
    src/runtime/modules/enemylist/enemy_list_module.hpp
    src/runtime/modules/enemylist/enemy_source.cpp
    src/runtime/modules/enemylist/enemy_source.hpp
    src/runtime/modules/party/party_source.cpp
    src/runtime/modules/party/party_module.cpp
    src/runtime/modules/party/party_entry_layout.cpp
    src/runtime/modules/inventory/inventory_source.cpp
    src/runtime/modules/inventory/inventory_module.cpp
    src/runtime/modules/equipment/equipment_source.cpp
    src/runtime/modules/equipment/equipment_module.cpp
    src/runtime/modules/gil/gil_source.cpp
    src/runtime/modules/gil/gil_module.cpp
    src/runtime/modules/readycheck/ready_check_module.cpp
    src/runtime/modules/readycheck/ready_check_module.hpp
    src/runtime/modules/readycheck/ready_check_state.hpp
    src/runtime/modules/readycheck/ready_check_sound.hpp
    src/runtime/modules/experience/experience_module.cpp
    src/runtime/modules/experience/experience_module.hpp
    src/runtime/modules/experience/experience_config.cpp
    src/runtime/modules/experience/experience_tracker.cpp
    src/runtime/modules/experience/experience_source.cpp
    src/runtime/modules/experience/experience_sessions.cpp
    src/runtime/modules/experience/experience_readouts.cpp
    src/runtime/modules/experience/experience_readout_layout.hpp
    src/runtime/resources/skill_names.hpp
    src/runtime/resources/item_icon.hpp
    src/runtime/resources/menu_window.hpp
    src/runtime/resources/menu_texture_scale.hpp
    src/runtime/ui/game_icon_cache.cpp
    src/runtime/ui/game_icon_cache.hpp
    src/runtime/ui/nine_slice.hpp
    src/runtime/ui/meter_text_layout.hpp
    src/runtime/ui/meter_skin.hpp
    src/runtime/ui/container_renderer.cpp
    src/runtime/ui/container_renderer.hpp
    src/runtime/notifications/notification_center.cpp
    src/runtime/notifications/notification_center.hpp
    src/runtime/notifications/notification_source.cpp
    src/runtime/notifications/notification_source.hpp
    src/runtime/notifications/inventory_tracker.hpp
    src/runtime/notifications/notification_renderer.cpp
    src/runtime/notifications/notification_renderer.hpp
    src/runtime/modules/notifications/notification_module.cpp
    src/runtime/modules/notifications/notification_module.hpp
    src/runtime/modules/general/general_module.cpp
    src/runtime/modules/general/general_module.hpp
    src/runtime/ui/layout_grid.hpp
    src/runtime/ui/tp_meter.hpp
    src/runtime/ui/text_metrics.hpp
    src/runtime/modules/game_hud/game_hud_module.cpp
    src/runtime/modules/game_hud/game_hud_module.hpp
    src/shared/native_hud_options.hpp
    src/runtime/settings/settings_store.cpp
    src/runtime/settings/text_style_settings.cpp
    src/runtime/settings/text_style_settings.hpp
    src/runtime/settings/settings_profiles.cpp
    src/runtime/settings/profile_share.cpp
    src/runtime/settings/profile_share.hpp
    src/runtime/settings/profile_share_delta.cpp
    src/runtime/settings/profile_share_delta.hpp
    src/runtime/config/clipboard.hpp
    src/runtime/settings/settings_profiles.hpp
    src/runtime/settings/settings_store.hpp
    src/runtime/settings/settings_writer.cpp
    src/runtime/settings/settings_writer.hpp
    src/shared/runtime_api.hpp
    ${XIUI_CURSOR_RESOURCE}
    ${XIUI_JOB_RESOURCE}
    ${XIUI_NOTIFICATION_RESOURCE}
    ${XIUI_RECAST_RESOURCE}
    ${XIUI_FONT_RESOURCE}
    ${XIUI_CHATMON_RESOURCE}
    ${XIUI_DEFAULTS_RESOURCE}
)

add_library(xiuinative_runtime MODULE
    src/runtime/runtime_exports.def
    src/runtime/runtime_exports.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_target(xiuinative_runtime)
target_link_libraries(xiuinative_runtime PRIVATE
    "${ASHITA_D3D8_LIB_DIR}/d3d8.lib"
)
set_target_properties(xiuinative_runtime PROPERTIES
    OUTPUT_NAME "xiuinative-runtime"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
if (MSVC)
    target_link_options(xiuinative_runtime PRIVATE /SAFESEH:NO)
endif()

add_library(xiui_minhook_release STATIC ${XIUI_MINHOOK_SOURCES})
target_include_directories(xiui_minhook_release PUBLIC third_party/minhook/include)
set(XIUI_RELEASE_SOURCES ${XIUI_HOST_SOURCES} ${XIUI_HUD_SOURCES} src/runtime/runtime_exports.cpp "${CMAKE_BINARY_DIR}/generated/xiui-version.rc")
list(REMOVE_DUPLICATES XIUI_RELEASE_SOURCES)
add_library(xiuinative_release MODULE ${XIUI_RELEASE_SOURCES})
configure_xiui_target(xiuinative_release)
target_compile_definitions(xiuinative_release PRIVATE XIUI_SINGLE_DLL)
target_link_libraries(xiuinative_release PRIVATE xiui_minhook_release "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiuinative_release PROPERTIES
    OUTPUT_NAME "xiuinative"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release"
)
if (MSVC)
    set_target_properties(xiuinative_release xiui_minhook_release PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_options(xiuinative_release PRIVATE /SAFESEH:NO)
endif()

add_executable(xiui_single_dll_smoke tests/single_dll_smoke.cpp)
configure_xiui_target(xiui_single_dll_smoke)
set_target_properties(xiui_single_dll_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_renderer_smoke
    tests/renderer_smoke.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_target(xiui_renderer_smoke)
target_compile_definitions(xiui_renderer_smoke PRIVATE XIUI_ENABLE_TEST_FAULTS)
target_link_libraries(xiui_renderer_smoke PRIVATE
    "${ASHITA_D3D8_LIB_DIR}/d3d8.lib"
)
set_target_properties(xiui_renderer_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_callback_gate_smoke
    tests/callback_gate_smoke.cpp
    src/host/callback_gate.hpp
)
configure_xiui_target(xiui_callback_gate_smoke)
set_target_properties(xiui_callback_gate_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_timing_smoke tests/timing_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_timing_smoke)
target_link_libraries(xiui_timing_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_timing_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_config_input_smoke
    tests/config_input_smoke.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_target(xiui_config_input_smoke)
target_compile_definitions(xiui_config_input_smoke PRIVATE XIUI_ENABLE_INPUT_TESTS)
target_link_libraries(xiui_config_input_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_config_input_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_layout_grid_smoke tests/layout_grid_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_layout_grid_smoke)
target_compile_definitions(xiui_layout_grid_smoke PRIVATE XIUI_ENABLE_GRID_TESTS)
target_link_libraries(xiui_layout_grid_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_layout_grid_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_tp_meter_smoke tests/tp_meter_smoke.cpp)
configure_xiui_target(xiui_tp_meter_smoke)
set_target_properties(xiui_tp_meter_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_settings_smoke
    tests/settings_persistence_smoke.cpp
    src/runtime/settings/settings_store.cpp
    src/runtime/settings/settings_writer.cpp
)
configure_xiui_target(xiui_settings_smoke)
set_target_properties(xiui_settings_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_theme_reload_smoke
    src/runtime/ui/icon_atlas.cpp
    tests/theme_reload_smoke.cpp
    src/runtime/design_system.cpp
    src/runtime/design_system.hpp
    src/runtime/ui/glyph_atlas.cpp
    src/runtime/ui/text_rasterizer.cpp
    src/runtime/ui/font_catalog.cpp
    ${XIUI_FONT_RESOURCE}
)
configure_xiui_target(xiui_theme_reload_smoke)
target_link_libraries(xiui_theme_reload_smoke PRIVATE
    "${ASHITA_D3D8_LIB_DIR}/d3d8.lib"
)
set_target_properties(xiui_theme_reload_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_library(xiui_rejected_runtime MODULE
    tests/rejected_runtime.cpp
    tests/rejected_runtime.def
    src/shared/runtime_api.hpp
)
configure_xiui_target(xiui_rejected_runtime)
set_target_properties(xiui_rejected_runtime PROPERTIES
    OUTPUT_NAME "xiui-rejected-runtime"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

function(add_xiui_synthetic_runtime target output_name mode)
    add_library(${target} MODULE
        tests/synthetic_runtime.cpp
        tests/synthetic_runtime.def
        src/shared/runtime_api.hpp
    )
    configure_xiui_target(${target})
    target_compile_definitions(${target} PRIVATE XIUI_SYNTHETIC_MODE=${mode})
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
    )
endfunction()

add_xiui_synthetic_runtime(xiui_stateful_runtime "xiui-stateful-runtime" 0)
add_xiui_synthetic_runtime(xiui_activation_failure_runtime "xiui-activation-failure-runtime" 1)
add_xiui_synthetic_runtime(xiui_render_failure_runtime "xiui-render-failure-runtime" 2)
add_xiui_synthetic_runtime(xiui_exit_failure_runtime "xiui-exit-failure-runtime" 3)

add_executable(xiui_hot_reload_smoke
    tests/hot_reload_smoke.cpp
    src/host/runtime_host.cpp
    src/host/runtime_host.hpp
    src/shared/runtime_api.hpp
)
configure_xiui_target(xiui_hot_reload_smoke)
add_dependencies(
    xiui_hot_reload_smoke
    xiuinative_runtime
    xiui_rejected_runtime
    xiui_stateful_runtime
    xiui_activation_failure_runtime
    xiui_render_failure_runtime
    xiui_exit_failure_runtime
)
set_target_properties(xiui_hot_reload_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_foundations_smoke
    tests/foundations_smoke.cpp
    src/runtime/notifications/notification_center.cpp
)
configure_xiui_target(xiui_foundations_smoke)
set_target_properties(xiui_foundations_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_inventory_notifications_smoke tests/inventory_notifications_smoke.cpp)
configure_xiui_target(xiui_inventory_notifications_smoke)
set_target_properties(xiui_inventory_notifications_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

enable_testing()
add_executable(xiui_experience_smoke tests/experience_smoke.cpp src/runtime/modules/experience/experience_tracker.cpp)
configure_xiui_target(xiui_experience_smoke)
set_target_properties(xiui_experience_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_experience_smoke COMMAND xiui_experience_smoke)
add_executable(xiui_casting_smoke tests/casting_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_casting_smoke)
target_compile_definitions(xiui_casting_smoke PRIVATE XIUI_ENABLE_CAST_TESTS)
target_link_libraries(xiui_casting_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_casting_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_casting_smoke COMMAND xiui_casting_smoke)
add_executable(xiui_readycheck_smoke tests/readycheck_smoke.cpp)
configure_xiui_target(xiui_readycheck_smoke)
set_target_properties(xiui_readycheck_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_readycheck_smoke COMMAND xiui_readycheck_smoke)
add_executable(xiui_module_parity_smoke tests/module_parity_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_module_parity_smoke)
target_compile_definitions(xiui_module_parity_smoke PRIVATE XIUI_ENABLE_PARITY_TESTS)
target_link_libraries(xiui_module_parity_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_module_parity_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_module_parity_smoke COMMAND xiui_module_parity_smoke)
add_executable(xiui_inventory_smoke tests/inventory_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_inventory_smoke)
target_compile_definitions(xiui_inventory_smoke PRIVATE XIUI_ENABLE_INVENTORY_TESTS)
target_link_libraries(xiui_inventory_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_inventory_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_inventory_smoke COMMAND xiui_inventory_smoke)
add_executable(xiui_chat_smoke tests/chat_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_chat_smoke)
target_compile_definitions(xiui_chat_smoke PRIVATE XIUI_ENABLE_CHAT_TESTS)
target_link_libraries(xiui_chat_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_chat_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_chat_smoke COMMAND xiui_chat_smoke)
add_executable(xiui_world_smoke tests/world_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_target(xiui_world_smoke)
target_link_libraries(xiui_world_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_world_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_world_smoke COMMAND xiui_world_smoke)
add_test(NAME xiui_inventory_notifications_smoke COMMAND xiui_inventory_notifications_smoke)
add_executable(xiui_native_hud_target_smoke
    tests/native_hud_target_smoke.cpp
    src/host/native_hud_controller.cpp
    src/host/native_menu_controller.cpp
)
configure_xiui_target(xiui_native_hud_target_smoke)
target_compile_definitions(xiui_native_hud_target_smoke PRIVATE XIUI_ENABLE_NATIVE_HUD_TESTS)
target_link_libraries(xiui_native_hud_target_smoke PRIVATE xiui_minhook)
set_target_properties(xiui_native_hud_target_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)
if (MSVC)
    target_link_options(xiui_native_hud_target_smoke PRIVATE /SAFESEH:NO)
endif()
add_test(NAME xiui_native_hud_target_smoke COMMAND xiui_native_hud_target_smoke)
add_test(NAME xiui_timing_smoke COMMAND xiui_timing_smoke)
add_test(NAME xiui_config_input_smoke COMMAND xiui_config_input_smoke)
add_test(NAME xiui_layout_grid_smoke COMMAND xiui_layout_grid_smoke)
add_test(NAME xiui_tp_meter_smoke COMMAND xiui_tp_meter_smoke)
add_test(NAME xiui_foundations_smoke COMMAND xiui_foundations_smoke)
add_test(NAME xiui_renderer_smoke COMMAND xiui_renderer_smoke)
add_test(NAME xiui_callback_gate_smoke COMMAND xiui_callback_gate_smoke)
add_test(NAME xiui_theme_reload_smoke COMMAND xiui_theme_reload_smoke)
add_test(
    NAME xiui_hot_reload_smoke
    COMMAND xiui_hot_reload_smoke
        "$<TARGET_FILE:xiuinative_runtime>"
        "$<TARGET_FILE:xiui_rejected_runtime>"
        "$<TARGET_FILE:xiui_stateful_runtime>"
        "$<TARGET_FILE:xiui_activation_failure_runtime>"
        "$<TARGET_FILE:xiui_exit_failure_runtime>"
        "$<TARGET_FILE:xiui_render_failure_runtime>"
)
