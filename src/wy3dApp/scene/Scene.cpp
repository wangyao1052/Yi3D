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

#include "Scene.h"

#include <cassert>

#include <osg/Node>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Group>
#include <osg/LineStipple>
#include <osg/Light>
#include <osg/LightSource>
#include <osgViewer/Viewer>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wy3dSketchEntity.h>
#include <wy3dDatumPlane.h>
#include <wy3dSolidModification.h>
#include <wy3dTableIndex.h>
#include <wy3dSelectionType.h>
#include <wy3dSolid.h>
#include <wy3dBoolean.h>
#include <wy3dCurve.h>

#include "application/Application.h"
#include <wyapSceneManager.h>
#include "scene/nodes/ElementNode.h"
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SketchElementNode.h"
#include "scene/nodes/SketchEntityElementNode.h"
#include "scene/nodes/DatumPlaneElementNode.h"
#include "scene/nodes/SolidModificationElementNode.h"
#include "gizmo/BaseGizmo.h"
#include "gizmo/renderer/OsgGizmoRenderer.h"
#include "scene/nodes/CurveElementNode.h"
#include "view/OsgView.h"
#include "snap/SnapResult.h"
#include "environments/sketch/SketchEnvironment.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"

//#define WCS_SU_STYLE

Scene::Scene(wyap::Document* pDoc) : wyap::Scene(pDoc)
{
    _pRoot = new osg::Group();
    _pRoot->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    // 元素根节点
    _pElemsRoot = new osg::Group();
    _pRoot->addChild(_pElemsRoot);
    std::string str = "WangYao";
    _pElemsRoot->setUserValue("WY3DAPP_TYPE", str);

    // osg测试根节点
    _pOsgTestRoot = new osg::Group();
    _pRoot->addChild(_pOsgTestRoot);

    // Gizmos
    _pGizmosGroup = new osg::Group();
    _pRoot->addChild(_pGizmosGroup);

    // 临时渲染对象
    _pTransientGroup = new osg::Group();
    _pRoot->addChild(_pTransientGroup);

    // 捕捉对象
    _pSnapObjectsGroup = new osg::Group();
    _pRoot->addChild(_pSnapObjectsGroup);
    {
        // 关闭灯光
        _pSnapObjectsGroup->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        // 关闭深度测试
        _pSnapObjectsGroup->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        // 后绘制
        _pSnapObjectsGroup->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SnapObject, "RenderBin");
    }

    // 世界坐标系
#ifdef WCS_SU_STYLE
    _pWCS = this->newSketchUpStyleWCS();
#else
    _pWCS = this->newWCS();
#endif
    if (_pWCS)
    {
        _pWCS->setNodeMask(~PICK_MASK); // 不可拾取
        _pRoot->addChild(_pWCS);
    }

    // 草图坐标系
    _pSketchCsys = this->newSketchCSYS(1e3);
    if (_pSketchCsys)
    {
        _pSketchCsys->setNodeMask(0); // 默认隐藏
        _pRoot->addChild(_pSketchCsys);
    }

    if (pDoc)
    {
        wydb::Database* pDb = pDoc->getDatabase();
        if (pDb)
        {
            wydb::DatabaseChangeInfo changeInfo;
            auto iter = pDb->createIterator();
            for (; !iter.isDone(); iter.moveNext())
            {
                changeInfo.addedIds.insert(iter.current());
            }
            this->onDatabaseChanged(pDb, nullptr, changeInfo);
            this->updateDatumPlaneVisualSize(pDb);
        }
    }
}

Scene::~Scene()
{
    // 销毁元素节点
    for (const auto& kvp : _id2ElemNode)
    {
        ElementNode* pElemNode = kvp.second;
        assert(pElemNode);
        this->destroyElementNode(pElemNode);
    }

    // 清空场景数据
    _id2ElemNode.clear();
}

osg::ref_ptr<osg::Group> Scene::newWCS()
{
    osg::ref_ptr<osg::AutoTransform> wcs = new osg::AutoTransform();
    wcs->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);
    wcs->setAutoScaleToScreen(true);
    {
        // 关闭灯光
        wcs->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        // 关闭深度测试
        wcs->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    }

    // 正向X,Y,Z轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        geom->setNodeMask(~PICK_MASK);
        wcs->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(6);
        float length = 50.0f;
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(length, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, length, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, length));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        geom->setColorArray(colors.get());
        geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        colors->reserve(6);
        float alpha = 1.0f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        // modified by wangyao 2025.04.27 {
        // Y轴的颜色改为青色
        //colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        //colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.1, 0.75, 0.75, alpha));
        colors->push_back(osg::Vec4(0.1, 0.75, 0.75, alpha));
        // }
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.7f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.7f, alpha));
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 6));
        // line width
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    }

    return wcs;
}

osg::ref_ptr<osg::Group> Scene::newSketchUpStyleWCS()
{
    osg::ref_ptr<osg::AutoTransform> wcs = new osg::AutoTransform();
    wcs->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    wcs->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);
    wcs->setAutoScaleToScreen(true);

    // 正向X,Y,Z轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        wcs->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(6);
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(3e3f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 3e3f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 3e3f));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        geom->setColorArray(colors.get());
        geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        colors->reserve(6);
        float alpha = 0.5f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.7f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.7f, alpha));
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 6));
    }

    // 反向X,Y,Z轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        wcs->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(6);
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(-3e3f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, -3e3f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, -3e3f));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        geom->setColorArray(colors.get());
        geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        colors->reserve(6);
        float alpha = 0.5f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.7f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.7f, alpha));
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 6));
        geom->getOrCreateStateSet()->setAttributeAndModes(
            new osg::LineStipple(2, 0xAAAA), osg::StateAttribute::ON);
    }

    return wcs;
}

