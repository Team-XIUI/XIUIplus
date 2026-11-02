set(XIUI_MODEL_SOURCES
    src/runtime/modules/experience/experience_tracker.cpp
    src/runtime/notifications/notification_center.cpp
)
foreach(target xiui_models xiui_models_release)
    add_library(${target} STATIC ${XIUI_MODEL_SOURCES})
    configure_xiui_target(${target})
endforeach()
if(MSVC)
    set_target_properties(xiui_models PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set_target_properties(xiui_models_release PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(configure_xiui_hud_target target)
    configure_xiui_target(${target})
    if(target STREQUAL "XIUIplus_release")
        target_link_libraries(${target} PRIVATE xiui_models_release)
    else()
        target_link_libraries(${target} PRIVATE xiui_models)
    endif()
endfunction()
