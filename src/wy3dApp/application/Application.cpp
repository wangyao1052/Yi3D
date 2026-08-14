///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <cassert>
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#include "application/Application.h"
#include "application/AutoSave.h"
#include "OsgUtils.h"
#include <wyVector3.h>
#include <wyapSceneManager.h>
#include "select/SelectHandler.h"

#include <osgDB/ReadFile>
#include <osgGA/TrackballManipulator>

#include <wyrxClassInfo.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wy3dFeature.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapCmdStack.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <wyapCmdManager.h>
#include <wyapViewManager.h>
#include <wyapViewManager.h>

#include <wy3dDatabase.h>

#include <wyapCmdStack.h>
#include "view/OsgView.h"
#include "scene/Scene.h"
#include "commands/OsgGuiEventDispatcher.h"
#include "Config.h"
#include "snap/SnapObjectFactory.h"
#include "snap/SnapObject.h"
#include "snap/SnapSystemImpl.h"
#include "view/ViewUtil.h"
#include "commands/UndoRedoCommands.h"
#include "commands/CommandNames.h"
#include "environments/sketch/SketchEnvironment.h"
#include "environments/modeling/ModelingEnvironment.h"
#include "gizmo/GizmoFactory.h"
#include "widgets/CursorCenter.h"
#include "utils/StringUtil.h"
#include "application/wy3dQApplication.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/frame/ViewWidgetContainer.h"
#include "widgets/frame/ViewWidget.h"
#include "widgets/frame/OsgViewWidget.h"
#include "application/Document.h"
#include "environments/gateway/GatewayEnvironment.h"
#include "environments/modeling/ModelingEnvironment.h"

#define WY3DAPP_VIEW(pView) dynamic_cast<BaseView*>(pView)
#define WY3DAPP_SCENE(pScene) dynamic_cast<Scene*>(pScene)

extern void wy3dappRegisterCommands();

#if defined(_WIN32)
#ifndef NDEBUG
#define WYAP_LIB "wyapd.dll"
#else
#define WYAP_LIB "wyap.dll"
#endif
#elif defined(__APPLE__)
#ifndef NDEBUG
#define WYAP_LIB "libwyapd.dylib"
#else
#define WYAP_LIB "libwyap.dylib"
#endif
#else
#ifndef NDEBUG
#define WYAP_LIB "libwyapd.so"
#else
#define WYAP_LIB "libwyap.so"
#endif
#endif

typedef bool (*wyapInitializeProcPtr)(wyap::Application*);
typedef bool (*wyapTerminateProcPtr)(wyap::Application*);

Application& Application::instance()
{
    static Application instance;
    return instance;
}

Application::Application() :
    _pMainWindow(nullptr), _pDockPanelManager(nullptr),
    _pConfig(new Config())
{}

Application::~Application()
{
    if (_pConfig)
    {
        delete _pConfig;
        _pConfig = nullptr;
    }
}

bool Application::initialize()
{
    // 配置项初始化
    assert(_pConfig);
    _pConfig->initialize();

    // 初始化
    // <1>文档管理器
    // <2>选择集管理器
    // <3>GIZMO管理器
    // <4>捕捉系统
    // <5>命令堆栈
    // <6>命令管理器
    // <7>视图管理器
    // <8>场景管理器
    // <9>粘贴板
    if (!this->wyapInitialize())
    {
        return false;
    }

    // 创建捕捉系统（原在 wyap 内部，现迁移到 yi3d）
    _pSnapSystem = std::make_unique<wyap::SnapSystemImpl>();

    // 注册为文档管理器反应器（接收文档激活/失活回调）
    this->getDocManager()->addReactor(static_cast<wyap::SnapSystemImpl*>(_pSnapSystem.get()));

    // Auto save module (registers as a document manager reactor)
    _pAutoSave = std::make_unique<AutoSave>();
    _pAutoSave->initialize();

    // 设置捕捉对象工厂
    wyap::SnapSystem* pSnapSys = this->getSnapSystem();
    assert(pSnapSys);
    pSnapSys->setSnapObjectFactory(std::make_shared<SnapObjectFactory>());

    // 设置常驻捕捉对象
    pSnapSys->beginChange();
    {
        // 坐标原点
        wyap::SnapObjectSPtr pSnapObject = std::make_shared<SnapCoordinatePoint>(wy::Vector3(0.0, 0.0, 0.0));
        pSnapSys->addResidentSnapObject(pSnapObject);
    }
    pSnapSys->endChange();

    // Gizmo创建工厂
    _pGizmoFactory = std::make_unique<GizmoFactory>();

    return true;
}