osg::ref_ptr<osg::AutoTransform> Scene::newSketchCSYS(float axisHalfLen)
{
    osg::ref_ptr<osg::AutoTransform> wcs = new osg::AutoTransform();
    wcs->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    wcs->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    wcs->getOrCreateStateSet()->setRenderBinDetails(10000, "RenderBin");  // 最后绘制
    wcs->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);

    // 正向X,Y轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        geom->setDataVariance(osg::Object::DYNAMIC);
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        wcs->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(4);
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(axisHalfLen, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, axisHalfLen, 0.0f));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        geom->setColorArray(colors.get());
        geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        colors->reserve(4);
        float alpha = 0.5f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 4));
    }

    // 反向X,Y轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        geom->setDataVariance(osg::Object::DYNAMIC);
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        wcs->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(4);
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(-axisHalfLen, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        vertices->push_back(osg::Vec3(0.0f, -axisHalfLen, 0.0f));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        geom->setColorArray(colors.get());
        geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        colors->reserve(4);
        float alpha = 0.5f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 4));
        geom->getOrCreateStateSet()->setAttributeAndModes(
            new osg::LineStipple(2, 0xAAAA), osg::StateAttribute::ON);
    }

    return wcs;
}

bool Scene::updateSketchCSYS(float axisHalfLen)
{
    if (!_pSketchCsys) return false;

    unsigned int num = _pSketchCsys->getNumChildren();
    if (2 != num)
    {
        assert(false);
        return false;
    }

    osg::Geometry* pGeomPositive = dynamic_cast<osg::Geometry*>(_pSketchCsys->getChild(0));
    osg::Geometry* pGeomNegtive = dynamic_cast<osg::Geometry*>(_pSketchCsys->getChild(1));
    if (!pGeomPositive || !pGeomNegtive)
    {
        assert(false);
        return false;
    }

    osg::Vec3Array* pVertexArrayPositive = dynamic_cast<osg::Vec3Array*>(pGeomPositive->getVertexArray());
    osg::Vec3Array* pVertexArrayNegtive = dynamic_cast<osg::Vec3Array*>(pGeomNegtive->getVertexArray());
    if (!pVertexArrayPositive || !pVertexArrayNegtive)
    {
        assert(false);
        return false;
    }
    if (pVertexArrayPositive->size() != 4 || pVertexArrayNegtive->size() != 4)
    {
        assert(false);
        return false;
    }

    (*pVertexArrayPositive)[0].set(0.0f, 0.0f, 0.0f);
    (*pVertexArrayPositive)[1].set(axisHalfLen, 0.0f, 0.0f);
    (*pVertexArrayPositive)[2].set(0.0f, 0.0f, 0.0f);
    (*pVertexArrayPositive)[3].set(0.0f, axisHalfLen, 0.0f);
    pVertexArrayPositive->dirty();
    pGeomPositive->dirtyBound();

    (*pVertexArrayNegtive)[0].set(0.0f, 0.0f, 0.0f);
    (*pVertexArrayNegtive)[1].set(-axisHalfLen, 0.0f, 0.0f);
    (*pVertexArrayNegtive)[2].set(0.0f, 0.0f, 0.0f);
    (*pVertexArrayNegtive)[3].set(0.0f, -axisHalfLen, 0.0f);
    pVertexArrayNegtive->dirty();
    pGeomNegtive->dirtyBound();

    return true;
}

class LightFollowCameraCallback : public osg::NodeCallback
{
public:
    LightFollowCameraCallback(osg::LightSource* lightSource, osgViewer::View* viewer)
        : _lightSource(lightSource), _viewer(viewer) {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        if (_lightSource.valid() && _viewer.valid())
        {
            // 获取摄像机的位置
            osg::Vec3d eye, center, up;
            _viewer->getCamera()->getViewMatrixAsLookAt(eye, center, up);
            osg::Vec3d dir = eye - center;
            dir.normalize();

            // 更新光源的位置
            osg::Vec4 position(dir.x(), dir.y(), dir.z(), 0.0f);
            _lightSource->getLight()->setPosition(position);
        }

        // 继续遍历场景图
        traverse(node, nv);
    }

private:
    osg::ref_ptr<osg::LightSource> _lightSource;
    osg::ref_ptr<osgViewer::View> _viewer;
};

void Scene::initLight(osgViewer::View* pView)
{
    if (!pView)
    {
        assert(false);
        return;
    }
    if (_mainLightSource)
    {
        //assert(false);
        return;
    }

    osg::ref_ptr<osg::Light> mainLight = new osg::Light;
    mainLight->setLightNum(0);
    mainLight->setPosition(osg::Vec4(1.0f, 1.0f, 1.0f, 0.0));    // 方向调整到斜上方
    mainLight->setDiffuse(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));    // 主光漫反射
    mainLight->setAmbient(osg::Vec4(0.32f, 0.32f, 0.32f, 1.0f)); // 环境光成分
    mainLight->setSpecular(osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));   // 高光
    _mainLightSource = new osg::LightSource;
    _mainLightSource->setLight(mainLight);
    _pRoot->addChild(_mainLightSource);

    // 灯光跟随摄像机
    _mainLightSource->setUpdateCallback(new LightFollowCameraCallback(_mainLightSource, pView));
}

void Scene::initBoxSelectRect(osgViewer::View* pView)
{
    if (_pBoxSelectRect)
    {
        _pRoot->removeChild(_pBoxSelectRect);
        _pBoxSelectRect = nullptr;
    }

    if (pView)
    {
        _pBoxSelectRect = new BoxSelectRectangle(pView);
        _pBoxSelectRect->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        _pRoot->addChild(_pBoxSelectRect);
    }
}

