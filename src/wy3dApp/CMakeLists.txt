cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "wy3dApp")
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

# qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL OpenGLExtensions Network LinguistTools REQUIRED)
find_package(OpenSceneGraph REQUIRED COMPONENTS osg osgDB osgViewer osgUtil osgGA osgText OpenThreads)
find_package(wydb)
find_package(wyap)

# 应用框架共享库(app framework shared library): YI3D.exe 与插件共同链接
# Application framework shared library: linked by both YI3D.exe and plugins
file(GLOB_RECURSE APP_SRCS ./*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/resources/wy3dApp.qrc)
list(APPEND APP_SRCS "${CMAKE_SOURCE_DIR}/src/common/osg/OsgUtils.cpp")
list(APPEND APP_SRCS "${CMAKE_SOURCE_DIR}/src/common/osg/OsgSelectUtils.cpp")
list(APPEND APP_SRCS "${CMAKE_SOURCE_DIR}/3rdParty/base64/base64.cpp")

add_library(wy3dApp SHARED ${APP_SRCS})
target_compile_definitions(wy3dApp PRIVATE WY3DAPP_LIBRARY)
# Explicit exports only: classes used by YI3D.exe or plugins carry WY3DAPP_EXPORT

target_include_directories(wy3dApp PUBLIC
    "${CMAKE_SOURCE_DIR}/src/common/osg"
    "${CMAKE_SOURCE_DIR}/src/common/wyap"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/3rdParty/bundles/include"
    "${CMAKE_SOURCE_DIR}/inc"
    "${CMAKE_SOURCE_DIR}/3rdParty"
    "${CMAKE_SOURCE_DIR}/3rdParty/osgQt/include"
    "${CMAKE_SOURCE_DIR}/src"
)
if(UNIX)
    # osgQOpenGL generate_export_header output
    target_include_directories(wy3dApp PUBLIC "${osgQt_BINARY_DIR}/src")
endif()

# link wycad libraries
target_link_libraries(wy3dApp PUBLIC wydb wyap)
target_link_libraries(wy3dApp PUBLIC wy3d)
# link opencascade libraries
find_package(OpenCASCADE REQUIRED)
target_link_libraries(wy3dApp PUBLIC TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep
    TKGeomAlgo TKTopAlgo TKPrim TKBO TKBool TKFeat TKFillet TKOffset
    TKHLR TKMesh TKSTEP TKXSBase TKIGES)
# link qt libraries
target_link_libraries(wy3dApp PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL Qt5::OpenGLExtensions Qt5::Network)
# link osg libraries
if(WIN32)
    target_link_libraries(wy3dApp PUBLIC
        OpenSceneGraph::OpenThreads
        OpenSceneGraph::osg
        OpenSceneGraph::osgDB
        OpenSceneGraph::osgViewer
        OpenSceneGraph::osgUtil
        OpenSceneGraph::osgGA
        OpenSceneGraph::osgText
        OpenSceneGraph::osgQOpenGL
    )
else()
    target_link_libraries(wy3dApp PUBLIC ${OPENSCENEGRAPH_LIBRARIES})
    target_include_directories(wy3dApp PUBLIC ${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(wy3dApp PUBLIC osgQOpenGL)
endif()
# link muparser libraries
target_link_libraries(wy3dApp PUBLIC muparser)
# link dl library (dlopen/dlsym/dlclose) - required on Linux for PythonScriptExecutor
if(NOT WIN32)
    target_link_libraries(wy3dApp PUBLIC ${CMAKE_DL_LIBS})
    target_compile_definitions(wy3dApp PRIVATE PYTHON_LIBRARY_NAME="${PYTHON3_LIB_NAME}")
endif()

# 多语言
set(TS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/wy3dApp")
set(TS_FILES "zh-CN.ts" "en-US.ts")
find_program(LUPDATE_EXECUTABLE lupdate)
find_program(LRELEASE_EXECUTABLE lrelease)
foreach(TSFILE ${TS_FILES})
	execute_process(COMMAND ${LUPDATE_EXECUTABLE} ${APP_SRCS} -ts "${TS_DIR}/${TSFILE}")
	execute_process(COMMAND ${LRELEASE_EXECUTABLE} "${TS_DIR}/${TSFILE}")
endforeach()

# Hand-maintained supplement for the Qt contexts missing from the official zh_CN catalogue:
# compile only, no lupdate (there is no source code to scan)
execute_process(COMMAND ${LRELEASE_EXECUTABLE} "${TS_DIR}/qt-supplement-zh-CN.ts")

# Qt's official zh_CN catalogue lives in the Qt bundle (3rdParty is not tracked by git).
# Where it is absent (system Qt) embed an empty placeholder so the qrc still compiles.
set(QT_ZH_CN_QM_SRC "${CMAKE_SOURCE_DIR}/3rdParty/bundles/qt5/translations/qt_zh_CN.qm")
set(QT_ZH_CN_QM "${TS_DIR}/qt_zh_CN.qm")
if(EXISTS "${QT_ZH_CN_QM_SRC}")
	file(COPY "${QT_ZH_CN_QM_SRC}" DESTINATION "${TS_DIR}")
else()
	execute_process(COMMAND ${LRELEASE_EXECUTABLE} -qm "${QT_ZH_CN_QM}" "${TS_DIR}/qt-empty.ts")
endif()

# 拷贝配置项
#file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/config.ini" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
