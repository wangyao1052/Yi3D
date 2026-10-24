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

#include <wy3dApp/BaseGizmo.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

BaseGizmo::~BaseGizmo()
{
    if (_pGizmoOp && _pRenderer)
        _pRenderer->onCancelDrag();
}

void BaseGizmo::onActivate()
{
    if (_pRenderer) _pRenderer->onActivate();
}

void BaseGizmo::onDeactivate()
{
    if (_pRenderer) _pRenderer->onDeactivate();
    if (_pGizmoOp)
    {
        _pGizmoOp->onCancelDrag();
        if (_pRenderer) _pRenderer->onCancelDrag();
        _pGizmoOp = nullptr;
    }
}

} // namespace wy3dApp
