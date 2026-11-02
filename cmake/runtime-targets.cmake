add_library(XIUIplus_runtime MODULE
    src/runtime/runtime_exports.def
    src/runtime/runtime_exports.cpp
    ${XIUI_HUD_SOURCES}
)
configure_xiui_hud_target(XIUIplus_runtime)
target_link_libraries(XIUIplus_runtime PRIVATE
    "${ASHITA_D3D8_LIB_DIR}/d3d8.lib"
)
set_target_properties(XIUIplus_runtime PROPERTIES
    OUTPUT_NAME "XIUIplus-runtime"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
if (MSVC)
    target_link_options(XIUIplus_runtime PRIVATE /SAFESEH:NO)
endif()

add_library(xiui_minhook_release STATIC ${XIUI_MINHOOK_SOURCES})
target_include_directories(xiui_minhook_release PUBLIC third_party/minhook/include)
set(XIUI_RELEASE_SOURCES ${XIUI_HOST_SOURCES} ${XIUI_HUD_SOURCES} src/runtime/runtime_exports.cpp "${CMAKE_BINARY_DIR}/generated/xiui-version.rc")
list(REMOVE_DUPLICATES XIUI_RELEASE_SOURCES)
add_library(XIUIplus_release MODULE ${XIUI_RELEASE_SOURCES})
configure_xiui_hud_target(XIUIplus_release)
target_compile_definitions(XIUIplus_release PRIVATE XIUI_SINGLE_DLL)
target_link_libraries(XIUIplus_release PRIVATE xiui_minhook_release "${ASHITA_D3D8_LIB_DIR}/d3d8.lib")
set_target_properties(XIUIplus_release PROPERTIES
    OUTPUT_NAME "XIUIplus"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release"
)
if (MSVC)
    set_target_properties(XIUIplus_release xiui_minhook_release PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_options(XIUIplus_release PRIVATE /SAFESEH:NO)
endif()
