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
file(GLOB XIUI_ICON_THEME_PAYLOADS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/icon-themes/*.bgra" "${CMAKE_SOURCE_DIR}/assets/icon-themes/*.txt")
set_property(SOURCE "${XIUI_JOB_RESOURCE}" PROPERTY OBJECT_DEPENDS "${XIUI_ICON_THEME_PAYLOADS}")
set_property(SOURCE "${XIUI_JOB_RESOURCE}" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/pets/portraits.bgra;${CMAKE_SOURCE_DIR}/assets/pets/NOTICES.txt")
set_property(SOURCE "${XIUI_JOB_RESOURCE}" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/minimap/maps.bin;${CMAKE_SOURCE_DIR}/assets/minimap/NOTICES.txt;${CMAKE_SOURCE_DIR}/assets/minimap/LICENSE.txt")
set_property(SOURCE "${XIUI_JOB_RESOURCE}" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/actionbars/actions.bgra;${CMAKE_SOURCE_DIR}/assets/actionbars/NOTICES.txt")
set_property(SOURCE "${XIUI_JOB_RESOURCE}" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/actionbars/vincerno/icons.bgra;${CMAKE_SOURCE_DIR}/assets/actionbars/vincerno/NOTICES.txt;${CMAKE_SOURCE_DIR}/assets/actionbars/vincerno/LICENSE.txt")
configure_file("${CMAKE_SOURCE_DIR}/assets/notifications/notifications.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-notifications.rc" @ONLY)
set(XIUI_NOTIFICATION_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-notifications.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/recast/recast.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-recast.rc" @ONLY)
set(XIUI_RECAST_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-recast.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/fonts/fonts.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-fonts.rc" @ONLY)
set(XIUI_FONT_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-fonts.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/xiv1/xiv1.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-xiv1.rc" @ONLY)
set(XIUI_XIV1_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-xiv1.rc")
set_property(SOURCE "${XIUI_XIV1_RESOURCE}" PROPERTY OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/xiv1/theme.bgra;${CMAKE_SOURCE_DIR}/assets/xiv1/NOTICE.txt")
configure_file("${CMAKE_SOURCE_DIR}/assets/chatmon/chatmon.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-chatmon.rc" @ONLY)
set(XIUI_CHATMON_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-chatmon.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/mobs/mobs.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-mobs.rc" @ONLY)
set(XIUI_MOB_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-mobs.rc")
configure_file("${CMAKE_SOURCE_DIR}/assets/bluemagic/bluemagic.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-bluemagic.rc" @ONLY)
set(XIUI_BLUE_MAGIC_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-bluemagic.rc")
set_property(SOURCE "${XIUI_BLUE_MAGIC_RESOURCE}" PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/bluemagic/blue_magic_learned.bgra;${CMAKE_SOURCE_DIR}/assets/bluemagic/blue_magic_learned.wav;${CMAKE_SOURCE_DIR}/assets/bluemagic/LICENSE.txt")
set_property(SOURCE "${XIUI_MOB_RESOURCE}" PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/mobs/phoenix_mobs.bin;${CMAKE_SOURCE_DIR}/assets/mobs/README.md;${CMAKE_SOURCE_DIR}/assets/mobs/LICENSE-GPL-3.0.txt;${CMAKE_SOURCE_DIR}/assets/mobs/icons/physical.bgra;${CMAKE_SOURCE_DIR}/assets/mobs/icons/detection.bgra;${CMAKE_SOURCE_DIR}/assets/mobs/icons/LICENSE.txt")
configure_file("${CMAKE_SOURCE_DIR}/config/defaults.rc.in" "${CMAKE_BINARY_DIR}/generated/xiui-defaults.rc" @ONLY)
set(XIUI_DEFAULTS_RESOURCE "${CMAKE_BINARY_DIR}/generated/xiui-defaults.rc")
set_property(SOURCE "${XIUI_DEFAULTS_RESOURCE}" PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/config/xiuinative-defaults.ini;${CMAKE_SOURCE_DIR}/config/xiuinative-theme.ini;${CMAKE_SOURCE_DIR}/assets/cursors/LICENSE.txt;${CMAKE_SOURCE_DIR}/third_party/minhook/LICENSE.txt;${CMAKE_SOURCE_DIR}/assets/profiles/share-v2.ini;${CMAKE_SOURCE_DIR}/config/party-classic.ini;${CMAKE_SOURCE_DIR}/config/party-xiv.ini;${CMAKE_SOURCE_DIR}/assets/status/NOTICE.txt;${CMAKE_SOURCE_DIR}/assets/status/LICENSE.txt")
set_property(SOURCE "${XIUI_DEFAULTS_RESOURCE}" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/satchel/NOTICES.txt;${CMAKE_SOURCE_DIR}/assets/satchel/LICENSE.txt")
set_property(SOURCE "${XIUI_DEFAULTS_RESOURCE}" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/config/hud-classic.ini;${CMAKE_SOURCE_DIR}/config/hud-xiv.ini;${CMAKE_SOURCE_DIR}/config/hud-xivclassic.ini")

function(pack_xiui_resource variable)
    set(source "${${variable}}")
    get_filename_component(name "${source}" NAME)
    set(output "${CMAKE_BINARY_DIR}/generated/packed/${name}")
    get_source_file_property(dependencies "${source}" OBJECT_DEPENDS)
    if(NOT dependencies)
        set(dependencies)
    endif()
    set(byproducts)
    file(STRINGS "${source}" rows REGEX "RCDATA")
    foreach(row IN LISTS rows)
        if(row MATCHES "^[ \t]*([0-9]+)[ \t]+RCDATA[ \t]+\"([^\"]+)\"")
            set(id "${CMAKE_MATCH_1}")
            set(payload "${CMAKE_MATCH_2}")
            list(APPEND dependencies "${payload}")
            if(payload MATCHES "\\.bgra$|/phoenix_mobs\\.bin$")
                get_filename_component(filename "${payload}" NAME)
                list(APPEND byproducts "${CMAKE_BINARY_DIR}/generated/packed/${id}-${filename}.zlib")
            endif()
        endif()
    endforeach()
    add_custom_command(OUTPUT "${output}"
        BYPRODUCTS ${byproducts}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/pack-resources.py" "${source}" "${output}"
        DEPENDS "${source}" "${CMAKE_SOURCE_DIR}/scripts/pack-resources.py" ${dependencies}
        VERBATIM)
    set(${variable} "${output}" PARENT_SCOPE)
endfunction()
foreach(resource XIUI_CURSOR_RESOURCE XIUI_JOB_RESOURCE XIUI_NOTIFICATION_RESOURCE XIUI_RECAST_RESOURCE
    XIUI_FONT_RESOURCE XIUI_CHATMON_RESOURCE XIUI_DEFAULTS_RESOURCE XIUI_MOB_RESOURCE XIUI_BLUE_MAGIC_RESOURCE XIUI_XIV1_RESOURCE)
    pack_xiui_resource(${resource})
endforeach()
