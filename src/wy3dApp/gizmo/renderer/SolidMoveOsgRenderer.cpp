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

#include "SolidMoveOsgRenderer.h"
#include <cassert>

#include <osg/Node>
#include <osg/AutoTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Shape>
#include <osg/ShapeDrawable>

#include <gp_Quaternion.hxx>

#include <wyVector3.h>

#include "gizmo/BaseGizmo.h"
#include <wy3dApp/Application.h>
#include <wy3dPrimitive.h>
#include <wy3dApp/Scene.h>
#include "scene/RenderConst.h"
#include <wy3dApp/ElementNode.h>
#include "scene/RenderConst.h"
#include "wy3dAppDefs.h"

#define RADIUS 2.0
#define LEN1 50.0
#define LEN2 20.0

// ── 辅助函数 ──

namespace wy3dApp {

namespace
{
    void computeNodePathToRoot(osg::Node& node, osg::NodePath& np)
    {
        np.clear();
        osg::NodePathList nodePaths = node.getParentalNodePaths();
        if (!nodePaths.empty())
        {
            np = nodePaths.front();
            if (nodePaths.size() > 1)
            {
                assert(false);
            }
        }
    }
}

// ── SolidMoveOsgRenderer ──

SolidMoveOsgRenderer::SolidMoveOsgRenderer(BaseGizmo* pGizmo, const osg::Vec4& color, const osg::Quat& axisRot)
    : OsgGizmoRenderer(pGizmo), _color(color)
{
    this->generateGeometry(color, axisRot);
    _previewGroup = new osg::Group();
    _previewGroup->setNodeMask(~PICK_MASK);
    this->refresh();
}

SolidMoveOsgRenderer::~SolidMoveOsgRenderer()
{
}

void SolidMoveOsgRenderer::attachToScene(Scene* pScene)
{
    OsgGizmoRenderer::attachToScene(pScene);
    pScene->getRoot()->addChild(_previewGroup);
}

void SolidMoveOsgRenderer::detachFromScene(Scene* pScene)
{
    pScene->getRoot()->removeChild(_previewGroup);
    OsgGizmoRenderer::detachFromScene(pScene);
}

void SolidMoveOsgRenderer::generateGeometry(const osg::Vec4& color, const osg::Quat& axisRot)
{
    osg::ref_ptr<osg::AutoTransform> at = new osg::AutoTransform();
    this->getOsgNode()->addChild(at.get());
    at->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);
    at->setAutoScaleToScreen(true);

    double radius = RADIUS;
    double len1 = LEN1;
    double len2 = LEN2;

    // axis
    osg::ref_ptr<osg::PositionAttitudeTransform> axis = new osg::PositionAttitudeTransform();
    at->addChild(axis.get());
    axis->setAttitude(axisRot);
    {
        osg::ref_ptr<osg::ShapeDrawable> cylinderGeode = new osg::ShapeDrawable();
        osg::ref_ptr<osg::Cylinder> cylinder = new osg::Cylinder(osg::Vec3(0, 0, len1 / 2), radius, len1);
        cylinderGeode->setShape(cylinder.get());
        cylinderGeode->setColor(color);

        osg::ref_ptr<osg::ShapeDrawable> coneGeode = new osg::ShapeDrawable();
        osg::ref_ptr<osg::Cone> cone = new osg::Cone(osg::Vec3(0, 0, 0), radius * 3, len2);
        cone->setCenter(osg::Vec3(0, 0, -cone->getBaseOffset() + len1));
        coneGeode->setShape(cone.get());
        coneGeode->setColor(color);

        axis->addChild(cylinderGeode.get());
        axis->addChild(coneGeode.get());

        _cylinder = cylinderGeode;
        _cone = coneGeode;
    }
}

void SolidMoveOsgRenderer::onActivate()
{
    _cylinder->setColor(OsgGizmoNode::YELLOW_COLOR);
    _cone->setColor(OsgGizmoNode::YELLOW_COLOR);
}

void SolidMoveOsgRenderer::onDeactivate()
{
    _cylinder->setColor(_color);
    _cone->setColor(_color);
}

void SolidMoveOsgRenderer::syncPosition(SolidMoveOsgRenderer* pOther)
{
    if (pOther)
        pOther->getOsgNode()->setPosition(this->getOsgNode()->getPosition());
}

