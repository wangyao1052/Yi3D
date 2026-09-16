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

#include "scene/nodes/ElementNode.h"

#include <cassert>

#include <gp_Quaternion.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <Precision.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <TColgp_Array1OfDir.hxx>
#include <Poly_Connect.hxx>
#include <GeomLib.hxx>

#include <osg/MatrixTransform>
#include <OsgUtils.h>

#include <wyVector3.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wy3dFeature.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>

#include "scene/SketchEntityLinearization.h"

osg::Matrix ElementNode::createMatrix(const wy::Vector3& pos, const wy::Vector3& rot)
{
    static gp_Vec xAxis(1.0, 0.0, 0.0);
    static gp_Vec yAxis(0.0, 1.0, 0.0);
    static gp_Vec zAxis(0.0, 0.0, 1.0);

    osg::Matrix matrix;
    // 移动
    matrix.makeTranslate(pos.x(), pos.y(), pos.z());
    // 旋转(四元数)
    // TODO 此处有优化的空间,可以一次性构造出四元数,而不是三个四元数做乘法
    gp_Quaternion quat;
    gp_Quaternion rotZ(zAxis, rot.z());
    gp_Quaternion rotX(xAxis, rot.x());
    gp_Quaternion rotY(yAxis, rot.y());
    quat = rotY * rotX * rotZ;
    matrix.setRotate(osg::Quat(quat.X(), quat.Y(), quat.Z(), quat.W()));

    return matrix;
}

ElementNode::ElementNode(const wydb::ElementId& id) : _id(id), _status(0)
{
    _osgNode = new osg::Group();
    _osgNode->setNodeMask(NodeMask::Visible);
}

ElementNode::GenRenderDataRet ElementNode::generateRenderData(Scene* pScene, const wydb::Element* pElement)
{
    this->initRenderData();
    return this->generateRenderDataImpl(pScene, pElement);
}

ElementNode::GenRenderDataRet ElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    return GenRenderDataRet::Ok_Empty;
}

osg::BoundingBox ElementNode::computeBoundingBox(const osg::Vec3Array& vertices)
{
    osg::BoundingBox bbox;
    for (const osg::Vec3& vertex : vertices)
    {
        bbox.expandBy(vertex);
    }
    return bbox;
}

void ElementNode::transformBoundingBox(osg::BoundingBox& bbox, const osg::Matrix& matrix)
{
    if (!bbox.valid()) return;
    if (matrix.isIdentity()) return;

    // 求出8个角点
    float xMin = bbox.xMin();
    float xMax = bbox.xMax();
    float yMin = bbox.yMin();
    float yMax = bbox.yMax();
    float zMin = bbox.zMin();
    float zMax = bbox.zMax();
    std::vector<osg::Vec3> cornerPoints;
    cornerPoints.reserve(8);
    // 1
    cornerPoints.emplace_back(osg::Vec3(xMin, yMin, zMin));
    cornerPoints.emplace_back(osg::Vec3(xMin, yMin, zMax));
    cornerPoints.emplace_back(osg::Vec3(xMin, yMax, zMin));
    cornerPoints.emplace_back(osg::Vec3(xMin, yMax, zMax));
    // 2
    cornerPoints.emplace_back(osg::Vec3(xMax, yMin, zMin));
    cornerPoints.emplace_back(osg::Vec3(xMax, yMin, zMax));
    cornerPoints.emplace_back(osg::Vec3(xMax, yMax, zMin));
    cornerPoints.emplace_back(osg::Vec3(xMax, yMax, zMax));

    // 对8个角点做变换构建新包围盒
    bbox.init();
    for (const osg::Vec3& pnt : cornerPoints)
    {
        bbox.expandBy(pnt * matrix);
    }
}

bool ElementNode::updateApperance(wydb::Database* pDb)
{
    // 获取对应元素
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    // 特征
    const wy3d::Feature* pFeature = wy3d::Feature::cast(pElem);
    if (pFeature)
    {
        this->setActive(this->computeWhetherActive(pElem));

        if (pFeature->isHidden())
        {
            this->hide(true);
        }
        else
        {
            this->hide(false);
        }
    }

    return true;
}

bool ElementNode::generateRenderObject(Scene* pScene, wydb::Database* pDb, bool isInitial)
{
    assert(pScene);

    // 清空渲染对象
    this->clearRenderObjects();
    // 清空渲染数据
    this->clearRenderData();
    // 重置包围盒
    this->resetBoundingBox();

    // 获取对应元素
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    // 生成渲染数据
    GenRenderDataRet ret = GenRenderDataRet::Ok;
    try
    {
        ret = this->generateRenderData(pScene, pElem);
        if (GenRenderDataRet::Error == ret)
        {
            assert(false);
            this->clearRenderData();
            return false;
        }
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        this->clearRenderData();
        return false;
    }
    catch (...)
    {
        assert(false);
        this->clearRenderData();
        return false;
    }
    if (GenRenderDataRet::Ok_Empty == ret) // 元素没有形体
    {
        return true;
    }

    // 生成渲染对象
    this->generateRenderObjectImpl(pScene, pElem);

    // 布尔体的成员默认不显示
    this->setActive(this->computeWhetherActive(pElem));

    // added by wangyao 2025.06.04 {
    // FixBug:新建模型;隐藏某些特征;保存;再打开;隐藏的特征在模型树上是灰显的,但在场景中依然可见;
    // 在生成渲染对象时需要设置显示和隐藏状态
    // 感觉可以和上一语句的setActive合并起来就可以只调用updateApperance;稳妥起见还是先按照最小化改动的方式来修改后续再仔细斟酌
    if (pElem->isHidden())
    {
        this->hide(true);
    }
    else
    {
        this->hide(false);
    }
    // }

    // 如果不是初始状态(重生时),还需要刷新高亮显示
    if (!isInitial)
    {
        this->highlight(this->isHighlighted(), true); // 强制刷新显示效果(FixBug:Gizmo拖拽布尔运算结果体松开鼠标后没有高亮显示)
    }

    // 后处理
    this->generateRenderObjectFinished(pElem);

    return true;
}

