# YI3D plugin SDK helper.
#
# In-tree usage: the plugin links the wy3dApp target and calls
#   include(${YI3D_HOST_SOURCE_DIR}/sdk/Yi3DPluginSdk.cmake)
#   yi3d_add_plugin(<target> ID <plugin-id> MANIFEST <plugin.ini>)
#
# A frozen out-of-tree SDK package (prebuilt import libraries) is planned for a
# later phase.

# Plugin SDK headers live with the host public headers (inc/) and are included
# as <plugin/...>.
get_filename_component(YI3D_SDK_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../inc" ABSOLUTE)

function(yi3d_add_plugin TARGET)
    cmake_parse_arguments(ARG "" "ID;MANIFEST" "" ${ARGN})
    if(NOT ARG_ID)
        message(FATAL_ERROR "yi3d_add_plugin(${TARGET}): ID is required")
    endif()

    set(_plugin_out_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/${ARG_ID}")
    # The DLL file name is part of the deployment contract: plugin.ini states it
    # verbatim, so strip PREFIX and the config postfix (CMAKE_DEBUG_POSTFIX).
    set_target_properties(${TARGET} PROPERTIES
        PREFIX ""
        DEBUG_POSTFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${_plugin_out_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${_plugin_out_dir}"
        AUTOMOC ON
        AUTORCC ON)

    target_include_directories(${TARGET} PRIVATE "${YI3D_SDK_INCLUDE_DIR}")

    if(ARG_MANIFEST)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ARG_MANIFEST}" "$<TARGET_FILE_DIR:${TARGET}>/plugin.ini")
    endif()
endfunction()
