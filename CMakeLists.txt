cmake_minimum_required(VERSION 3.22)

set(ASHITA_PROJECT_NAME "XIUIplus")
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" XIUI_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
project(${ASHITA_PROJECT_NAME} VERSION ${XIUI_VERSION} LANGUAGES C CXX)
# VERSION may omit the patch (e.g. 0.2); the version resource and Ashita number still need one.
if (NOT PROJECT_VERSION_PATCH)
    set(PROJECT_VERSION_PATCH 0)
endif()

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

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared dependencies" FORCE)
foreach(option WEBP_BUILD_ANIM_UTILS WEBP_BUILD_CWEBP WEBP_BUILD_DWEBP WEBP_BUILD_GIF2WEBP
    WEBP_BUILD_IMG2WEBP WEBP_BUILD_VWEBP WEBP_BUILD_WEBPINFO WEBP_BUILD_LIBWEBPMUX WEBP_BUILD_WEBPMUX WEBP_BUILD_EXTRAS WEBP_USE_THREAD)
    set(${option} OFF CACHE BOOL "Unused WebP feature" FORCE)
endforeach()
add_subdirectory(third_party/libwebp EXCLUDE_FROM_ALL)
set(XIUI_WEBP_DECODER_SOURCES)
foreach(component webpdecode webpdspdecode webputilsdecode)
    get_target_property(sources ${component} SOURCES)
    list(APPEND XIUI_WEBP_DECODER_SOURCES ${sources})
endforeach()
list(REMOVE_DUPLICATES XIUI_WEBP_DECODER_SOURCES)
foreach(source IN LISTS XIUI_WEBP_DECODER_SOURCES)
    foreach(property COMPILE_OPTIONS COMPILE_FLAGS COMPILE_DEFINITIONS)
        get_source_file_property(value "${source}" DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/libwebp" ${property})
        if(value)
            set_property(SOURCE "${source}" APPEND PROPERTY ${property} "${value}")
        endif()
    endforeach()
    set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_DEFINITIONS HAVE_CONFIG_H)
endforeach()
set_property(SOURCE third_party/miniz/miniz_tinfl.c PROPERTY COMPILE_DEFINITIONS
    MINIZ_NO_ARCHIVE_APIS MINIZ_NO_DEFLATE_APIS MINIZ_NO_ZLIB_APIS MINIZ_NO_TIME)

function(configure_xiui_target target)
    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/src/host"
        "${CMAKE_SOURCE_DIR}/src/runtime"
        "${CMAKE_SOURCE_DIR}/src/shared"
        "${CMAKE_BINARY_DIR}/generated"
        "${CMAKE_SOURCE_DIR}/third_party/libwebp"
        "${CMAKE_BINARY_DIR}/third_party/libwebp"
        "${CMAKE_SOURCE_DIR}/third_party/miniz"
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

include(cmake/models.cmake)
include(cmake/host.cmake)
include(cmake/resources.cmake)
include(cmake/runtime-sources.cmake)
include(cmake/runtime-targets.cmake)
enable_testing()
include(cmake/tests.cmake)
include(cmake/focused-checks.cmake)
