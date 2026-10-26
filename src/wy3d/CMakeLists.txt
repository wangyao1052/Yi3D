cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "wy3d")
set(LIBRARY_NAME ${PROJECT_NAME})
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

add_definitions(-DWY3D_LIBRARY)

find_package(wydb)
find_package(ModelRefine CONFIG REQUIRED)

file(GLOB_RECURSE SRCS ./*.cpp)
list(APPEND SRCS "${CMAKE_SOURCE_DIR}/3rdParty/base64/base64.cpp")

add_library(${LIBRARY_NAME} SHARED ${SRCS})

target_include_directories(${LIBRARY_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/inc"
    "${CMAKE_SOURCE_DIR}/3rdParty"
    "${CMAKE_SOURCE_DIR}/3rdParty/bundles/include"
    "${CMAKE_SOURCE_DIR}/src"
)

if(WIN32)
    target_include_directories(${LIBRARY_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/3rdParty/bundles/osg/include"
    )
endif()

# link wy libraries
target_link_libraries(${LIBRARY_NAME} wydb)
# link OpenCasCade libraries
find_package(OpenCASCADE REQUIRED)
find_package(TBB REQUIRED)
find_package(Freetype REQUIRED)
target_link_libraries(${LIBRARY_NAME}
TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKGeomAlgo TKTopAlgo TKPrim 
TKBO TKBool TKFeat TKFillet TKOffset TKHLR TKMesh TKShHealing TKXSBase TKSTEP TKIGES)
target_link_libraries(${LIBRARY_NAME} Freetype::Freetype)
# link ModelRefine (separate LGPL-2.1-or-later shared library)
target_link_libraries(${LIBRARY_NAME} ModelRefine::ModelRefine)