osg::BoundingSphere Scene::getElementsBoundingBox() const
{
    return _pElemsRoot->getBound();
}

ElementNode* Scene::getElementNode(const wydb::ElementId& id) const
{
    auto iter = _id2ElemNode.find(id);
    if (iter == _id2ElemNode.cend())
    {
        return nullptr;
    }

    return iter->second;
}

ElementNode* Scene::newElementNodeOfSolid(const wy3d::Solid* pSolid)
{
    assert(pSolid);
    wydb::ElementId id = pSolid->getId();
    auto toOsgColor = [](const wy3d::Color& color) -> osg::Vec4
    {
        constexpr float inv255 = 1.0f / 255.0f;
        return osg::Vec4(
            static_cast<float>(color.red) * inv255,
            static_cast<float>(color.green) * inv255,
            static_cast<float>(color.blue) * inv255,
            1.0f);
    };
    auto newSolidNode = [id](const osg::Vec4& faceColor) -> SolidElementNode*
    {
        SolidElementNode* pNode = new SolidElementNode(id, faceColor);
        return pNode;
    };
    osg::Vec4 faceColor = toOsgColor(pSolid->getColor());

    if (pSolid->getParent().isNull()) // 首层实体元素
    {
        return newSolidNode(faceColor);
    }

    
    if (pSolid->isCut()) // 切除材料
    {
        if (pSolid->getParent().isNull()) // 还未切除主体时创建实体节点,比如在创建拉伸切除命令中(最后才切除实体).
        {
            return newSolidNode(faceColor);
        }
        else // 切除了主体
        {
            return new SolidModificationElementNode(id);
        }
    }
    else
    {
        const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pSolid->getDatabase()->getElement(pSolid->getParent()));
        if (pBoolean) // 说明实体本身是布尔目标体或参与体
        {
            return newSolidNode(faceColor);
        }
        else // 说明实体本身是被合并的
        {
            return new SolidModificationElementNode(id);
        }
    }
}

bool Scene::addElementNode(const wydb::ElementId& id, bool addSketchEntity)
{
    // 校验是否已经存在
    auto iter = _id2ElemNode.find(id);
    if (_id2ElemNode.cend() != iter)
    {
        assert(false);
        return false;
    }

    // 元素
    wyap::Document* pDoc = this->getDocument();
    assert(pDoc);
    wydb::Database* pDb = pDoc->getDatabase();
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    // 创建元素节点
    ElementNode* pElemNode(nullptr);
    // 实体
    if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
    {
        pElemNode = this->newElementNodeOfSolid(pSolid);
    }
    // 草图
    else if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem))
    {
        pElemNode = new SketchElementNode(id);
    }
    // 草图图元
    else if (const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem))
    {
        if (!addSketchEntity)
        {
            return false;
        }
        pElemNode = new SketchEntityElementNode(id);
    }
    // 基准面
    else if (const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElem))
    {
        pElemNode = new DatumPlaneElementNode(id);
    }
    // 实体修改对象
    else if (const wy3d::SolidModification* pSolidMod = wy3d::SolidModification::cast(pElem))
    {
        pElemNode = new SolidModificationElementNode(id);
    }
    // 曲线
    else if (const wy3d::Curve* pCurve = wy3d::Curve::cast(pElem))
    {
        pElemNode = new CurveElementNode(id);
    }
    // 其它
    else
    {
        assert(false);
        return false;
    }

    // 生成元素渲染对象
    if (!pElemNode->generateRenderObject(this, pDb))
    {
        assert(false);
    }

    // 添加到场景中
    this->addToScene(pElemNode);

    // 添加到RTree
    this->addToRTree(pElemNode);

    return true;
}

void Scene::addToScene(ElementNode* pElemNode)
{
    assert(pElemNode);
    // 添加到场景中
    _id2ElemNode[pElemNode->getElementId()] = pElemNode;
    // 添加到OSG场景树中
    this->addToOsgScene(pElemNode);
}

bool isFeature(const wyap::Document* pDoc, const wydb::ElementId& id)
{
    assert(pDoc);
    wydb::Database* pDb = pDoc->getDatabase();
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(id);
    const wy3d::Feature* pFeature = wy3d::Feature::cast(pElem);
    if (pFeature) return true;
    else return false;
}

bool Scene::removeElementNode(const wydb::ElementId& id)
{
    // 校验
    auto iter = _id2ElemNode.find(id);
    if (_id2ElemNode.cend() == iter)
    {
        assert(!isFeature(this->getDocument(), id));
        return false;
    }

    // 从场景中移除
    ElementNode* pElemNode = iter->second;
    assert(pElemNode);
    this->removeFromScene(iter);

    // 从RTree中移除
    this->removeFromRTree(pElemNode);

    // 销毁元素节点
    this->destroyElementNode(pElemNode);

    return true;
}

void Scene::removeFromScene(std::map<wydb::ElementId, ElementNode*>::const_iterator iter)
{
    this->removeFromOsgScene(iter->second);
    _id2ElemNode.erase(iter);
}