void Application::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    wyap::Application::onDocumentActivated(pActivatedDoc);

    if (_pSnapSystem)
    {
        Scene* pScene = static_cast<Scene*>(getSceneManager()->getScene(pActivatedDoc));
        if (pScene)
        {
            _pSnapSystem->addReactor(pScene);
        }
    }
}

void Application::onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate)
{
    wyap::Application::onDocumentToBeDeactivated(pDocToDeactivate);

    if (_pSnapSystem)
    {
        Scene* pScene = static_cast<Scene*>(getSceneManager()->getScene(pDocToDeactivate));
        if (pScene)
        {
            _pSnapSystem->removeReactor(pScene);
        }
    }
}

QToolBar* Application::getToolBar(const std::string& toolBarName) const
{
    if (!_pMainWindow) return nullptr;
    return _pMainWindow->findChild<QToolBar*>(toolBarName.c_str());
}

void Application::setDockPanelManager(DockPanelManager* pDockPanelManager)
{
    _pDockPanelManager = pDockPanelManager;
}

wydb::Database* Application::newDatabase()
{
    return new wy3d::Database();
}

wyap::Document* Application::newDocument()
{
    return new Document();
}

wyap::Scene* Application::newScene(wyap::Document* pDoc)
{
    return new Scene(pDoc);
}

wyap::View* Application::newView(wyap::Document* pDoc)
{
    assert(pDoc);
    MainWindow* pMainWindow = this->getMainWindow();
    assert(pMainWindow);
    ViewWidgetContainer* pViewWidgetContainer = pMainWindow->getViewWidgetContainer();
    assert(pViewWidgetContainer);
    OsgViewWidget* pOsgViewWidget = new OsgViewWidget(pViewWidgetContainer);
    int index = pViewWidgetContainer->addPage(pDoc, pOsgViewWidget);
    assert(index >= 0);
    osgViewer::View* pOsgView = pOsgViewWidget->getOsgView();
    if (pOsgView)
    {
        return new OsgView(pDoc, pOsgView);
    }
    else
    {
        assert(false);
        return nullptr;
    }
}

std::unique_ptr<wyap::GatewayEnvironment> Application::newDefaultGatewayEnvironment()
{
    GatewayEnvironment* pGatewayEnv = new GatewayEnvironment();
    return std::unique_ptr<wyap::GatewayEnvironment>(pGatewayEnv);
}

std::unique_ptr<wyap::DocumentEnvironment> Application::newDefaultDocumentEnvironment()
{
    ModelingEnvironment* pModelingEnv = new ModelingEnvironment();
    return std::unique_ptr<wyap::DocumentEnvironment>(pModelingEnv);
}

BaseView* Application::getActiveView() const
{
    wyap::Document* pActiveDoc = this->getActiveDocument();
    if (!pActiveDoc)
    {
        return nullptr;
    }
    wyap::View* pView = this->getViewManager()->getView(pActiveDoc);
    return WY3DAPP_VIEW(pView);
}

Scene* Application::getActiveScene() const
{
    wyap::Document* pActiveDoc = this->getActiveDocument();
    if (!pActiveDoc)
    {
        return nullptr;
    }
    wyap::Scene* pScene = this->getSceneManager()->getScene(pActiveDoc);
    return WY3DAPP_SCENE(pScene);
}

