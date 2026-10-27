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

#ifndef WY3DAPP_SOLID_MOVE_OSG_RENDERER_H
#define WY3DAPP_SOLID_MOVE_OSG_RENDERER_H

#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/LineSegment>

#include <wy3dApp/OsgGizmoRenderer.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Scene;
class ElementNode;

// SolidMoveGizmo 的 OSG 渲染器
// 负责：几何创建、高亮、ghost 预览、拖拽时 gizmo 位移
class SolidMoveOsgRenderer : public OsgGizmoRenderer
{
public:
    SolidMoveOsgRenderer(Gizmo* pGizmo, const osg::Vec4& color, const osg::Quat& axisRot);
    virtual ~SolidMoveOsgRenderer();

    // ── GizmoRenderer 拖拽生命周期 ──
    virtual bool onBeginDrag(const wy::Vector3& startPos) override;
    virtual bool onDragging(const wy::Vector3& curPos) override;
    virtual void onEndDrag(const wy::Vector3& endPos) override;
    virtual void onCancelDrag() override;

    // ── GizmoRenderer 刷新 ──
    virtual void refresh() override;

    virtual void attachToScene(Scene* pScene) override;
    virtual void detachFromScene(Scene* pScene) override;

    // ── Gizmo 激活/去激活 ──
    virtual void onActivate() override;
    virtual void onDeactivate() override;

    // ── 位置同步（拖拽时兄弟轴跟随）──
    void syncPosition(SolidMoveOsgRenderer* pOther);

    // ── 几何查询（用于最近点计算）──
    osg::ref_ptr<osg::LineSegment> getAxisLineSegment() const;

private:
    void generateGeometry(const osg::Vec4& color, const osg::Quat& axisRot);
    ElementNode* getRelatedElementNode() const;

private:
    osg::Vec4 _color;
    osg::ref_ptr<osg::ShapeDrawable> _cylinder;
    osg::ref_ptr<osg::ShapeDrawable> _cone;

    // ── 拖拽 ghost 状态 ──
    osg::ref_ptr<osg::Group> _previewGroup;
    wy::Vector3 _gizmoStartPos;   // 拖拽开始时 gizmo 节点位置
    wy::Vector3 _dragStartPnt;    // 拖拽起始点（轴上最近点）
    osg::ref_ptr<osg::PositionAttitudeTransform> _ghostPat;
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_MOVE_OSG_RENDERER_H