bool Scene::modifyElementNode(const wydb::ElementId& id, const wydb::DatabaseChangeDetails& details, bool isInSketchEnv)
{
    // 校验
    auto iter = _id2ElemNode.find(id);
    if (_id2ElemNode.cend() == iter)
    {
        // added by wangyao 2025.02.23 {
        // 修改元素场景节点找不到只可能是在建模环境下undo&redo,草图图元节点更改了
        assert(!isInSketchEnv);
        //assert(id.xdata() >= wy3d::SKETCH_ENTITY_TABLE_INDEX_BEG);
        //assert(id.xdata() <= wy3d::SKETCH_ENTITY_TABLE_INDEX_END);
        // }
        return false;
    }

    ElementNode* pElemNode = iter->second;
    assert(pElemNode);
    wydb::Database* pDb = this->getDocument()->getDatabase();
    assert(pDb);

    // added by wangyao 2025.05.25 {
    // 对于层级修改的实体节点以及实体修改节点,有可能需要切换节点类型
    ElementNodeType nodeType = pElemNode->getNodeType();
    if ((nodeType == ElementNodeType::Solid || nodeType == ElementNodeType::SolidModification)
        && details.isDataPieceDirty(id, wydb::ElementDataPieceType::Hierarchy))
    {
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (pSolid) // 实体元素
        {
            // 子节点>>>首层节点
            if (pSolid->getParent().isNull())
            {
                if (nodeType == ElementNodeType::SolidModification)
                {
                    this->removeElementNode(id);
                    return this->addElementNode(id);
                }
            }
            // 首层节点>>>子节点
            // 如果不是布尔体的成员则需要切换到实体修改节点
            else
            {
                const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pDb->getElement(pSolid->getParent()));
                if (!pBoolean)
                {
                    this->removeElementNode(id);
                    return this->addElementNode(id);
                }
            }
        }
    }
    // }

    // 形体产生了变化
    if (details.isDataPieceDirty(id, wydb::ElementDataPieceType::Completion) ||
        details.isDataPieceDirty(id, wydb::ElementDataPieceType::Shape) ||
        details.isDataPieceDirty(id, wydb::ElementDataPieceType::UserDefinedExt4))
    {
        // 从OSG场景中移除
        //this->removeFromOsgScene(pElemNode); // commented by wangyao 2025.02.21
        // 从RTree中移除
        this->removeFromRTree(pElemNode);
        // 重生
        pElemNode->reGenerateRenderObject(this, pDb);
        // 添加到OSG场景中
        //this->addToOsgScene(pElemNode); // commented by wangyao 2025.02.21
        // 添加到RTree中
        this->addToRTree(pElemNode);
    }
    else
    {
        // 方位产生了变化
        if (details.isDataPieceDirty(id, wydb::ElementDataPieceType::Transform))
        {
            // 从RTree中移除
            this->removeFromRTree(pElemNode);
            // transform
            pElemNode->transform(pDb);
            // 添加到RTree中
            this->addToRTree(pElemNode);
        }

        // 外观产生了变化
        if (details.isDataPieceDirty(id, wydb::ElementDataPieceType::Appearance))
        {
            pElemNode->updateApperance(pDb);
        }
    }

    return true;
}

void Scene::addToOsgScene(const ElementNode* pElemNode)
{
    assert(pElemNode);
    _pElemsRoot->addChild(pElemNode->getOsgNode());
}

void Scene::removeFromOsgScene(const ElementNode* pElemNode)
{
    assert(pElemNode);
    _pElemsRoot->removeChild(pElemNode->getOsgNode());
}

void Scene::addToRTree(const ElementNode* pElemNode)
{
    if (!pElemNode) return;

    const osg::BoundingBox& bbox = pElemNode->getBoundingBox();
    if (bbox.valid())
    {
        double min[3] = { bbox.xMin(), bbox.yMin(), bbox.zMin() };
        double max[3] = { bbox.xMax(), bbox.yMax(), bbox.zMax() };
        if (pElemNode->getNodeType() == ElementNodeType::DatumPlane)
        {
            _rtreeDatum.Insert(min, max, pElemNode->getElementId().value());
        }
        else
        {
            _rtree.Insert(min, max, pElemNode->getElementId().value());
        }
    }
}

void Scene::removeFromRTree(const ElementNode* pElemNode)
{
    if (!pElemNode) return;

    const osg::BoundingBox& bbox = pElemNode->getBoundingBox();
    if (bbox.valid())
    {
        double min[3] = { bbox.xMin(), bbox.yMin(), bbox.zMin() };
        double max[3] = { bbox.xMax(), bbox.yMax(), bbox.zMax() };
        if (pElemNode->getNodeType() == ElementNodeType::DatumPlane)
        {
            _rtreeDatum.Remove(pElemNode->getElementId().value());
        }
        else
        {
            _rtree.Remove(pElemNode->getElementId().value());
        }
    }
}

