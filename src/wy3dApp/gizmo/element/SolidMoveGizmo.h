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

#ifndef WY3DAPP_SOLID_MOVE_GIZMO_H
#define WY3DAPP_SOLID_MOVE_GIZMO_H

#include <osg/LineSegment>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wy3dPrimitive.h>

#include <wy3dApp/DraggableGizmo.h>
#include <wy3dApp/OsgGizmoNode.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 单轴移动 Gizmo 基类
class SolidMoveGizmo : public DraggableGizmo
{
public:
    virtual ~SolidMoveGizmo();

    // 设置兄弟 Gizmo（拖拽时同步位置）
    void setSiblings(SolidMoveGizmo* pSiblingA, SolidMoveGizmo* pSiblingB);

protected:
    DraggableGizmo::StepResult onStartDrag(wydb::Database* pDb, const MouseEvent& event) override;
    DraggableGizmo::StepResult onDragging(wydb::Database* pDb, const MouseEvent& event) override;
    DraggableGizmo::StepResult onEndDrag(wydb::Database* pDb, const MouseEvent& event) override;
    void onDragMoved() override;
    void onDragCancelled() override;

    SolidMoveGizmo(const wy3d::Primitive* pPrimitive, const osg::Vec4& color, const osg::Quat& axisRot);

private:
    // 计算最近点
    bool computeClosestPoint(osg::Camera* camera, float winX, float winY, osg::Vec3d& outPnt);
    // 获取世界坐标系下的轴线段
    osg::ref_ptr<osg::LineSegment> getAxisLineSegment() const;

    // 同步位置到兄弟 Gizmo
    void syncPosition();

    // 本次拖拽的起始点
    wy::Vector3 _startPos;

protected:
    // 兄弟 Gizmo（同 Primitive 的另外两个轴）
    SolidMoveGizmo* _siblings[2]{nullptr, nullptr};
};

// X轴移动 Gizmo（红色）
class SolidMoveXGizmo : public SolidMoveGizmo
{
public:
    explicit SolidMoveXGizmo(const wy3d::Primitive* pPrimitive);
};

// Y轴移动 Gizmo（绿色）
class SolidMoveYGizmo : public SolidMoveGizmo
{
public:
    explicit SolidMoveYGizmo(const wy3d::Primitive* pPrimitive);
};

// Z轴移动 Gizmo（蓝色）
class SolidMoveZGizmo : public SolidMoveGizmo
{
public:
    explicit SolidMoveZGizmo(const wy3d::Primitive* pPrimitive);
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_MOVE_GIZMO_H
