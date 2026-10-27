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

#include <wy3dApp/Gizmo.h>
#include <wy3dApp/KeyCode.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

Gizmo::~Gizmo()
{
    // 基类析构时派生类已销毁，不能回调 operation 的派生钩子。
    // 这里只做 renderer 的视觉清理。
    if (_pGizmoOp && _pRenderer)
        _pRenderer->onCancelDrag();
}

void Gizmo::onActivate()
{
    if (_pRenderer) _pRenderer->onActivate();
}

void Gizmo::onKeyDown(const KeyEvent& event)
{
    if (event.key == KeyCode::Escape)
        this->onEscapeKey();
}

void Gizmo::onDeactivate()
{
    if (_pRenderer) _pRenderer->onDeactivate();
    // 先移出成员：回调期间重入 onDeactivate 不会二次取消同一个 operation。
    auto pOp = std::move(_pGizmoOp);
    if (!pOp) return;
    pOp->onCancelDrag();
    if (_pRenderer) _pRenderer->onCancelDrag();
}

} // namespace wy3dApp