void ElementNode::reGenerateRenderObject(Scene* pScene, wydb::Database* pDb)
{
    assert(pDb);
    bool isInitial(false);
    this->generateRenderObject(pScene, pDb, isInitial);
}

void ElementNode::recomputeNodeMask()
{
    unsigned int nodeMask(0);
    if (this->isHighlighted())
    {
        nodeMask |= NodeMask::Highlight;
    }
    _osgNode->setNodeMask(nodeMask);
    this->updateVisibleNodeMask();
}

void ElementNode::highlight(bool flag, bool forced)
{
    if (!forced)
    {
        if (this->isHighlighted() == flag)
        {
            return;
        }
    }

    // 取消预览:控制次高位为0
    _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Preview));

    if (flag)
    {
        // 高亮:控制最高位为1
        _osgNode->setNodeMask(_osgNode->getNodeMask() | NodeMask::Highlight);
        this->addStatus(Status::Highlighted);
    }
    else
    {
        // 取消高亮:控制最高位为0
        _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Highlight));
        this->removeStatus(Status::Highlighted);
    }

    this->highlightImpl(flag);
}

void ElementNode::preview(bool flag)
{
    // 预览态是临时态(没有变量记录是否在该状态下),不能影响当前的高亮态.
    if (this->isHighlighted())
    {
        return;
    }

    if (flag)
    {
        // 预览:控制次高位为1
        _osgNode->setNodeMask(_osgNode->getNodeMask() | NodeMask::Preview);
    }
    else
    {
        // 取消预览:控制次高位为0
        _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Preview));
    }

    this->previewImpl(flag);
}

void ElementNode::setActive(bool flag)
{
    if (this->isActive() == flag)
    {
        return;
    }

    if (flag)
        this->removeStatus(Status::Inactive);
    else
        this->addStatus(Status::Inactive);

    this->updateVisibleNodeMask();
    this->setActiveImpl(flag);
}

void ElementNode::hide(bool flag)
{
    if (this->isHidden() == flag)
    {
        return;
    }

    if (flag)
        this->addStatus(Status::Hidden);
    else
        this->removeStatus(Status::Hidden);

    this->updateVisibleNodeMask();
}

//void ElementNode::setStatus(unsigned int status)
//{
//    // Active的优先级是最高的,涉及到节点的显示与隐藏
//    bool isActive = !(status & static_cast<unsigned int>(Status::Inactive));
//    if (isActive != this->isActive())
//    {
//        this->setActive(isActive);
//    }
//
//    // 高亮状态
//    bool isHighlighted = status & static_cast<unsigned int>(Status::Highlighted);
//    if (isHighlighted != this->isHighlighted())
//    {
//        this->highlight(isHighlighted);
//    }
//}

bool ElementNode::pickByNormalBox(osg::Polytope& polytope) const
{
    if (!_boundBox.valid()) return false;
    assert(_vertices);
    if (_vertices->empty()) return false;
    
    // 过滤掉非活动的节点
    // 布尔体的成员节点为Inactive状态
    if (!this->isActive()) return false;

    // 是否与外包围盒(世界坐标系下)有交集
    if (!polytope.contains(_boundBox))
    {
        return false;
    }

    return this->pickByNormalBoxImpl(polytope);
}

bool ElementNode::pickByCrossBox(osg::Polytope& polytope) const
{
    if (!_boundBox.valid()) return false;
    assert(_vertices);
    if (_vertices->empty()) return false;

    // 过滤掉非活动的节点
    // 布尔体的成员节点为Inactive状态
    if (!this->isActive()) return false;

    // 是否与外包围盒(世界坐标系下)有交集
    if (!polytope.contains(_boundBox))
    {
        return false;
    }

    // added by wangyao 2025.01.17 {
    // TODO
    // 临时这么处理,后面的逻辑是错误的
    // 目前pickByCrossBox只是在捕捉的时候会用到,cross框选还是用的OSG那一套
    return true;
    // }

    //// 是否包含某一个点
    //assert(_shapeNode);
    //osg::MatrixTransform* pMatrixTransf = dynamic_cast<osg::MatrixTransform*>(_shapeNode.get());
    //if (pMatrixTransf)
    //{
    //    // 构建模型坐标系下的多面体
    //    osg::Polytope transformedPolytope;
    //    transformedPolytope.setAndTransformProvidingInverse(polytope, pMatrixTransf->getMatrix());

    //    for (const osg::Vec3& vertex : *_vertices) // 模型坐标系下的点坐标
    //    {
    //        if (transformedPolytope.contains(vertex))
    //        {
    //            return true;
    //        }
    //    }
    //}
    //else
    //{
    //    for (const osg::Vec3& vertex : *_vertices) // 模型坐标系下的点坐标
    //    {
    //        if (polytope.contains(vertex))
    //        {
    //            return true;
    //        }
    //    }
    //}
    
    return false;
}
