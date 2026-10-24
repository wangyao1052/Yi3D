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

#ifndef WY3DAPP_SOLID_DRAG_OPERATION_H
#define WY3DAPP_SOLID_DRAG_OPERATION_H

#include <wy3dApp/GizmoOperation.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class BaseGizmo;

// 移动拖拽操作 — 仅负责事务管理（数据修改），渲染由 SolidMoveOsgRenderer 处理
class SolidDragOperation : public GizmoOperation
{
public:
    explicit SolidDragOperation(wydb::Database* pDb, BaseGizmo* pGizmo);
    ~SolidDragOperation();

    // 启动
    virtual bool onBeginDrag(const wy::Vector3& startPos) override;
    // 进行中（SolidMove 的数据修改在 end 时一次性完成）
    virtual bool onDragging(const wy::Vector3& curPos) override;
    // 结束
    virtual void onEndDrag(const wy::Vector3& endPos) override;
    // 取消
    virtual void onCancelDrag() override;

private:
    bool perform();

private:
    // 拖拽起点
    wy::Vector3 _dragStartPnt;
    // 拖拽终点
    wy::Vector3 _dragEndPnt;
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_DRAG_OPERATION_H
