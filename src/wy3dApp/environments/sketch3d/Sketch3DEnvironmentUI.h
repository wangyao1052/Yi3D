///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH3D_ENVIRONMENT_UI_H
#define WY3DAPP_SKETCH3D_ENVIRONMENT_UI_H

#include <QPointer>

#include "widgets/frame/ViewportOverlayBar.h"

class Sketch3DEnvironment;

class Sketch3DEnvironmentUI
{
public:
    Sketch3DEnvironmentUI();
    ~Sketch3DEnvironmentUI();

    void initialize(Sketch3DEnvironment* pEnv);
    void teardown(Sketch3DEnvironment* pEnv);

private:
    // End/cancel buttons over the viewport. Its buttons hold command actions, which the
    // environment destroys on teardown, so it has to be dropped before destroyUI().
    // The bar is a child of the view widget, which can be torn down with the document while
    // this object is still alive, so the pointer has to notice the bar going away with it.
    QPointer<ViewportOverlayBar> _pOverlayBar;
};

#endif // WY3DAPP_SKETCH3D_ENVIRONMENT_UI_H
