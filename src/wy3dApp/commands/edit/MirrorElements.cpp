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

#include "MirrorElements.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dMirror.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"

MirrorElemens::MirrorElemens(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd)
{
    _pMatrixTransform = new osg::MatrixTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_pMatrixTransform);
    }
}

MirrorElemens::~MirrorElemens()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_pMatrixTransform);
    }
}

bool MirrorElemens::init(const wyap::SelectionSet& ss)
{
    assert(_pDb);

    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        ids.insert(iter.current().getElementId());
    }
    if (ids.empty()) return false;

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return false;

    for (const wydb::ElementId& id : ids)
    {
        ElementNode* pElemNode = pActiveScene->getElementNode(id);
        if (!pElemNode)
        {
            assert(false);
            continue;
        }
        osg::Group* pElemOsgRoot = pElemNode->getOsgNode();
        if (!pElemOsgRoot) continue;
        osg::ref_ptr<osg::Group> copy = new osg::Group(*pElemOsgRoot); // 浅拷贝
        copy->setNodeMask(~PICK_MASK); // added by wangyao 2025.08.30 不可PICK
        _pMatrixTransform->addChild(copy);
    }

    return true;
}

static osg::Matrix createMirrorMatrix(const osg::Vec3d& planeOrigin, const osg::Vec3d& planeNormal)
{
    // 1.计算平移矩阵T:将平面平移到原点
    osg::Matrix T = osg::Matrix::translate(-planeOrigin);

    // 2.计算旋转矩阵R:将平面法向量旋转到与Z轴对齐
    osg::Matrix R = osg::Matrix::rotate(planeNormal, osg::Vec3d(0.0, 0.0, 1.0));

    // 3.计算镜像矩阵:关于XOY平面镜像
    osg::Matrix M = osg::Matrix::identity();
    M(2, 2) = -1.0;

    // 4.计算逆旋转矩阵inverseR:将平面旋转回原来的方向
    osg::Matrix inverseR = osg::Matrix::rotate(osg::Vec3d(0.0, 0.0, 1.0), planeNormal);

    // 5.计算逆平移矩阵inverseT:将平面平移回原来的位置
    osg::Matrix inverseT = osg::Matrix::translate(planeOrigin);

    // 组合变换
    return T * R * M * inverseR * inverseT;
}

bool MirrorElemens::update(const wy3d::SketchPlane& sketchPlane, const wy::Vector2& axisStartPnt, const wy::Vector2& aixsEndPnt)
{
    if (!sketchPlane.isValid())
    {
        return false;
    }
    wy::Vector3 normal = sketchPlane.getNormal();
    wy::Vector3 startPnt = sketchPlane.value(axisStartPnt);
    wy::Vector3 endPnt = sketchPlane.value(aixsEndPnt);
    wy::Vector3 axisDir = endPnt - startPnt;
    axisDir.normalize();
    if (axisDir.length() < 0.1)
    {
        return false;
    }
    wy::Vector3 mirrorPlaneNormal = axisDir.cross(normal);
    osg::Matrix matrix = createMirrorMatrix(MathUtils::toVec3d(startPnt), MathUtils::toVec3d(mirrorPlaneNormal));
    _pMatrixTransform->setMatrix(matrix);

    return true;
}

bool MirrorElemens::perform(const wyap::SelectionSet& ss, const wydb::ElementId& sketchId, const wy::Vector2& axisStartPnt, const wy::Vector2& aixsEndPnt)
{
    if (!_pDb)
    {
        assert(false);
        return false;
    }

    // 过滤元素
    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current().getElementId();
        ids.insert(id);
    }
    if (ids.empty()) return false;

    std::vector<wydb::ElementId> elemIds;
    for (const wydb::ElementId& id : ids)
    {
        const wydb::Element* pElem = _pDb->getElement(id);
        if (!pElem) continue;
        const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
        if (!pSketchEntity) continue;
        elemIds.push_back(id);
    }
    if (elemIds.empty()) return false;

    // 复制元素
    std::shared_ptr<wyap::ElementsClipData> pClipData = wyap::Clipboard::newElementsClipData(_pDb, elemIds);
    if (!pClipData) return false;

    // 镜像元素
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    std::vector<wydb::Element*> copyedElems;
    if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTrans, *pClipData, copyedElems))
    {
        assert(copyedElems.empty());
        return false;
    }
    if (copyedElems.empty())
    {
        assert(false);
        return false;
    }
    wy3d::Matrix3 mirrorMatrix;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch) goto ABORT_TRANS;

    mirrorMatrix = wy3d::Matrix3::createReflection2D(axisStartPnt, aixsEndPnt);
    for (wydb::Element* pCopyElem : copyedElems)
    {
        if (!pCopyElem)
        {
            assert(false);
            continue;
        }
        wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pCopyElem);
        if (!pSketchEntity) continue;
        if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchEntity)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pSketchEntity->transform(mirrorMatrix)) goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    for (wydb::Element* pCopyElem : copyedElems)
    {
        if (pCopyElem) wydb::deleteElement(pCopyElem);
    }
    return false;
}