void Scene::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    assert(pDb);
    bool isInSketchEnv = (_pSketchEnvInfo != nullptr);

    // 区分出实体修改元素
    // 对实体修改元素滞后处理
    std::set<wydb::ElementId> addedSolidMods;
    std::set<wydb::ElementId> modifiedSolidMods;
    auto isSolidModification = [pDb](const wydb::ElementId& id) -> bool
    {
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::SolidModification* pSolidMod = wy3d::SolidModification::cast(pElem);
        if (pSolidMod) return true;
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
        {
            
            if (pSolid->isCut())
            {
                return true;
            }

            return !pSolid->getParent().isNull();
        }
        else
        {
            return false;
        }
    };

    // 新增元素
    for (const wydb::ElementId& id : changeInfo.addedIds)
    {
        if (isSolidModification(id))
        {
            addedSolidMods.insert(id);
        }
        else
        {
            this->addElementNode(id, isInSketchEnv);
        }
    }

    // 修改元素
    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        if (changeInfo.erasedIds.find(id) != changeInfo.erasedIds.cend())
        {
            continue;
        }
        if (changeInfo.addedIds.find(id) != changeInfo.addedIds.cend())
        {
            continue;
        }
        if (isSolidModification(id))
        {
            modifiedSolidMods.insert(id);
        }
        else
        {
            this->modifyElementNode(id, changeInfo.details, isInSketchEnv);
        }
    }

    // 对实体修改元素
    for (const wydb::ElementId& id : addedSolidMods)
    {
        this->addElementNode(id, isInSketchEnv);
    }
    for (const wydb::ElementId& id : modifiedSolidMods)
    {
        this->modifyElementNode(id, changeInfo.details, isInSketchEnv);
    }

    // 删除元素
    for (const wydb::ElementId& id : changeInfo.erasedIds)
    {
        this->removeElementNode(id);
    }

    // Gizmos
    for (auto iter = Application::instance().getGizmoManager()->createIterator();
         !iter.isDone(); iter.moveNext())
    {
        wyap::GizmoSPtr pGizmo = iter.current();
        if (!pGizmo) continue;
        // Gizmo关联的元素修改了则刷新Gizmo
        BaseGizmo* pGizmo2 = dynamic_cast<BaseGizmo*>(pGizmo.get());
        if (!pGizmo2) continue;
        wydb::ElementId id = pGizmo2->getModifiedElement();
        if (changeInfo.modifiedIds.find(id) != changeInfo.modifiedIds.cend())
        {
            pGizmo2->refresh();
        }
    }

    // added by wangyao 2025.07.19 {
    // 刷新草图坐标系的尺寸
    if (_pSketchCsys)
    {
        osg::BoundingSphere boundSphere = this->getElementsBoundingBox();
        float maxDistanceFromOrigin = boundSphere.center().length() + boundSphere.radius();
        maxDistanceFromOrigin *= 1.2;
        float remainder = fmod(maxDistanceFromOrigin, 500.0f);
        if (remainder > 0) maxDistanceFromOrigin += (500.0f - remainder); // 如果余数大于0,则增加(500 - 余数)使值达到下一个500的整数倍
        if (maxDistanceFromOrigin < 1e3)
        {
            maxDistanceFromOrigin = 1e3;
        }
        this->updateSketchCSYS(maxDistanceFromOrigin);
    }
    // }
}

void Scene::onTransactionStarted(wydb::Transaction* pTrans)
{
}

void Scene::onTransactionEnded(wydb::Transaction* pTrans)
{
    wydb::Database* pDb = this->getDocument()->getDatabase();
    wydb::Transaction* pActiveTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (!pActiveTrans)
    {
        this->updateDatumPlaneVisualSize(pDb);
    }
}

void Scene::onTransactionAborted(wydb::Transaction* pTrans)
{
}

void Scene::onTransactionUndone(wydb::Transaction* pTrans)
{
    wydb::Database* pDb = this->getDocument()->getDatabase();
    wydb::Transaction* pActiveTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (!pActiveTrans)
    {
        this->updateDatumPlaneVisualSize(pDb);
    }
}

void Scene::onTransactionRedone(wydb::Transaction* pTrans)
{
    wydb::Database* pDb = this->getDocument()->getDatabase();
    wydb::Transaction* pActiveTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (!pActiveTrans)
    {
        this->updateDatumPlaneVisualSize(pDb);
    }
}

void Scene::updateDatumPlaneVisualSize(const wydb::Database* pDb)
{
    if (!pDb)
    {
        assert(false);
        return;
    }

    // 实体元素的外包围盒
    auto rect = _rtree.GetRootBoundingBox();
    if (!rect.isValid())
    {
        assert(false);
        return;
    }

    osg::BoundingBox bbox(rect.m_min[0], rect.m_min[1], rect.m_min[2], rect.m_max[0], rect.m_max[1], rect.m_max[2]);
    for (auto kvp : _id2ElemNode)
    {
        assert(kvp.second);
        if (kvp.second->getNodeType() != ElementNodeType::DatumPlane) continue;
        DatumPlaneElementNode* pDatumPlaneNode = static_cast<DatumPlaneElementNode*>(kvp.second);
        assert(pDatumPlaneNode);
        this->removeFromRTree(kvp.second);
        pDatumPlaneNode->update(pDb, bbox);
        this->addToRTree(kvp.second);
    }
}

void Scene::updateDatumPlaneVisualSize(const wydb::Database* pDb, const wydb::ElementId& datumPlnId)
{
    if (!pDb)
    {
        assert(false);
        return;
    }

    DatumPlaneElementNode* pDatumPlaneNode = dynamic_cast<DatumPlaneElementNode*>(this->getElementNode(datumPlnId));
    if (!pDatumPlaneNode)
    {
        assert(false);
        return;
    }

    // 实体元素的外包围盒
    auto rect = _rtree.GetRootBoundingBox();
    if (!rect.isValid())
    {
        assert(false);
        return;
    }
    osg::BoundingBox bbox(rect.m_min[0], rect.m_min[1], rect.m_min[2], rect.m_max[0], rect.m_max[1], rect.m_max[2]);

    // 刷新基准面显示大小
    this->removeFromRTree(pDatumPlaneNode);
    pDatumPlaneNode->update(pDb, bbox);
    this->addToRTree(pDatumPlaneNode);
}

void Scene::setDisplayMode(DisplayMode mode)
{
    if (_displayMode == mode) return;
    _displayMode = mode;
    bool isWireframe = (mode == DisplayMode::Wireframe);
    for (auto& kvp : _id2ElemNode)
    {
        if (auto* pNode = dynamic_cast<SolidElementNode*>(kvp.second))
            pNode->setWireframe(isWireframe);
    }
}

void Scene::endNoBatchRender()
{
    for (auto kvp : _id2ElemNode)
    {
        assert(kvp.second);
        switch (kvp.second->getNodeType())
        {
        case ElementNodeType::Solid:
        {
            SolidElementNode* pSolidNode = static_cast<SolidElementNode*>(kvp.second);
            pSolidNode->clearDynamicRenderGeometry();
        }
        break;

        case ElementNodeType::Sketch:
        {
            SketchElementNode* pSketchNode = static_cast<SketchElementNode*>(kvp.second);
            pSketchNode->clearDynamicRenderGeometry();
        }
        break;

        case ElementNodeType::SketchEntity:
        case ElementNodeType::DatumPlane:
        case ElementNodeType::SolidModification:
        case ElementNodeType::Curve:
        {
            // do nothing
        }
        break;

        default:
        {
            assert(false);
        }
        break;
        }
    }
}

