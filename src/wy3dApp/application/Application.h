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

#ifndef WY3DAPP_APPLICATION_H
#define WY3DAPP_APPLICATION_H

#include <memory>
#include <string>
#include <set>
#include <deque>

#include <QObject>
#include <QGLWidget>
#include <QTranslator>
#include <QToolBar>

#include <osg/Node>
#include <osg/Geode>
#include <osg/Group>
#include <osgViewer/View>
#include <wydbElementId.h>
#include <wyapApplication.h>
#include <wyapSceneManager.h>
#include "snap/SnapSystemBase.h"
#include "widgets/StatusBarHelper.h"

namespace wydb
{
    class Database;
}

namespace wyap
{
    class Document;
    class DocManager;
    class SelManager;
    class CmdStack;
    class CmdManager;
    class ViewManager;
    class SceneManager;
}

class View;
class BaseView;
class Scene;

class MainWindow;
class DockPanelManager;
class Config;
class CmdActionMgr;
class GuiCmdMenuActionMgr;
class GizmoFactory;
class AutoSave;

enum class CursorType;

class Application : public QObject, public wyap::Application
{
    Q_OBJECT
private:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

public:
    // 单例
    static Application& instance();
    // 初始化
    bool initialize();

    // 程序配置
    const Config* getConfig() const
    {
        return _pConfig;
    }

    // Auto save module
    AutoSave* getAutoSave() const
    {
        return _pAutoSave.get();
    }

    // 获取当前激活的视图
    BaseView* getActiveView() const;
    // 获取当前激活的场景
    Scene* getActiveScene() const;

    // 获取标题
    const QString& getTitle() const
    {
        return _title;
    }
    // 设置标题
    void setTitle(const QString& title)
    {
        _title = title;
    }

    // 获取主窗口
    MainWindow* getMainWindow() const
    {
        return _pMainWindow;
    }
    // 设置主窗口
    void setMainWindow(MainWindow* pMainWindow)
    {
        _pMainWindow = pMainWindow;
    }

    DockPanelManager* getDockPanelManager() const
    {
        return _pDockPanelManager;
    }
    void setDockPanelManager(DockPanelManager* pDockPanelManager);

    // 获取ToolBar
    QToolBar* getToolBar(const std::string& toolBarName) const;

    // 获取Gizmo创建工厂
    GizmoFactory* getGizmoFactory() const
    {
        return _pGizmoFactory.get();
    }

    // 设置鼠标样式
    void setCursor(CursorType cursorType);

    // 新建数据库
    virtual wydb::Database* newDatabase() override;
    // 新建文档
    virtual wyap::Document* newDocument() override;

    // 新建场景
    virtual wyap::Scene* newScene(wyap::Document* pDoc) override;
    // 新建视图
    virtual wyap::View* newView(wyap::Document* pDoc) override;
    // 新建默认入口环境
    virtual std::unique_ptr<wyap::GatewayEnvironment> newDefaultGatewayEnvironment() override;
    // 新建默认文档环境
    virtual std::unique_ptr<wyap::DocumentEnvironment> newDefaultDocumentEnvironment() override;

    // 状态栏帮助类
    StatusBarHelper* getStatusBar() { return &_statusBar; }

    // 获取捕捉系统
    wyap::SnapSystem* getSnapSystem() const { return _pSnapSystem.get(); }

    // 文档激活/失活回调（管理 Scene 的 SnapSystem 注册）
    virtual void onDocumentActivated(wyap::Document* pActivatedDoc) override;
    virtual void onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate) override;

    void fitView();
    void viewISO();

protected:
    // wyap初始化
    // <1>文档管理器
    // <2>选择集管理器
    // <3>命令堆栈
    // <4>命令管理器
    // <5>视图管理器
    // <6>场景管理器
    bool wyapInitialize();

    // wyap退出(目前没有调用)
    bool wyapTerminate();

private:
    // 程序标题
    QString _title;
    // 主窗口
    MainWindow* _pMainWindow;
    // 停靠面板管理器
    DockPanelManager* _pDockPanelManager;
    // 状态栏帮助类
    StatusBarHelper _statusBar;
    // 配置项
    Config* _pConfig;
    // Auto save module
    std::unique_ptr<AutoSave> _pAutoSave;
    // Gizmo创建工厂
    std::unique_ptr<GizmoFactory> _pGizmoFactory;
    // 捕捉系统
    std::unique_ptr<wyap::SnapSystem> _pSnapSystem;
    // 选择过滤字符串
    std::string _selectFilterString;
    std::vector<std::string> _selFilterTokens;

    friend class MainWindow;

private:
    bool _idleRequested = false;
};

#endif // WY3DAPP_APPLICATION_H
