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

#include "RotateElements.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <osg/Array>
#include <osg/PrimitiveSet>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <Standard_Failure.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSolid.h>
#include <wy3dPrimitive.h>
#include <wy3dRotate.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"


RotateElements::RotateElements(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd)
{
    _matrixTransform = new osg::MatrixTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_matrixTransform);
    }
}

RotateElements::~RotateElements()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_matrixTransform);
    }
}

bool RotateElements::init(const wyap::SelectionSet& ss,
    const wy::Vector3& centerPnt, const wy::Vector2& centerPnt2,
    const wy::Vector3& axisDir)
{
    _centerPnt = centerPnt;
    _centerPnt2 = centerPnt2;
    _axisDir = axisDir;

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
        copy->setNodeMask(~PICK_MASK); // added by wangyao 2025.08.26 不可PICK
        _matrixTransform->addChild(copy);
    }

    return true;
}

bool RotateElements::update(double rotateAngle)
{
    osg::Vec3d centerPnt(_centerPnt.x(), _centerPnt.y(), _centerPnt.z());
    _matrixTransform->setMatrix(
          osg::Matrix::translate(-centerPnt)
        * osg::Matrix::rotate(rotateAngle, osg::Vec3d(_axisDir.x(), _axisDir.y(), _axisDir.z()))
        * osg::Matrix::translate(centerPnt));

    return true;
}

static void getElementAllLevelsChildren(wydb::Database* pDb, const wydb::Element* pElem, std::list<wydb::ElementId>& allLevelChildren)
{
    std::vector<wydb::ElementId> children = pElem->getChildren();
    for (const wydb::ElementId& childId : children)
    {
        allLevelChildren.emplace_back(childId);
        const wydb::Element* pChildElem = pDb->getElement(childId);
        if (!pChildElem) continue;
        getElementAllLevelsChildren(pDb, pChildElem, allLevelChildren);
    }
}

bool RotateElements::perform(const wyap::SelectionSet& ss, double rotateAngle, GuiCmdEnvType mode)
{
    if (!_pDb)
    {
        assert(false);
        return true;
    }

    // 选择的元素
    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        ids.insert(iter.current().getElementId());
    }
    if (ids.empty()) return true;
    
    // 旋转
    if (GuiCmdEnvType::Sketching == mode)
    {
        return this->performImpl_Sketching(ids, rotateAngle);
    }
    else
    {
        return this->performImpl_Modeling(ids, rotateAngle);
    }
}

bool RotateElements::performImpl_Sketching(const std::set<wydb::ElementId>& ids, double rotateAngle)
{
    assert(_pDb);
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    for (const wydb::ElementId& id : ids)
    {
        wydb::Element* pElem = pTrans->getElementForWrite(id);
        wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
        if (!pSketchEntity) continue;        pSketchEntity->rotateAround(_centerPnt2, rotateAngle);
    }
    _pDb->getTransactionManager()->endTransaction();

    return true;
}

