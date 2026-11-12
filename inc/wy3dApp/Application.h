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

#include <QString>

#include <wyapApplication.h>
#include <wy3dApp/wy3dAppDefs.h>

class QMainWindow;
class QWidget;

namespace wy3dApp {

class ElementNodeFactory;
class GizmoFactory;
class ParamDisplayNameRegistry;
class Scene;

enum class CursorType;

class WY3DAPP_EXPORT Application : public wyap::Application
{
protected:
    Application();
    ~Application() override;

public:
    // Sets the status-bar tip; an empty string clears it. UI thread only.
    virtual void setStatusBarTip(const QString& text) = 0;
    virtual void setCursor(CursorType cursorType) = 0;
    // Owned by the application; non-null once the application has started.
    // The caller does not own the returned pointer.
    virtual ParamDisplayNameRegistry* getParamDisplayNameRegistry() const = 0;
    // Owned by the application; non-null once the application has started.
    // The caller does not own the returned pointer.
    virtual GizmoFactory* getGizmoFactory() const = 0;
    // Owned by the application; non-null once the application has initialized.
    // The caller does not own the returned pointer.
    virtual ElementNodeFactory* getElementNodeFactory() const = 0;

    virtual Scene* getActiveScene() const = 0;

    virtual QMainWindow* getMainWindow() const = 0;

    virtual QWidget* getActiveViewWidget() const = 0;
};

// Returns the process-wide Application singleton (implemented by ApplicationImpl).
// Never null; the caller does not own it.
WY3DAPP_EXPORT Application* getApplication();

} // namespace wy3dApp

#endif // WY3DAPP_APPLICATION_H
