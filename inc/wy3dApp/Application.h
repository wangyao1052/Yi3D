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
#include <wy3dApp/wy3dAppDefs.h>

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

namespace wy3dApp {

class View;
class BaseView;
class Scene;
class MainWindow;
class DockPanelManager;
class Config;

class GizmoFactory;
class AutoSave;

enum class CursorType;

class WY3DAPP_EXPORT Application : public QObject, public wyap::Application
{
    Q_OBJECT

protected:
    Application();
    ~Application() override;

public:
    // Create the Qt application and main window, load plugins, enter the gateway
    // environment, then run the event loop. Returns the event loop's exit code.
    virtual int runApplication(int argc, char** argv) = 0;

    virtual StatusBarHelper* getStatusBar() = 0;
    virtual MainWindow* getMainWindow() const = 0;
    virtual void setCursor(CursorType cursorType) = 0;
};

// Returns the process-wide Application singleton (implemented by ApplicationImpl).
// Never null; the caller does not own it.
WY3DAPP_EXPORT Application* getApplication();

} // namespace wy3dApp

#endif // WY3DAPP_APPLICATION_H
