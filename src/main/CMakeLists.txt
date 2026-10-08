cmake_minimum_required(VERSION 3.15)

set(APPLICATION_NAME "YI3D")

add_executable(${APPLICATION_NAME} main.cpp)

if(WIN32)
    target_sources(${APPLICATION_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources/Yi3D.rc)
endif()

target_link_libraries(${APPLICATION_NAME} PRIVATE wy3dApp)

# Release版本不需要命令行窗口
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(${APPLICATION_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# 编译后自动拷贝运行时依赖DLL(仅Windows)
if(WIN32)
    add_custom_command(TARGET ${APPLICATION_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_FILE=$<TARGET_FILE:${APPLICATION_NAME}>
            -DOUT_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            -DSEARCH_DIRS="${CMAKE_SOURCE_DIR}/3rdParty/bundles"
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
    )
endif()