osg::ref_ptr<osg::LineSegment> SolidMoveOsgRenderer::getAxisLineSegment() const
{
    static osg::Vec3d localSPnt(0.0, 0.0, 0.0);
    static osg::Vec3d localEPnt(0.0, 0.0, LEN1 + LEN2);

    osg::Node* pNode = _cylinder.get();
    if (!pNode) return nullptr;

    osg::NodePath nodePathToRoot;
    computeNodePathToRoot(*pNode, nodePathToRoot);
    osg::Matrix localToWorld = osg::computeLocalToWorld(nodePathToRoot);
    osg::Vec3d sPnt = localSPnt * localToWorld;
    osg::Vec3d ePnt = localEPnt * localToWorld;

    return new osg::LineSegment(sPnt, ePnt);
}

void SolidMoveOsgRenderer::refresh()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wydb::Element* pElem = pDb->getElement(_pGizmo->getModifiedElement());
    if (!pElem) return;
    const wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
    if (!pPrimitive) return;

    wy::Vector3 pos = pPrimitive->getPosition();
    wy::Vector3 rot = pPrimitive->getRotation();

    assert(this->getOsgNode());
    this->getOsgNode()->setPosition(osg::Vec3d(pos.x(), pos.y(), pos.z()));

    gp_Quaternion rotZ;
    rotZ.SetVectorAndAngle(gp_Vec(0.0, 0.0, 1.0), rot.z());
    gp_Quaternion rotX;
    rotX.SetVectorAndAngle(gp_Vec(1.0, 0.0, 0.0), rot.x());
    gp_Quaternion rotY;
    rotY.SetVectorAndAngle(gp_Vec(0.0, 1.0, 0.0), rot.y());
    gp_Quaternion quat = rotY * rotX * rotZ;
    this->getOsgNode()->setAttitude(osg::Quat(quat.X(), quat.Y(), quat.Z(), quat.W()));
}

ElementNode* SolidMoveOsgRenderer::getRelatedElementNode() const
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return nullptr;
    return pActiveScene->getElementNode(_pGizmo->getModifiedElement());
}

// ── 拖拽生命周期 ──

bool SolidMoveOsgRenderer::onBeginDrag(const wy::Vector3& startPos)
{
    // 保存起始位置
    osg::Vec3d gizmoPos = this->getOsgNode()->getPosition();
    _gizmoStartPos.set(gizmoPos.x(), gizmoPos.y(), gizmoPos.z());
    _dragStartPnt = startPos;

    // 创建 ghost 预览
    _ghostPat = new osg::PositionAttitudeTransform();

    ElementNode* pElemNode = this->getRelatedElementNode();
    if (pElemNode && pElemNode->getOsgNode())
    {
        osg::ref_ptr<osg::Group> copy = new osg::Group(*(pElemNode->getOsgNode())); // 浅拷贝
        _ghostPat->addChild(copy);
    }
    _previewGroup->addChild(_ghostPat);

    // 暂时隐藏元素节点
    if (pElemNode && pElemNode->getOsgNode())
    {
        pElemNode->getOsgNode()->setNodeMask(0);
    }

    return true;
}

bool SolidMoveOsgRenderer::onDragging(const wy::Vector3& curPos)
{
    wy::Vector3 delta = curPos - _dragStartPnt;
    this->getOsgNode()->setPosition(
        osg::Vec3d(_gizmoStartPos.x() + delta.x(),
                   _gizmoStartPos.y() + delta.y(),
                   _gizmoStartPos.z() + delta.z()));
    if (_ghostPat.valid())
    {
        _ghostPat->setPosition(osg::Vec3d(delta.x(), delta.y(), delta.z()));
    }
    return true;
}

void SolidMoveOsgRenderer::onEndDrag(const wy::Vector3& endPos)
{
    // 恢复显示元素节点
    ElementNode* pElemNode = this->getRelatedElementNode();
    if (pElemNode) pElemNode->recomputeNodeMask();

    // 清理 ghost
    osg::Group* pElementsRenderNode = _previewGroup;
    if (pElementsRenderNode && _ghostPat.valid())
    {
        pElementsRenderNode->removeChild(_ghostPat);
    }
    _ghostPat = nullptr;
}

void SolidMoveOsgRenderer::onCancelDrag()
{
    // 还原 gizmo 位置
    this->getOsgNode()->setPosition(
        osg::Vec3d(_gizmoStartPos.x(), _gizmoStartPos.y(), _gizmoStartPos.z()));

    // 恢复显示元素节点
    ElementNode* pElemNode = this->getRelatedElementNode();
    if (pElemNode) pElemNode->recomputeNodeMask();

    // 清理 ghost
    osg::Group* pElementsRenderNode = _previewGroup;
    if (pElementsRenderNode && _ghostPat.valid())
    {
        pElementsRenderNode->removeChild(_ghostPat);
    }
    _ghostPat = nullptr;
}

}
