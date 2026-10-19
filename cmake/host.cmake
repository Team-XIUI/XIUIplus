set(XIUI_HOST_SOURCES
    src/host/exports.def
    src/host/callback_gate.hpp
    src/host/controller_input.cpp
    src/host/controller_input.hpp
    src/host/controller_normalizer.hpp
    src/shared/controller_input.hpp
    src/host/plugin.cpp
    src/host/plugin.hpp
    src/host/native_hud_controller.cpp
    src/host/native_hud_controller.hpp
    src/runtime/modules/actionbars/native_macros.cpp
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
