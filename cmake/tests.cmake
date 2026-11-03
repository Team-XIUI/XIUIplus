add_executable(xiui_localization_smoke tests/localization_smoke.cpp src/runtime/settings/settings_store.cpp)
configure_xiui_target(xiui_localization_smoke)
set_target_properties(xiui_localization_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_localization_smoke COMMAND xiui_localization_smoke)

add_executable(xiui_single_dll_smoke tests/single_dll_smoke.cpp)
configure_xiui_target(xiui_single_dll_smoke)
set_target_properties(xiui_single_dll_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_ashita_compat_smoke tests/ashita_compat_smoke.cpp)
configure_xiui_target(xiui_ashita_compat_smoke)
set_target_properties(xiui_ashita_compat_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
foreach(interface IN ITEMS 4.16 4.30)
    string(REPLACE "." "_" suffix "${interface}")
    add_library(xiui_ashita_${suffix}_fixture MODULE tests/ashita_host_fixture.cpp tests/ashita_host_fixture.def)
    target_compile_definitions(xiui_ashita_${suffix}_fixture PRIVATE XIUI_TEST_ASHITA_INTERFACE=${interface})
    set_target_properties(xiui_ashita_${suffix}_fixture PROPERTIES OUTPUT_NAME Ashita PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/ashita-${interface}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/ashita-${interface}"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/ashita-${interface}")
endforeach()

add_library(xiui_ashita_facade_fixture MODULE tests/ashita_host_fixture.cpp tests/ashita_facade_fixture.def)
target_compile_definitions(xiui_ashita_facade_fixture PRIVATE XIUI_TEST_ASHITA_FACADE)
set_target_properties(xiui_ashita_facade_fixture PROPERTIES OUTPUT_NAME CoreFacade PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/ashita-facade"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/ashita-facade"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/ashita-facade")

add_executable(xiui_storage_paths_smoke tests/storage_paths_smoke.cpp src/runtime/settings/storage_paths.cpp src/runtime/settings/settings_store.cpp)
configure_xiui_target(xiui_storage_paths_smoke)
set_target_properties(xiui_storage_paths_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

set(XIUI_SCENE_TEST_SOURCES ${XIUI_HOST_SOURCES})
list(FILTER XIUI_SCENE_TEST_SOURCES EXCLUDE REGEX "\\.def$")
add_executable(xiui_world_overlay_smoke tests/world_overlay_smoke.cpp ${XIUI_SCENE_TEST_SOURCES})
configure_xiui_target(xiui_world_overlay_smoke)
target_compile_definitions(xiui_world_overlay_smoke PRIVATE XIUI_ENABLE_SCENE_TESTS)
target_link_libraries(xiui_world_overlay_smoke PRIVATE xiui_minhook "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_world_overlay_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
if (MSVC)
    target_link_options(xiui_world_overlay_smoke PRIVATE /SAFESEH:NO)
endif()

add_executable(xiui_capture_defaults tools/capture_defaults.cpp src/runtime/settings/settings_store.cpp)
configure_xiui_target(xiui_capture_defaults)
set_target_properties(xiui_capture_defaults PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tools")

add_executable(xiui_resource_smoke
    tests/resource_smoke.cpp
    src/runtime/resources/embedded_resource.cpp
    src/runtime/modules/minimap/minimap_maps.cpp
    src/runtime/resources/wic_image.cpp
    third_party/miniz/miniz_tinfl.c
    ${XIUI_WEBP_DECODER_SOURCES})
configure_xiui_target(xiui_resource_smoke)
set_target_properties(xiui_resource_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_dependencies(xiui_resource_smoke XIUIplus_release)
add_test(NAME xiui_resource_smoke COMMAND xiui_resource_smoke "$<TARGET_FILE:XIUIplus_release>" "${CMAKE_SOURCE_DIR}")

add_executable(xiui_renderer_smoke
    tests/renderer_smoke.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_hud_target(xiui_renderer_smoke)
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
configure_xiui_hud_target(xiui_timing_smoke)
target_link_libraries(xiui_timing_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_timing_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_config_input_smoke
    tests/setup_wizard_checks.cpp
    tests/config_input_smoke.cpp
    tests/module_menu_checks.cpp
    tests/edit_mode_checks.cpp
    tests/window_input_checks.cpp
    tests/config_check_support.cpp
    tests/config_profile_checks.cpp
    tests/lua_profile_checks.cpp
    tests/config_text_checks.cpp
    tests/config_navigation_checks.cpp
    tests/hud_scale_checks.cpp
    tests/config_module_checks.cpp
    tests/config_render_checks.cpp
    tests/config_control_checks.cpp
    tests/config_responsive_checks.cpp
    tests/mp_cost_preview_checks.cpp
    tests/heal_prediction_checks.cpp
    tests/target_ring_checks.cpp
    tests/player_resource_checks.cpp
    tests/target_display_checks.cpp
    tests/ready_check_sound_checks.cpp
    tests/blue_magic_checks.cpp
    tests/phantom_roll_checks.cpp
    tests/mob_info_checks.cpp
    tests/icon_theme_checks.cpp
    tests/pet_hud_checks.cpp
    tests/notification_feedback_checks.cpp
    tests/minimap_checks.cpp
    tests/skillchain_checks.cpp
    tests/phoenix_integration_checks.cpp
    tests/target_child_checks.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_hud_target(xiui_config_input_smoke)
target_compile_definitions(xiui_config_input_smoke PRIVATE XIUI_ENABLE_INPUT_TESTS XIUI_ENABLE_PARITY_TESTS)
target_link_libraries(xiui_config_input_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_config_input_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests"
)

add_executable(xiui_layout_grid_smoke tests/layout_grid_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_layout_grid_smoke)
target_compile_definitions(xiui_layout_grid_smoke PRIVATE XIUI_ENABLE_GRID_TESTS)
target_link_libraries(xiui_layout_grid_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_layout_grid_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_tp_meter_smoke tests/tp_meter_smoke.cpp)
configure_xiui_target(xiui_tp_meter_smoke)
set_target_properties(xiui_tp_meter_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")

add_executable(xiui_native_pattern_smoke tests/native_pattern_smoke.cpp)
configure_xiui_target(xiui_native_pattern_smoke)
set_target_properties(xiui_native_pattern_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_native_pattern_smoke COMMAND xiui_native_pattern_smoke)

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
    tests/controller_host_checks.cpp
    src/host/runtime_host.cpp
    src/host/runtime_host.hpp
    src/shared/runtime_api.hpp
)
configure_xiui_target(xiui_hot_reload_smoke)
add_dependencies(
    xiui_hot_reload_smoke
    XIUIplus_runtime
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

add_executable(xiui_satchel_actions_smoke tests/satchel_actions_smoke.cpp src/runtime/modules/satchel/satchel_actions.cpp)
configure_xiui_target(xiui_satchel_actions_smoke)
set_target_properties(xiui_satchel_actions_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_satchel_actions_smoke COMMAND xiui_satchel_actions_smoke)
add_executable(xiui_satchel_data_smoke
    tests/satchel_data_smoke.cpp
    src/runtime/modules/satchel/satchel_source.cpp
    src/runtime/modules/satchel/satchel_identity.cpp
    src/runtime/modules/satchel/satchel_cache.cpp
    src/runtime/settings/settings_store.cpp
    src/runtime/settings/settings_writer.cpp
    src/runtime/chat/chat_encoding.cpp)
configure_xiui_target(xiui_satchel_data_smoke)
target_compile_definitions(xiui_satchel_data_smoke PRIVATE XIUI_ENABLE_SATCHEL_TESTS)
set_target_properties(xiui_satchel_data_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_satchel_data_smoke COMMAND xiui_satchel_data_smoke)
add_executable(xiui_satchel_ui_smoke tests/satchel_ui_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_satchel_ui_smoke)
target_compile_definitions(xiui_satchel_ui_smoke PRIVATE XIUI_ENABLE_SATCHEL_TESTS)
target_link_libraries(xiui_satchel_ui_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_satchel_ui_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_satchel_ui_smoke COMMAND xiui_satchel_ui_smoke)
add_executable(xiui_controller_input_smoke tests/controller_input_smoke.cpp)
configure_xiui_target(xiui_controller_input_smoke)
set_target_properties(xiui_controller_input_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_controller_input_smoke COMMAND xiui_controller_input_smoke)

add_executable(xiui_crossbar_input_smoke tests/crossbar_input_smoke.cpp)
configure_xiui_target(xiui_crossbar_input_smoke)
set_target_properties(xiui_crossbar_input_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_crossbar_input_smoke COMMAND xiui_crossbar_input_smoke)

add_executable(xiui_custom_icon_smoke tests/custom_icon_smoke.cpp
    src/runtime/resources/custom_icon_library.cpp src/runtime/resources/wic_image.cpp)
configure_xiui_target(xiui_custom_icon_smoke)
set_target_properties(xiui_custom_icon_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_custom_icon_smoke COMMAND xiui_custom_icon_smoke)

add_executable(xiui_keyboard_suppression_smoke tests/keyboard_suppression_smoke.cpp)
configure_xiui_target(xiui_keyboard_suppression_smoke)
set_target_properties(xiui_keyboard_suppression_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_keyboard_suppression_smoke COMMAND xiui_keyboard_suppression_smoke)

add_executable(xiui_actionbars_data_smoke tests/actionbars_data_smoke.cpp
    src/runtime/modules/actionbars/action_store.cpp src/runtime/modules/actionbars/action_executor.cpp
    src/runtime/modules/actionbars/action_import.cpp src/runtime/settings/settings_store.cpp src/runtime/settings/lua_data.cpp
    src/runtime/chat/chat_encoding.cpp src/runtime/combat/ability_recast.cpp)
configure_xiui_target(xiui_actionbars_data_smoke)
set_target_properties(xiui_actionbars_data_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_actionbars_data_smoke COMMAND xiui_actionbars_data_smoke)

add_executable(xiui_actionbars_ui_smoke tests/actionbars_ui_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_actionbars_ui_smoke)
target_compile_definitions(xiui_actionbars_ui_smoke PRIVATE XIUI_ENABLE_ACTIONBAR_TESTS)
target_link_libraries(xiui_actionbars_ui_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_actionbars_ui_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_actionbars_ui_smoke COMMAND xiui_actionbars_ui_smoke)
add_test(NAME xiui_migration_capabilities_smoke COMMAND xiui_actionbars_ui_smoke --migration-capabilities-only)
add_test(NAME xiui_custom_icons_smoke COMMAND xiui_actionbars_ui_smoke --custom-icons-only)

add_executable(xiui_controller_smoke
    tests/controller_smoke.cpp
    tests/controller_modules_checks.cpp
    tests/controller_config_checks.cpp
    tests/party_timing_checks.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_hud_target(xiui_controller_smoke)
target_compile_definitions(xiui_controller_smoke PRIVATE XIUI_ENABLE_INPUT_TESTS XIUI_ENABLE_CHAT_TESTS)
target_link_libraries(xiui_controller_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_controller_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_controller_smoke COMMAND xiui_controller_smoke)
add_test(NAME xiui_party_timing_smoke COMMAND xiui_controller_smoke --party-timing-only)

add_executable(xiui_experience_smoke tests/experience_smoke.cpp)
configure_xiui_target(xiui_experience_smoke)
target_link_libraries(xiui_experience_smoke PRIVATE xiui_models)
set_target_properties(xiui_experience_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_experience_smoke COMMAND xiui_experience_smoke)
add_executable(xiui_casting_smoke tests/casting_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_casting_smoke)
target_compile_definitions(xiui_casting_smoke PRIVATE XIUI_ENABLE_CAST_TESTS)
target_link_libraries(xiui_casting_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_casting_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_casting_smoke COMMAND xiui_casting_smoke)
add_executable(xiui_readycheck_smoke tests/readycheck_smoke.cpp)
configure_xiui_target(xiui_readycheck_smoke)
set_target_properties(xiui_readycheck_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_readycheck_smoke COMMAND xiui_readycheck_smoke)
add_executable(xiui_module_parity_smoke
    tests/module_parity_smoke.cpp
    tests/enemy_layout_checks.cpp
    tests/visibility_checks.cpp
    tests/status_display_checks.cpp
    tests/status_module_checks.cpp
    ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_module_parity_smoke)
target_compile_definitions(xiui_module_parity_smoke PRIVATE XIUI_ENABLE_PARITY_TESTS)
target_link_libraries(xiui_module_parity_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_module_parity_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_module_parity_smoke COMMAND xiui_module_parity_smoke)
add_executable(xiui_inventory_smoke tests/inventory_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_inventory_smoke)
target_compile_definitions(xiui_inventory_smoke PRIVATE XIUI_ENABLE_INVENTORY_TESTS)
target_link_libraries(xiui_inventory_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_inventory_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_inventory_smoke COMMAND xiui_inventory_smoke)
add_executable(xiui_chat_smoke tests/chat_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_chat_smoke)
target_compile_definitions(xiui_chat_smoke PRIVATE XIUI_ENABLE_CHAT_TESTS)
target_link_libraries(xiui_chat_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_chat_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_chat_smoke COMMAND xiui_chat_smoke)
add_test(NAME xiui_chat_native_channel_smoke COMMAND xiui_chat_smoke --native-channel-only)
add_executable(xiui_world_smoke tests/world_smoke.cpp ${XIUI_HUD_SOURCES})
configure_xiui_hud_target(xiui_world_smoke)
target_link_libraries(xiui_world_smoke PRIVATE "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(xiui_world_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests")
add_test(NAME xiui_world_smoke COMMAND xiui_world_smoke)
add_test(NAME xiui_inventory_notifications_smoke COMMAND xiui_inventory_notifications_smoke)
add_executable(xiui_native_hud_target_smoke
    tests/native_hud_target_smoke.cpp
    src/host/native_hud_controller.cpp
    src/runtime/modules/actionbars/native_macros.cpp
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
add_test(NAME xiui_settings_presentation_smoke COMMAND xiui_config_input_smoke --settings-presentation-only)
add_test(NAME xiui_party_name_cast_smoke COMMAND xiui_config_input_smoke --party-name-cast-only)
add_test(NAME xiui_layout_grid_smoke COMMAND xiui_layout_grid_smoke)
add_test(NAME xiui_tp_meter_smoke COMMAND xiui_tp_meter_smoke)
add_test(NAME xiui_foundations_smoke COMMAND xiui_foundations_smoke)
add_test(NAME xiui_renderer_smoke COMMAND xiui_renderer_smoke)
add_test(NAME xiui_callback_gate_smoke COMMAND xiui_callback_gate_smoke)
add_test(NAME xiui_theme_reload_smoke COMMAND xiui_theme_reload_smoke)
add_test(
    NAME xiui_hot_reload_smoke
    COMMAND xiui_hot_reload_smoke
        "$<TARGET_FILE:XIUIplus_runtime>"
        "$<TARGET_FILE:xiui_rejected_runtime>"
        "$<TARGET_FILE:xiui_stateful_runtime>"
        "$<TARGET_FILE:xiui_activation_failure_runtime>"
        "$<TARGET_FILE:xiui_exit_failure_runtime>"
        "$<TARGET_FILE:xiui_render_failure_runtime>"
)