void Application::fitView()
{
    BaseView* pActiveView = this->getActiveView();
    if (!pActiveView) return;
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;
    osg::BoundingSphere bsSphere = pScene->getElementsBoundingBox();
    pActiveView->viewAll(bsSphere);
}

void Application::viewISO()
{
    BaseView* pView = this->getActiveView();
    if (!pView) return;
    pView->lookAtISO();
}

void Application::setCursor(CursorType cursorType)
{
    wyap::Document* pDoc = this->getActiveDocument();
    if (!pDoc)
    {
        assert(false);
        return;
    }
    if (_pMainWindow)
    {
        if (ViewWidgetContainer* pLayoutWindow = _pMainWindow->getViewWidgetContainer())
        {
            ViewWidget* pViewWidget = pLayoutWindow->getViewWidget(pDoc);
            if (pViewWidget)
            {
                pViewWidget->setCursor(CursorCenter::instance().getCursor(cursorType));
            }
        }
    }
}

// 通过wyap初始化<1>文档管理器<2>选择集管理器<3>命令堆栈<4>命令管理器
bool Application::wyapInitialize()
{
#if defined(_WIN32)
    // wyap.dll
    HMODULE hDllWYAP = GetModuleHandleA(WYAP_LIB);
    if (!hDllWYAP)
    {
        assert(false);
        return false;
    }

    // wyapInitialize
    wyapInitializeProcPtr pInitializeProc = (wyapInitializeProcPtr)::GetProcAddress(hDllWYAP, "wyapInitialize");
    if (!pInitializeProc)
    {
        assert(false);
        return false;
    }
    if (!pInitializeProc(this))
    {
        assert(false);
        return false;
    }
    return true;
#elif defined(__linux__) || defined(__APPLE__)
    // libwyap.so / libwyap.dylib (query already loaded module, align with GetModuleHandle semantics)
    void* hDllWYAP = dlopen(WYAP_LIB, RTLD_NOW | RTLD_NOLOAD);
    if (!hDllWYAP)
    {
        assert(false);
        return false;
    }

    // wyapInitialize
    wyapInitializeProcPtr pInitializeProc = reinterpret_cast<wyapInitializeProcPtr>(dlsym(hDllWYAP, "wyapInitialize"));
    if (!pInitializeProc)
    {
        assert(false);
        return false;
    }
    if (!pInitializeProc(this))
    {
        assert(false);
        return false;
    }
    return true;
#else
    assert(false);
    return false;
#endif
}

bool Application::wyapTerminate()
{
#if defined(_WIN32)
    // wyap.dll
    HMODULE hDllWYAP = GetModuleHandleA(WYAP_LIB);
    if (!hDllWYAP)
    {
        assert(false);
        return false;
    }

    // wyapTerminate
    wyapTerminateProcPtr pTerminateProc = (wyapTerminateProcPtr)::GetProcAddress(hDllWYAP, "wyapTerminate");
    if (!pTerminateProc)
    {
        assert(false);
        return false;
    }
    if (!pTerminateProc(this))
    {
        assert(false);
        return false;
    }
    return true;
#elif defined(__linux__) || defined(__APPLE__)
    // libwyap.so / libwyap.dylib (query already loaded module, align with GetModuleHandle semantics)
    void* hDllWYAP = dlopen(WYAP_LIB, RTLD_NOW | RTLD_NOLOAD);
    if (!hDllWYAP)
    {
        assert(false);
        return false;
    }

    // wyapTerminate
    wyapTerminateProcPtr pTerminateProc = reinterpret_cast<wyapTerminateProcPtr>(dlsym(hDllWYAP, "wyapTerminate"));
    if (!pTerminateProc)
    {
        assert(false);
        return false;
    }
    if (!pTerminateProc(this))
    {
        assert(false);
        return false;
    }
    return true;
#else
    assert(false);
    return false;
#endif
}
