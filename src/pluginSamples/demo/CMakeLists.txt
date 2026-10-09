cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "demo")
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
find_package(wydb REQUIRED)
find_package(wyap REQUIRED)

file(GLOB DEMO_SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc"
)

add_library(${PROJECT_NAME} SHARED ${DEMO_SRCS})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets wydb wyap wy3d wy3dApp)

include(${CMAKE_SOURCE_DIR}/sdk/Yi3DPluginSdk.cmake)
yi3d_add_plugin(${PROJECT_NAME} ID "com.yi3d.demo" MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini")