// assert专用
static bool isElementErasedByAssert(const wydb::ElementId& id)
{
    wyap::Document* pActiveDoc = Application::instance().getDocManager()->getActiveDocument();
    assert(pActiveDoc);
    wydb::Database* pActiveDb = pActiveDoc->getDatabase();
    assert(pActiveDb);
    const wydb::Element* pElem = pActiveDb->getElement(id);
    assert(pElem);
    return pElem->isErased();
}

bool _checkIsAllSelsOneItemType(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& curSS)
{
    std::set<unsigned int> types;

    for (auto iter = addedSS.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        types.insert(sel.getSelectionType());
        if (types.size() > 1)
        {
            return false;
        }
    }

    for (auto iter = removedSS.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        types.insert(sel.getSelectionType());
        if (types.size() > 1)
        {
            return false;
        }
    }

    for (auto iter = curSS.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        types.insert(sel.getSelectionType());
        if (types.size() > 1)
        {
            return false;
        }
    }

    return true;
}

void Scene::onSelectionChanged(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& curSS)
{
    // 理论上选择集中只可能存在同一类型项类型
    // 需要在程序操作上限制(在Creo中就存在这个问题)
    assert(_checkIsAllSelsOneItemType(addedSS, removedSS, curSS));

    // 移除选择集
    for (auto iter = removedSS.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        ElementNode* pElemNode = this->getElementNode(sel.getElementId());
        if (!pElemNode)
        {
            // 此种情况只可能是:删除处于选择集中的元素
            assert(isElementErasedByAssert(sel.getElementId()));
            continue;
        }

        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::Element:
        {
            pElemNode->highlight(false);
        }
        break;

        case wy3d::SelectionType::SolidFace:
        {
            if (sel.getSubPath().empty())
            {
                assert(false);
                break;
            }
            SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode);
            if (!pSolidElemNode)
            {
                assert(false);
                break;
            }
            pSolidElemNode->highlightFace(std::stoul(sel.getSubPath()), false);
        }
        break;

        case wy3d::SelectionType::SolidEdge:
        {
            if (sel.getSubPath().empty())
            {
                assert(false);
                break;
            }
            SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode);
            if (!pSolidElemNode)
            {
                assert(false);
                break;
            }
            pSolidElemNode->highlightEdge(std::stoul(sel.getSubPath()), false);
        }
        break;

        case wy3d::SelectionType::SketchCurve:
        {
            if (sel.getSubPath().empty())
            {
                assert(false);
                break;
            }
            SketchElementNode* pSketchElemNode = dynamic_cast<SketchElementNode*>(pElemNode);
            if (!pSketchElemNode)
            {
                assert(false);
                break;
            }
            pSketchElemNode->highlightCurveById(std::stoul(sel.getSubPath()), false);
        }
        break;

        default:
        {
            assert(false);
        }
        break;
        }
    }

    // 添加选择集
    for (auto iter = addedSS.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        ElementNode* pElemNode = this->getElementNode(sel.getElementId());
        if (!pElemNode)
        {
            assert(false);
            continue;
        }

        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::Element:
        {
            pElemNode->highlight(true);
        }
        break;

        case wy3d::SelectionType::SolidFace:
        {
            if (sel.getSubPath().empty())
            {
                assert(false);
                break;
            }
            SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode);
            if (!pSolidElemNode)
            {
                assert(false);
                break;
            }
            pSolidElemNode->highlightFace(std::stoul(sel.getSubPath()), true);
        }
        break;

        case wy3d::SelectionType::SolidEdge:
        {
            if (sel.getSubPath().empty())
            {
                assert(false);
                break;
            }
            SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode);
            if (!pSolidElemNode)
            {
                assert(false);
                break;
            }
            pSolidElemNode->highlightEdge(std::stoul(sel.getSubPath()), true);
        }
        break;

        case wy3d::SelectionType::SketchCurve:
        {
            if (sel.getSubPath().empty())
            {
                assert(false);
                break;
            }
            SketchElementNode* pSketchElemNode = dynamic_cast<SketchElementNode*>(pElemNode);
            if (!pSketchElemNode)
            {
                assert(false);
                break;
            }
            pSketchElemNode->highlightCurveById(std::stoul(sel.getSubPath()), true);
        }
        break;

        default:
        {
            assert(false);
        }
        break;
        }
    }
}

void Scene::onGizmoChanged(
    const std::set<wyap::GizmoSPtr>& addedGizmos,
    const std::set<wyap::GizmoSPtr>& removedGizmos,
    const std::set<wyap::GizmoSPtr>& currGizmos)
{
    // 移除Gizmo
    for (const wyap::GizmoSPtr pGizmoSPtr : removedGizmos)
    {
        if (!pGizmoSPtr) continue;
        BaseGizmo* pGizmo = dynamic_cast<BaseGizmo*>(pGizmoSPtr.get());
        if (!pGizmo)
        {
            assert(false);
            continue;
        }
        auto* pRenderer = static_cast<OsgGizmoRenderer*>(pGizmo->getRenderer());
        pRenderer->detachFromScene(this);
        auto iter = _node2Gizmo.find(pRenderer->getOsgNode());
        if (iter != _node2Gizmo.cend())
        {
            _node2Gizmo.erase(iter);
        }
        else
        {
            assert(false);
        }
    }

    // 添加Gizmo
    for (const wyap::GizmoSPtr pGizmoSPtr : addedGizmos)
    {
        if (!pGizmoSPtr) continue;
        BaseGizmo* pGizmo = dynamic_cast<BaseGizmo*>(pGizmoSPtr.get());
        if (!pGizmo)
        {
            assert(false);
            continue;
        }
        pGizmo->refresh();
        auto* pRenderer = static_cast<OsgGizmoRenderer*>(pGizmo->getRenderer());
        pRenderer->attachToScene(this);
        _node2Gizmo[pRenderer->getOsgNode()] = pGizmoSPtr;
    }

    assert(_pGizmosGroup->getNumChildren() == currGizmos.size());
    assert(_node2Gizmo.size() == currGizmos.size());
}

