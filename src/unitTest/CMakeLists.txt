cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "unitTest")
set(EXE_NAME ${PROJECT_NAME})
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

include(gtest)

find_package(wydb REQUIRED)

file(GLOB_RECURSE SRCS ./*.cpp)
add_executable(${EXE_NAME} ${SRCS})

# PluginManifest and SketchRectangleRecognizer are pure C++ (no Qt/OSG):
# compile them straight into the test binary
target_sources(${EXE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src/wy3dApp/plugins/PluginManifest.cpp"
    "${CMAKE_SOURCE_DIR}/src/wy3dApp/utils/SketchRectangleRecognizer.cpp"
)

target_include_directories(${EXE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/inc"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/wy3dApp"
)

target_link_libraries(${EXE_NAME} PRIVATE wydb wy3d gtest)
