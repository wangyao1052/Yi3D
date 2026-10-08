cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "demo")
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

# YI3D demo plugin: SDK template plus host verification case.
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
find_package(wydb REQUIRED)
find_package(wyap REQUIRED)

add_library(${PROJECT_NAME} SHARED
    DemoPlugin.cpp
    DemoHelloCmd.cpp
    resources/demo.qrc
)

# wy3dApp: host framework (SDK face); wyap/wydb: command framework and
# runtime class registration.
target_link_libraries(${PROJECT_NAME} PRIVATE wy3dApp wyap wydb Qt5::Widgets)

include(${CMAKE_SOURCE_DIR}/sdk/Yi3DPluginSdk.cmake)
yi3d_add_plugin(${PROJECT_NAME} ID "com.yi3d.demo" MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini")
