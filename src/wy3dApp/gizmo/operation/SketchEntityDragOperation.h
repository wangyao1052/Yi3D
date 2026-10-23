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

#ifndef WY3DAPP_SKETCH_ENTITY_DRAG_OPERATION_H
#define WY3DAPP_SKETCH_ENTITY_DRAG_OPERATION_H

#include <memory>
#include <vector>

#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>

#include "GizmoOperation.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchEntityGripGizmo;

class SketchEntityDragOperation : public GizmoOperation
{
public:
    SketchEntityDragOperation(wydb::Database* pDb, SketchEntityGripGizmo* pGizmo);
    ~SketchEntityDragOperation();

    // 开始
    virtual bool onBeginDrag(const wy::Vector3& startPos) override;
    // 拖动中
    virtual bool onDragging(const wy::Vector3& currentPos) override;
    // 结束
    virtual void onEndDrag(const wy::Vector3& endPos) override;
    // 取消
    virtual void onCancelDrag() override;

private:
    void collectSynchronizedGrips();

private:
    SketchEntityGripGizmo* _pGripGizmo;
    std::vector<std::weak_ptr<SketchEntityGripGizmo>> _synchronizedGrips;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_ENTITY_DRAG_OPERATION_H