void Scene::onGizmoToBeDeactivated(wyap::GizmoSPtr pGizmoSPtr)
{
    assert(pGizmoSPtr);
    BaseGizmo* pGizmo = dynamic_cast<BaseGizmo*>(pGizmoSPtr.get());
    assert(pGizmo);
}

void Scene::onGizmoToBeActivated(wyap::GizmoSPtr pGizmoSPtr)
{
    assert(pGizmoSPtr);
}
void Scene::onGizmoActivated(wyap::GizmoSPtr pGizmoSPtr)
{
    BaseGizmo* pGizmo = dynamic_cast<BaseGizmo*>(pGizmoSPtr.get());
    assert(pGizmo);
}

void Scene::onSnapResultChanged(
    wyap::SnapResultSPtr pCurSnapResult,
    wyap::SnapResultSPtr pLastSnapResult)
{
    if (pLastSnapResult)
    {
        osg::ref_ptr<osg::Node> pNode = REAL_SNAP_RESULT(pLastSnapResult.get())->getOrCreateOsgNode();
        if (pNode) this->removeSnapObject(pNode);
    }

    if (pCurSnapResult)
    {
        osg::ref_ptr<osg::Node> pNode = REAL_SNAP_RESULT(pCurSnapResult.get())->getOrCreateOsgNode();
        if (pNode) this->addSnapObject(pNode);
    }
}

std::list<wydb::ElementId> Scene::pickByNormalBox(osg::Polytope& polytope, unsigned int pickMask) const
{
    return this->pickByBoxImpl(polytope, false, pickMask);
}

std::list<wydb::ElementId> Scene::pickByCrossBox(osg::Polytope& polytope, unsigned int pickMask) const
{
    return this->pickByBoxImpl(polytope, true, pickMask);
}

std::list<wydb::ElementId> Scene::pickByBoxImpl(osg::Polytope& polytope, bool isCross, unsigned int pickMask) const
{
    // RTree初筛
    std::set<unsigned int> rtreeSelectIds;
    _rtree.BoxSelect(polytope, isCross, [&rtreeSelectIds](const unsigned int& idValue) {
        rtreeSelectIds.insert(idValue);
        return true;
        });
    _rtreeDatum.BoxSelect(polytope, isCross, [&rtreeSelectIds](const unsigned int& idValue) {
        rtreeSelectIds.insert(idValue);
        return true;
        });

    std::list<wydb::ElementId> pickedIds;
    for (const auto& kvp : _id2ElemNode)
    {
        if (rtreeSelectIds.find(kvp.first.value()) == rtreeSelectIds.cend())
        {
            continue;
        }

        const ElementNode* pElemNode = kvp.second;
        assert(pElemNode);
        bool canPick = static_cast<unsigned int>(pElemNode->getNodeType()) & pickMask;
        if (!canPick) continue;

        if (isCross)
        {
            if (pElemNode->pickByCrossBox(polytope))
            {
                pickedIds.emplace_back(pElemNode->getElementId());
            }
        }
        else
        {
            if (pElemNode->pickByNormalBox(polytope))
            {
                pickedIds.emplace_back(pElemNode->getElementId());
            }
        }
    }
    return pickedIds;
}

wyap::GizmoSPtr Scene::pickGizmo(osg::Polytope& polytope) const
{
    osg::ref_ptr<osgUtil::PolytopeIntersector> intersector = new osgUtil::PolytopeIntersector(
        osgUtil::Intersector::MODEL, polytope);
    intersector->setIntersectionLimit(osgUtil::Intersector::IntersectionLimit::LIMIT_ONE_PER_DRAWABLE);
    osgUtil::IntersectionVisitor iv(intersector.get());
    // added by wangyao 2025.03.01 {
    // 排除一些不需要Pick的Gizmo,比如GhostGizmo(设置NodeMask为~PICK_GIZMO_MASK)
    iv.setTraversalMask(PICK_GIZMO_MASK);
    // }
    _pGizmosGroup->accept(iv);
    if (!intersector->containsIntersections())
    {
        return nullptr;
    }

    osgUtil::PolytopeIntersector::Intersections& intersections = intersector->getIntersections();
    for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
    {
        const osgUtil::PolytopeIntersector::Intersection& intersection = *iter;

        // 遍历NodePath找出OsgGizmo节点
        for (osg::NodePath::const_iterator iter = intersection.nodePath.cbegin();
            iter != intersection.nodePath.cend();
            ++iter)
        {
            osg::Node* pNode = *iter;
            OsgGizmoNode* pGizmoNode = dynamic_cast<OsgGizmoNode*>(pNode);
            if (!pGizmoNode) continue;
            auto iterNode2Gizmo = _node2Gizmo.find(pGizmoNode);
            if (iterNode2Gizmo != _node2Gizmo.cend())
            {
                assert(iterNode2Gizmo->second);
                return iterNode2Gizmo->second;
            }
            else
            {
                assert(false);
            }
        }
    }

    return nullptr;
}

void Scene::onEnvironmentToBeEntered(wyap::Environment* pEnvironment)
{
}

