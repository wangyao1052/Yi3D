cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "wy3dApp")
set(APPLICATION_NAME "YI3D")
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
# main.cpp 只属于可执行程序
list(REMOVE_ITEM APP_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

add_library(wy3dApp SHARED ${APP_SRCS})
target_compile_definitions(wy3dApp PRIVATE WY3DAPP_LIBRARY)
# 自动导出全部符号, 保证薄壳 exe 与插件能链接到框架类(插件 SDK 面的类再显式加 WY3DAPP_EXPORT)
set_target_properties(wy3dApp PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

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

# 可执行程序: 薄壳(main.cpp + 图标资源)
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

# 编译后自动拷贝运行时依赖 DLL（仅 Windows）
if(WIN32)
    add_custom_command(TARGET ${APPLICATION_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_FILE=$<TARGET_FILE:${APPLICATION_NAME}>
            -DOUT_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            -DSEARCH_DIRS="${CMAKE_SOURCE_DIR}/3rdParty/bundles"
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
    )
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