bool RotateElements::performImpl_Modeling(const std::set<wydb::ElementId>& ids, double rotateAngle)
{
    gp_Trsf rotTrsf;
    try
    {
        gp_Ax1 ax1(gp_Pnt(_centerPnt.x(), _centerPnt.y(), _centerPnt.z()), gp_Dir(_axisDir.x(), _axisDir.y(), _axisDir.z()));
        rotTrsf.SetRotation(ax1, rotateAngle);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
    }

    assert(_pDb);
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Cascade;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    for (const wydb::ElementId& id : ids)
    {
        wydb::Element* pElem = pTrans->getElementForWrite(id);
        wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        if (pSolid)
        {
            if (!pSolid->getParent().isNull())
            {
                assert(false);
                continue;
            }

            wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pSolid);
            const std::vector<wydb::ElementId>& modifications = pSolid->getModifications();
            if (pPrimitive && modifications.empty())
            {
                wy::Vector3 pos = pPrimitive->getPosition();
                wy::Vector3 rot = pPrimitive->getRotation();

                // 算出最终的transformation
                gp_Trsf trsf = MathUtils::createTrsf(pos, rot);
                trsf.PreMultiply(rotTrsf);
                const gp_XYZ& retPosXYZ = trsf.TranslationPart();
                wy::Vector3 retEulerAngles = MathUtils::quaternionToEulerZXY(trsf.GetRotation());

                // 赋值
                pPrimitive->setPosition(wy::Vector3(retPosXYZ.X(), retPosXYZ.Y(), retPosXYZ.Z()));
                pPrimitive->setRotation(retEulerAngles);
            }
            else
            {
                wy3d::Rotate* pRotate(nullptr);
                if (wy::ErrorStatus::Ok == wy3d::Rotate::create(pTrans, pSolid, _centerPnt, _axisDir, rotateAngle, pRotate))
                {
                }
                else
                {
                    assert(false);
                }
            }
            continue;
        }

        // 片体没有位置参数, 一律走 Rotate 特征
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem);
        if (pSheet)
        {
            if (!pSheet->getParent().isNull())
            {
                assert(false);
                continue;
            }
            wy3d::Rotate* pRotate(nullptr);
            if (wy::ErrorStatus::Ok == wy3d::Rotate::create(pTrans, pSheet, _centerPnt, _axisDir, rotateAngle, pRotate))
            {
            }
            else
            {
                assert(false);
            }
            continue;
        }

        assert(false);
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;
}

RotateTransient::RotateTransient()
{
    this->initGeom(_geomBaseLine, _verticesBaseLine);
    this->initGeom(_geomRotateLine, _verticesRotateLine);

    _root->addChild(_geomBaseLine);
    _root->addChild(_geomRotateLine);
}

void RotateTransient::initGeom(osg::ref_ptr<osg::Geometry>& geom, osg::ref_ptr<osg::Vec3Array>& vertices)
{
    geom = new osg::Geometry();
    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    // 顶点数组
    vertices = new osg::Vec3Array();
    vertices->resize(2);
    (*vertices)[0] = osg::Vec3(0.0f, 0.0f, 0.0f);
    (*vertices)[1] = osg::Vec3(1.0f, 0.0f, 0.0f);
    geom->setVertexArray(vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(2);
    (*indices)[0] = 0;
    (*indices)[1] = 1;
    // GL_TRIANGLES
    geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));
}

RotateTransient::~RotateTransient()
{
}

void RotateTransient::show()
{
    this->showBaseLine();
    this->showRotateLine();
}

void RotateTransient::showBaseLine()
{
    _geomBaseLine->setNodeMask(1);
}

void RotateTransient::showRotateLine()
{
    _geomRotateLine->setNodeMask(1);
}

void RotateTransient::hide()
{
    this->hideBaseLine();
    this->hideRotateLine();
}

void RotateTransient::hideBaseLine()
{
    _geomBaseLine->setNodeMask(0);
}

void RotateTransient::hideRotateLine()
{
    _geomRotateLine->setNodeMask(0);
}

void RotateTransient::updateBaseLine(const wy::Vector3& pnt1, wy::Vector3 pnt2)
{
    double length = (pnt2 - pnt1).length();
    if (length > 1e-5 && length < 10000)
    {
        wy::Vector3 dir = pnt2 - pnt1;
        dir.normalize();
        pnt2 = pnt1 + dir * 10000;

    }
    (*_verticesBaseLine)[0].set(pnt1.x(), pnt1.y(), pnt1.z());
    (*_verticesBaseLine)[1].set(pnt2.x(), pnt2.y(), pnt2.z());
    _verticesBaseLine->dirty();
    _geomBaseLine->dirtyBound();
}

void RotateTransient::updateRotateLine(const wy::Vector3& pnt1, const wy::Vector3& pnt2)
{
    (*_verticesRotateLine)[0].set(pnt1.x(), pnt1.y(), pnt1.z());
    (*_verticesRotateLine)[1].set(pnt2.x(), pnt2.y(), pnt2.z());
    _verticesRotateLine->dirty();
    _geomRotateLine->dirtyBound();
}