void Scene::onEnvironmentEntered(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnvironment);
    if (pSketchEnv)
    {
        this->enterSketchEnvironment(pSketchEnv);
    }
}

void Scene::onEnvironmentToBeExited(wyap::Environment* pEnvironment)
{
}

void Scene::onEnvironmentExited(
    wyap::Environment* pEnvironment,
    wyap::Environment::ExitCode exitCode)
{
    assert(pEnvironment);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnvironment);
    if (pSketchEnv)
    {
        this->exitSketchEnvironment(pSketchEnv, exitCode);
    }
}

void Scene::enterSketchEnvironment(SketchEnvironment* pSketchEnv)
{
    assert(pSketchEnv);
    assert(!_pSketchEnvInfo);
    
    wydb::ElementId sketchId = pSketchEnv->getSketchId();
    ElementNode* pSketchElemNode = this->getElementNode(sketchId);
    assert(pSketchElemNode);
    if (pSketchElemNode)
    {
        this->removeFromOsgScene(pSketchElemNode);
    }

    _pSketchEnvInfo = std::make_shared<SketchEnvInfo>();
    _pSketchEnvInfo->sketchId = sketchId;
    _pSketchEnvInfo->pSketchElemNode = pSketchElemNode;

    assert(this->getDocument());
    wydb::Database* pDb = this->getDocument()->getDatabase();
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(sketchId);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch)
    {
        assert(false);
        return;
    }
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        this->addElementNode(iter.current(), true);
    }

    // 移除SketchOwner的渲染节点
    if (!pSketch->getParent().isNull())
    {
        ElementNode* pSketchOwnerElemNode = this->getElementNode(pSketch->getParent());
        assert(pSketchOwnerElemNode);
        if (pSketchOwnerElemNode)
        {
            this->removeFromOsgScene(pSketchOwnerElemNode);
        }
    }

    // 显示草图坐标系
    this->showSketchCSYS(pSketchEnv->getSketchPlane());
}

void Scene::exitSketchEnvironment(
    SketchEnvironment* pSketchEnv,
    wyap::Environment::ExitCode exitCode)
{
    assert(pSketchEnv);
    assert(_pSketchEnvInfo);

    // 隐藏草图坐标系
    this->hideSketchCSYS();

    wydb::ElementId sketchId = pSketchEnv->getSketchId();
    assert(sketchId == _pSketchEnvInfo->sketchId);
    ElementNode* pSketchElemNode = this->getElementNode(sketchId);
    // 新建草图
    if (pSketchEnv->getOperation() == SketchEnvironment::Operation::New)
    {
        if (wyap::Environment::ExitCode::Ok == exitCode &&
            pSketchEnv->isTransactionCommited())
        {
            assert(pSketchElemNode);
            assert(pSketchElemNode == _pSketchEnvInfo->pSketchElemNode);
            if (pSketchElemNode)
            {
                this->addToOsgScene(pSketchElemNode);
            }
        }
        else
        {
            assert(!pSketchElemNode);
        }
    }
    // 编辑草图
    else
    {
        assert(pSketchElemNode);
        assert(pSketchElemNode == _pSketchEnvInfo->pSketchElemNode);
        if (pSketchElemNode)
        {
            this->addToOsgScene(pSketchElemNode);
        }
    }

    _pSketchEnvInfo = nullptr;

    assert(this->getDocument());
    wydb::Database* pDb = this->getDocument()->getDatabase();
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(sketchId);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch)
    {
        assert(false);
        return;
    }
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        this->removeElementNode(iter.current());
    }

    // 添加SketchOwner的渲染节点
    if (!pSketch->getParent().isNull())
    {
        ElementNode* pSketchOwnerElemNode = this->getElementNode(pSketch->getParent());
        assert(pSketchOwnerElemNode);
        if (pSketchOwnerElemNode)
        {
            this->addToOsgScene(pSketchOwnerElemNode);
        }
    }
}

bool Scene::isWCSVisible() const
{
    if (_pWCS)
    {
        return _pWCS->getNodeMask() != 0;
    }
    else
    {
        return false;
    }
}

void Scene::showWCS()
{
    if (_pWCS)
    {
        _pWCS->setNodeMask(~PICK_MASK);
    }
}

void Scene::hideWCS()
{
    if (_pWCS)
    {
        _pWCS->setNodeMask(0);
    }
}

void Scene::showSketchCSYS(const wy3d::SketchPlane& sketchPlane)
{
    if (!_pSketchCsys) return;

    if (!sketchPlane.isValid())
    {
        assert(false);
        return;
    }
    // 平移
    _pSketchCsys->setPosition(MathUtils::toVec3d(sketchPlane.getOrigin()));
    // 旋转
    wy::Vector3 xDir = sketchPlane.getXDir();
    wy::Vector3 yDir = sketchPlane.getYDir();
    wy::Vector3 zDir = sketchPlane.getNormal();
    osg::Matrix matrix;
    matrix.set(
        xDir.x(), yDir.x(), zDir.x(), 0,
        xDir.y(), yDir.y(), zDir.y(), 0,
        xDir.z(), yDir.z(), zDir.z(), 0,
        0.0, 0.0, 0.0, 1.0);
    matrix.transpose(matrix); // 转置矩阵以适配OSG的列优先存储
    osg::Quat quat = matrix.getRotate();
    _pSketchCsys->setRotation(quat);

    // 显示
    _pSketchCsys->setNodeMask(~PICK_MASK); // 显示且不能被Pick
}

void Scene::hideSketchCSYS()
{
    if (!_pSketchCsys) return;

    _pSketchCsys->setPosition(osg::Vec3d());
    _pSketchCsys->setRotation(osg::Quat());
    _pSketchCsys->setNodeMask(0); // 隐藏
}
