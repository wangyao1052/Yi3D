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

#ifndef WY3DAPP_SKETCH_RECTANGLE_GIZMO_CREATOR_H
#define WY3DAPP_SKETCH_RECTANGLE_GIZMO_CREATOR_H

#include <list>

#include <wyapGizmo.h>
#include <wyapSelection.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 选择集级创建器：恰好 4 条直线且构成矩形时，创建矩形专用 Gizmo
class SketchRectangleGizmoCreator
{
public:
    // 识别成功则把 10 个 Gizmo（9 个手柄 + 1 个对角线 Ghost）追加到 gizmos 并返回 true；
    // 否则不修改 gizmos，返回 false，由调用方回退逐线 grip。
    static bool tryCreateGizmos(const wyap::SelectionSet& sels,
                                std::list<wyap::GizmoSPtr>& gizmos);
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_GIZMO_CREATOR_H
