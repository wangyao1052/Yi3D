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

#include "CameraCommands.h"
#include <wy3dApp/Application.h>
#include "view/OsgView.h"
#include "view/ViewUtil.h"
#include <wy3dApp/Scene.h>


int OrthoCameraCommand::run()
{
    BaseView* pActiveView = Application::instance().getActiveView();
    if (!pActiveView) return -1;
    pActiveView->ortho();

    return 0;
}


int PerspectiveCameraCommand::run()
{
    BaseView* pActiveView = Application::instance().getActiveView();
    if (pActiveView)
    {
        pActiveView->perspective();
    }

    return 0;
}