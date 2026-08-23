cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "yi3d")
project(${PROJECT_NAME} VERSION 0.19.0 LANGUAGES CXX)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUT_DIR "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR})
set(CMAKE_PDB_OUTPUT_DIRECTORY ${OUT_DIR})
set(CMAKE_DEBUG_POSTFIX "d")

if(MSVC)
    add_compile_options("/utf-8")
    # 编译选项:保留调试信息,生成PDB文件
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    # 链接选项:生成调试信息,但不影响优化
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CPM)
include(muparser)
include(pybind11)
include(gtest)
if (UNIX)
    include(osgQt)
endif()

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/3rdParty/bundles/wyaf")
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/3rdParty/bundles/qt5")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/3rdParty/bundles/occt")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/3rdParty/bundles/osg")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/3rdParty/bundles/python3")
endif()

file(COPY "${CMAKE_SOURCE_DIR}/scripts" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(COPY "${CMAKE_SOURCE_DIR}/samples" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(COPY "${CMAKE_SOURCE_DIR}/skills" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/bundles/python3/bin/debug/" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python3")
    else()
        file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/bundles/python3/bin/release/" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python3")
    endif()
    # Pure-Python packages (ezdxf etc.) are optional at configure time: without
    # them the build succeeds and the DXF commands report a runtime error
    if(EXISTS "${CMAKE_SOURCE_DIR}/3rdParty/bundles/python3/packages")
        file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/bundles/python3/packages/" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python3")
    endif()
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/bundles/qt5/bin/Qt5Svg.dll" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/bundles/qt5/bin/Qt5Svgd.dll" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/bundles/qt5/plugins" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else()
    find_package(Python3 REQUIRED COMPONENTS Development)
    get_filename_component(_python3_lib_dir "${Python3_LIBRARIES}" DIRECTORY)
    get_filename_component(_python3_lib_name "${Python3_LIBRARIES}" NAME)
    file(GLOB _python3_so_files "${_python3_lib_dir}/${_python3_lib_name}*")
    file(COPY ${_python3_so_files} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python3")
    set(PYTHON3_LIB_NAME "${_python3_lib_name}")
endif()

# Centrally find OpenCASCADE so all targets inherit include dirs
find_package(OpenCASCADE REQUIRED)
if(UNIX)
    include_directories(SYSTEM ${OpenCASCADE_INCLUDE_DIR})
endif()

add_subdirectory(src/wy3d)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/wy3dApp/wy3dAppVersion.h.in"
    "${CMAKE_SOURCE_DIR}/src/wy3dApp/wy3dAppVersion.h")
add_subdirectory(src/wy3dApp)
add_subdirectory(src/wy3dPY)
add_subdirectory(src/unitTest)
