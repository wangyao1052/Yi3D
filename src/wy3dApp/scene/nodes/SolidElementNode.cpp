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

#include "SolidElementNode.h"

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
#include <GeomLib.hxx>

#include <osg/MatrixTransform>
#include <OsgUtils.h>
#include <osg/LineWidth>

#include "scene/Scene.h"

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
#include <wy3dSolid.h>
#include <wy3dBoolean.h>

#include <osg/BlendFunc>
#include <osg/PolygonOffset>
#include <osg/CullFace>

#include "scene/SketchEntityLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"

bool SolidElementNode::pickByNormalBoxImpl(osg::Polytope& polytope) const
{
    // 是否包含所有点
    assert(_shapeNode);
    osg::MatrixTransform* pMatrixTransf = dynamic_cast<osg::MatrixTransform*>(_shapeNode.get());
    if (pMatrixTransf)
    {
        // 构建模型坐标系下的多面体
        osg::Polytope transformedPolytope;
        transformedPolytope.setAndTransformProvidingInverse(polytope, pMatrixTransf->getMatrix());

        for (const osg::Vec3& vertex : *_vertices)
        {
            if (!transformedPolytope.contains(vertex))
            {
                return false;
            }
        }
    }
    else
    {
        for (const osg::Vec3& vertex : *_vertices)
        {
            if (!polytope.contains(vertex))
            {
                return false;
            }
        }
    }

    return true;
}

bool SolidElementNode::transform(wydb::Database* pDb)
{
    // 由于当前Solid::Transform-->Solid::Shape,所以该接口直接不做任何操作.
    return true;
}

osg::Matrix convertTopLocToOsgMatrix(const TopLoc_Location& loc)
{
    if (loc.IsIdentity())
    {
        return osg::Matrix::identity();
    }
    const gp_Trsf& trsf = loc.Transformation();
    
    // 提取旋转矩阵元素
    gp_Mat rotation = trsf.VectorialPart();
    double r11 = rotation.Value(1, 1);
    double r12 = rotation.Value(1, 2);
    double r13 = rotation.Value(1, 3);
    double r21 = rotation.Value(2, 1);
    double r22 = rotation.Value(2, 2);
    double r23 = rotation.Value(2, 3);
    double r31 = rotation.Value(3, 1);
    double r32 = rotation.Value(3, 2);
    double r33 = rotation.Value(3, 3);

    // 提取平移分量
    gp_XYZ translation = trsf.TranslationPart();
    double tx = translation.X();
    double ty = translation.Y();
    double tz = translation.Z();

    // 构建OSG矩阵（列优先存储）
    return osg::Matrix(
        r11, r21, r31, 0.0,  // 第一列 (旋转部分)
        r12, r22, r32, 0.0,  // 第二列
        r13, r23, r33, 0.0,  // 第三列
        tx,  ty,  tz,  1.0   // 第四列 (平移部分)
    );
}

void SolidElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (!pSolid)
    {
        assert(false);
        return;
    }

    
    if (pSolid->isCut()) this->addFlag(Flag::Cut);
    else this->removeFlag(Flag::Cut);

    assert(_vertices);
    assert(_normals);
    assert(_triangleIndices);
    assert(_lineIndices);
    assert(!_faceInfos.empty());
    assert(!_edgeInfos.empty());

    // 获取移动旋转矩阵
    TopoDS_Shape shape = pSolid->getShape();
    _matrix = convertTopLocToOsgMatrix(shape.Location());

    // 生成包围盒
    osg::BoundingBox bbox = this->computeBoundingBox(*_vertices);
    _boundBoxInit = bbox;
    if (!_matrix.isIdentity())
    {
        this->transformBoundingBox(bbox, _matrix);
    }
    _boundBox = bbox;

    // 面片
    if (!_triangleIndices->empty())
    {
        // batch
        _shapeGeom = this->generateShapeGeom(pElem->getId(), true);
        // no batch
        if (this->hasFlag(Flag::NoBatchFace))
        {
            _shapeGeom->setNodeMask(0);
            _nobatchShapeGeom = this->generateShapeGeom(pElem->getId(), false);
        }
        osg::ref_ptr<osg::MatrixTransform> pMatrixTransform = new osg::MatrixTransform(_matrix);
        pMatrixTransform->addChild(_shapeGeom);
        if (_nobatchShapeGeom) pMatrixTransform->addChild(_nobatchShapeGeom);
        _shapeNode = pMatrixTransform;
        _osgNode->addChild(_shapeNode);
    }
    // 边
    if (!_lineIndices->empty())
    {
        _edgeGeom = this->generateEdgeGeom(pElem->getId());
        osg::ref_ptr<osg::MatrixTransform> pMatrixTransform = new osg::MatrixTransform(_matrix);
        pMatrixTransform->addChild(_edgeGeom);
        _edgeNode = pMatrixTransform;
        _osgNode->addChild(_edgeNode);
    }

    // 同步线框模式
    this->setWireframe(pScene->getDisplayMode() == Scene::DisplayMode::Wireframe);
}

void SolidElementNode::generateRenderObjectFinished(const wydb::Element* pElem)
{
    this->updateColorAndTransparent();
}

void SolidElementNode::setWireframe(bool flag)
{
    _wireframe = flag;
    if (_shapeNode)
    {
        _shapeNode->setNodeMask(flag ? 0 : static_cast<unsigned int>(getNodeType()));
    }
}

osg::ref_ptr<osg::Geometry> SolidElementNode::generateShapeGeom(const wydb::ElementId& id, bool batch) const
{
    if (_triangleIndices->empty() || _faceInfos.empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> shapeGeom = new osg::Geometry();
    shapeGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
    shapeGeom->setUseDisplayList(false);
    shapeGeom->setUseVertexBufferObjects(true);
    shapeGeom->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
    shapeGeom->getOrCreateStateSet()->setAttribute(new osg::CullFace(osg::CullFace::BACK));
    shapeGeom->setVertexArray(_vertices);
    shapeGeom->setNormalArray(_normals, osg::Array::Binding::BIND_PER_VERTEX);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    if (batch)
    {
        colors->push_back(this->getFaceDefaultColor());
        shapeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
        shapeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES, _triangleIndices->begin(), _triangleIndices->end()));
    }
    else
    {
        size_t numFaces = _faceInfos.size();
        colors->resize(numFaces, this->getFaceDefaultColor());
        shapeGeom->setColorArray(colors, osg::Array::Binding::BIND_PER_PRIMITIVE_SET);

        auto iterBeg = _triangleIndices->begin();
        auto iterEnd = iterBeg;
        for (size_t i = 0; i < numFaces; ++i)
        {
            iterEnd += static_cast<size_t>(_faceInfos[i].numTriangles) * 3;
            shapeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES, iterBeg, iterEnd));
            iterBeg = iterEnd;
        }
    }

    shapeGeom->setUserValue("ElementId", static_cast<unsigned int>(id.value()));
    return shapeGeom;
}

osg::ref_ptr<osg::Geometry> SolidElementNode::generateEdgeGeom(const wydb::ElementId& id) const
{
    if (_lineIndices->empty() || _edgeInfos.empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(this->getEdgeDefaultColor());
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _lineIndices->begin(), _lineIndices->end()));
    edgeGeom->setUserValue("ElementId", static_cast<unsigned int>(id.value()));
    osg::StateSet* stateSet = edgeGeom->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(1.5f), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    return edgeGeom;
}

osg::ref_ptr<osg::Geometry> SolidElementNode::generateEdgeGeom_Highlight()
{
    if (_lineIndices->empty() || _edgeInfos.empty())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t totalNumIndices(0);
    for (const EdgeInfo& edgeInfo : _edgeInfos)
    {
        if (edgeInfo.hasFlag(EdgeInfoFlag::Highlight))
        {
            totalNumIndices += static_cast<size_t>(edgeInfo.numLines) * 2;
        }
    }
    if (totalNumIndices == 0) // 没有高亮的边
    {
        return nullptr;
    }
    indices->reserve(totalNumIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (const EdgeInfo& edgeInfo : _edgeInfos)
    {
        size_t numIndices = static_cast<size_t>(edgeInfo.numLines) * 2;
        if (edgeInfo.hasFlag(EdgeInfoFlag::Highlight))
        {
            indices->insert(indices->end(),
                _lineIndices->begin() + startIndex,
                _lineIndices->begin() + startIndex + numIndices);
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~PICK_MASK); // 不可拾取
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kEdge_Highlight);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return edgeGeom;
}

osg::ref_ptr<osg::Geometry> SolidElementNode::generateEdgeGeom_Preview(unsigned int edgeIndex)
{
    if (_lineIndices->empty() || _edgeInfos.empty())
    {
        assert(false);
        return nullptr;
    }
    if (edgeIndex >= _edgeInfos.size())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t numIndices = static_cast<size_t>(_edgeInfos[edgeIndex].numLines) * 2;
    indices->reserve(numIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (size_t i = 0; i < _edgeInfos.size(); ++i)
    {
        const EdgeInfo& edgeInfo = _edgeInfos[i];
        numIndices = static_cast<size_t>(edgeInfo.numLines) * 2;
        if (i == edgeIndex)
        {
            indices->insert(indices->end(),
                _lineIndices->begin() + startIndex,
                _lineIndices->begin() + startIndex + numIndices);
            break;
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~PICK_MASK); // 不可拾取
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kEdge_Preview);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return edgeGeom;
}

ElementNode::GenRenderDataRet SolidElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _normals = new osg::Vec3Array();
    _triangleIndices = new osg::UIntArray();
    _lineIndices = new osg::UIntArray();
    _faceInfos.clear();

    assert(pElement);
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement);
    if (!pSolid)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }

    assert(pSolid);
    TopLoc_Location loc;
    TopoDS_Shape shape = pSolid->getShape();
    shape.Location(loc, Standard_False);
    return this->generateRenderData(shape);
}

ElementNode::GenRenderDataRet SolidElementNode::generateRenderData(TopoDS_Shape& shape)
{
    if (shape.IsNull())
    {
        return GenRenderDataRet::Ok_Empty;
    }

    // 布尔差可以得到shape.IsNull()为false但是shape里面啥都没有
    TopExp_Explorer exp(shape, TopAbs_FACE);
    bool hasFace = exp.More();
    if (!hasFace)
    {
        return GenRenderDataRet::Ok_Empty;
    }

    // 由包围盒确定三角网格化精度
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    bounds.SetGap(0.0);
    Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    Standard_Real deflection = ((xMax - xMin) + (yMax - yMin) + (zMax - zMin)) / 2400.0;
    if (deflection < gp::Resolution())
    {
        deflection = Precision::Confusion();
    }
    deflection = std::min(deflection, 20.0);
    Standard_Real AngDeflectionRads = 0.5; // 弧度

    // 三角网格化
    BRepMesh_IncrementalMesh incrementalMesh(shape, deflection, Standard_False, AngDeflectionRads, Standard_True);
    TopLoc_Location aLoc;
    shape.Location(aLoc);

    // 全局量化数据
    unsigned int numTriangles = 0; // 三角形数量
    unsigned int numNodes = 0;     // 顶点数量
    unsigned int numNorms = 0;     // 法向数量
    unsigned int numFaces = 0, numEdges = 0; // Face&Edge 数量
    std::set<int> faceEdges; // 面中的边,用于区分是否是自由边

    // 遍历面
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    for (int i = 1; i <= faceMap.Extent(); i++)
    {
        Handle(Poly_Triangulation) mesh = BRep_Tool::Triangulation(TopoDS::Face(faceMap(i)), aLoc);
        if (!mesh.IsNull())
        {
            numTriangles += mesh->NbTriangles();
            numNodes += mesh->NbNodes();
            numNorms += mesh->NbNodes();
        }
        else
        {
            assert(false);
        }

        TopExp_Explorer xp;
        for (xp.Init(faceMap(i), TopAbs_EDGE); xp.More(); xp.Next())
        {
            faceEdges.insert(xp.Current().HashCode(INT_MAX));
        }

        numFaces++;
    }

    // 遍历边
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    std::set<int> edgeIdxSet; // all edges(1...NumOfEdges)
    std::map<int, std::vector<int>> lineSetMap; // edge index <> nodes indices
    for (int i = 1; i <= edgeMap.Extent(); i++)
    {
        edgeIdxSet.insert(i);
        const TopoDS_Edge& aEdge = TopoDS::Edge(edgeMap(i));
        int hash = aEdge.HashCode(INT_MAX);
        TopLoc_Location aLoc;
        if (faceEdges.find(hash) == faceEdges.end()) // free edge
        {
            Handle(Poly_Polygon3D) aPoly = BRep_Tool::Polygon3D(aEdge, aLoc);
            if (!aPoly.IsNull())
            {
                int nbNodesInEdge = aPoly->NbNodes();
                numNodes += nbNodesInEdge;
            }
            else
            {
                assert(false);
            }
        }

        numEdges++;
    }

    // 遍历顶点
    TopTools_IndexedMapOfShape vertexMap;
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertexMap);
    numNodes += vertexMap.Extent();

    // 数组容量
    _vertices->resize(numNodes, osg::Vec3(0.0f, 0.0f, 0.0f));
    _normals->resize(numNorms, osg::Vec3(0.0f, 0.0f, 1.0f));
    _triangleIndices->resize(static_cast<size_t>(numTriangles) * 3, 0);
    _faceInfos.resize(numFaces);

    // 遍历面
    unsigned int ii = 0, faceNodeOffset = 0, faceTriaOffset = 0;
    unsigned int idxVertices(0);
    unsigned int idxNormals(0);
    unsigned int idxTriIndices(0);
    for (unsigned int i = 1; i <= faceMap.Extent(); i++, ii++)
    {
        const TopoDS_Face& actFace = TopoDS::Face(faceMap(i));
        TopLoc_Location aLoc;
        Handle(Poly_Triangulation) mesh = BRep_Tool::Triangulation(actFace, aLoc);
        if (mesh.IsNull())
        {
            assert(false);
            _faceInfos[ii].numTriangles = 0; // 第ii个面的三角面片的数量
            continue;
        }

        gp_Trsf myTransf;
        Standard_Boolean identity = true;
        if (!aLoc.IsIdentity())
        {
            identity = false;
            myTransf = aLoc.Transformation();
        }

        // 顶点数组赋值
        unsigned int nbNodesInFace = mesh->NbNodes();
        for (unsigned int k = 1; k <= nbNodesInFace; ++k)
        {
            gp_Pnt pnt = mesh->Node(k);
            if (!identity)
            {
                pnt.Transform(myTransf);
            }
            (*_vertices)[idxVertices++].set(pnt.X(), pnt.Y(), pnt.Z());
        }
        assert(idxVertices == faceNodeOffset + nbNodesInFace);

        // 计算法向量
        TColgp_Array1OfDir Normals(1, nbNodesInFace);
        if (mesh->HasNormals())
        {
            for (Standard_Integer aNodeIter = 1; aNodeIter <= nbNodesInFace; ++aNodeIter)
            {
                Normals(aNodeIter) = mesh->Normal(aNodeIter);
            }
        }
        else
        {
            const TopoDS_Face aZeroFace = TopoDS::Face(actFace.Located(TopLoc_Location()));
            Handle(Geom_Surface) aSurf = BRep_Tool::Surface(aZeroFace);
            const Standard_Real aTol = Precision::Confusion();
            Standard_Boolean hasNodesUV = mesh->HasUVNodes() && !aSurf.IsNull();

            // Poly_Connect在部分网格上析构崩溃，直接遍历三角形生成节点回退法线。
            std::vector<gp_XYZ> fallbackNormals(
                static_cast<size_t>(nbNodesInFace) + 1,
                gp_XYZ(0.0, 0.0, 0.0));
            for (Standard_Integer triangleIndex = 1;
                triangleIndex <= mesh->NbTriangles();
                ++triangleIndex)
            {
                Standard_Integer n1, n2, n3;
                mesh->Triangle(triangleIndex).Get(n1, n2, n3);
                if (n1 < 1 || n1 > nbNodesInFace ||
                    n2 < 1 || n2 > nbNodesInFace ||
                    n3 < 1 || n3 > nbNodesInFace)
                {
                    assert(false);
                    continue;
                }

                const gp_XYZ v1 = mesh->Node(n2).Coord() - mesh->Node(n1).Coord();
                const gp_XYZ v2 = mesh->Node(n3).Coord() - mesh->Node(n1).Coord();
                const gp_XYZ triangleNormal = v1 ^ v2;
                const Standard_Real normalLength = triangleNormal.Modulus();
                if (normalLength < aTol)
                {
                    continue;
                }

                const gp_XYZ unitNormal = triangleNormal / normalLength;
                fallbackNormals[n1] += unitNormal;
                fallbackNormals[n2] += unitNormal;
                fallbackNormals[n3] += unitNormal;
            }

            mesh->AddNormals();
            for (Standard_Integer aNodeIter = 1; aNodeIter <= nbNodesInFace; ++aNodeIter)
            {
                // try to retrieve normal from real surface first, when UV coordinates are available
                if (!hasNodesUV || GeomLib::NormEstim(aSurf, mesh->UVNode(aNodeIter), aTol, Normals(aNodeIter)) > 1)
                {
                    const gp_XYZ& fallbackNormal = fallbackNormals[aNodeIter];
                    Normals(aNodeIter) = fallbackNormal.Modulus() > aTol
                        ? gp_Dir(fallbackNormal)
                        : gp::DZ();
                }
                mesh->SetNormal(aNodeIter, Normals(aNodeIter));
            }
        }
        if (actFace.Orientation() == TopAbs_REVERSED)
        {
            for (Standard_Integer aNodeIter = 1; aNodeIter <= nbNodesInFace; ++aNodeIter)
            {
                Normals.ChangeValue(aNodeIter).Reverse();
            }
        }

        // 法向数组赋值
        for (unsigned int k = 1; k <= nbNodesInFace; ++k)
        {
            gp_Dir normal = Normals(k);
            if (!identity)
            {
                normal.Transform(myTransf);
            }
            (*_normals)[idxNormals++].set(normal.X(), normal.Y(), normal.Z());
        }
        assert(idxNormals == faceNodeOffset + nbNodesInFace);

        // 三角面片索引赋值
        unsigned int nbTriInFace = mesh->NbTriangles();
        for (int g = 1; g <= nbTriInFace; g++)
        {
            Standard_Integer N1, N2, N3;
            mesh->Triangle(g).Get(N1, N2, N3);
            // change orientation of the triangle if the face is reversed
            if (actFace.Orientation() != TopAbs_FORWARD)
            {
                std::swap(N1, N2);
            }
            (*_triangleIndices)[idxTriIndices++] = N1 + faceNodeOffset - 1;
            (*_triangleIndices)[idxTriIndices++] = N2 + faceNodeOffset - 1;
            (*_triangleIndices)[idxTriIndices++] = N3 + faceNodeOffset - 1;
        }
        assert(idxTriIndices == faceTriaOffset * 3 + nbTriInFace * 3);

        // handling the edges on this face
        TopExp_Explorer Exp;
        std::vector<int> edgeIndices;
        edgeIndices.reserve(20);
        for (Exp.Init(actFace, TopAbs_EDGE); Exp.More(); Exp.Next())
        {
            const TopoDS_Edge& curEdge = TopoDS::Edge(Exp.Current());
            int edgeIndex = edgeMap.FindIndex(curEdge);
            edgeIndices.emplace_back(edgeIndex - 1);
            if (edgeIdxSet.find(edgeIndex) != edgeIdxSet.end()) // not handled
            {
                Handle(Poly_PolygonOnTriangulation) aPoly = BRep_Tool::PolygonOnTriangulation(curEdge, mesh, aLoc);
                if (aPoly.IsNull())
                {
                    assert(false);
                    continue; // polygon does not exist
                }
                // getting the indexes of the edge polygon
                const TColStd_Array1OfInteger& indices = aPoly->Nodes();
                lineSetMap[edgeIndex].reserve(indices.Size());
                for (Standard_Integer i = indices.Lower(); i <= indices.Upper(); i++)
                {
                    int index = faceNodeOffset + indices(i) - 1;
                    lineSetMap[edgeIndex].push_back(index);
                }
                edgeIdxSet.erase(edgeIndex); // handled
            }
        }

        _faceInfos[ii].numTriangles = nbTriInFace;
        _faceInfos[ii].edgeIndices.reserve(edgeIndices.size());
        _faceInfos[ii].edgeIndices = edgeIndices;
        faceNodeOffset += nbNodesInFace;
        faceTriaOffset += nbTriInFace;
    }

    // handling of the free edges
    assert(idxVertices == faceNodeOffset);
    idxVertices = faceNodeOffset;
    for (int i = 1; i <= edgeMap.Extent(); i++)
    {
        const TopoDS_Edge& aEdge = TopoDS::Edge(edgeMap(i));
        int hash = aEdge.HashCode(INT_MAX);
        if (faceEdges.find(hash) != faceEdges.end())
        {
            continue; // not free edge
        }

        TopLoc_Location aLoc;
        Handle(Poly_Polygon3D) aPoly = BRep_Tool::Polygon3D(aEdge, aLoc);
        if (aPoly.IsNull())
        {
            continue;
        }

        Standard_Boolean identity = true;
        gp_Trsf myTransf;
        if (!aLoc.IsIdentity())
        {
            identity = false;
            myTransf = aLoc.Transformation();
        }

        const TColgp_Array1OfPnt& aNodes = aPoly->Nodes();
        int nbNodesInEdge = aPoly->NbNodes();
        gp_Pnt pnt;
        lineSetMap[i].reserve(nbNodesInEdge);
        for (Standard_Integer j = 1; j <= nbNodesInEdge; j++)
        {
            pnt = aNodes(j);
            if (!identity) pnt.Transform(myTransf);
            int index = faceNodeOffset + j - 1;
            (*_vertices)[idxVertices++].set(pnt.X(), pnt.Y(), pnt.Z());
            lineSetMap[i].push_back(index);
        }
        faceNodeOffset += nbNodesInEdge;
    }

    // 顶点
    assert(idxVertices == faceNodeOffset);
    idxVertices = faceNodeOffset;
    for (int i = 1; i <= vertexMap.Extent(); i++)
    {
        const TopoDS_Vertex& aVertex = TopoDS::Vertex(vertexMap(i));
        gp_Pnt pnt = BRep_Tool::Pnt(aVertex);
        (*_vertices)[idxVertices++].set(pnt.X(), pnt.Y(), pnt.Z());
    }

    // 校验顶点&三角面片&法向数量
    assert(idxVertices == numNodes);
    assert(idxNormals == numNorms);
    assert(idxTriIndices == numTriangles * 3);

    // 边的Line
    unsigned int numOfLineIndices(0);
    _edgeInfos.reserve(lineSetMap.size());
    for (const auto& kvp : lineSetMap)
    {
        unsigned int size = kvp.second.size();
        assert(size == 0 || size > 1);
        if (size > 1)
        {
            EdgeInfo edgeInfo;
            edgeInfo.numLines = size - 1;
            _edgeInfos.emplace_back(edgeInfo);
            numOfLineIndices += (size - 1) * 2;
        }
    }
    _lineIndices->resize(numOfLineIndices);
    unsigned int idxLine(0);
    for (const auto& kvp : lineSetMap)
    {
        const std::vector<int>& indices = kvp.second;
        if (indices.size() <= 1) continue;
        for (size_t i = 0; i < indices.size() - 1; ++i)
        {
            (*_lineIndices)[idxLine++] = indices[i];
            (*_lineIndices)[idxLine++] = indices[i + 1];
        }
    }
    assert(idxLine == numOfLineIndices);

    return GenRenderDataRet::Ok;
}

void SolidElementNode::highlightImpl(bool flag)
{
    this->updateColorAndTransparent();
}

void SolidElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }

    // 面
    if (_shapeNode)
    {
        OsgUtils::setNodeColor(_shapeNode, flag ? Colors::kSolidFace_Preview : this->getFaceDefaultColor());
    }

    // 边
    if (_edgeGeom)
    {
        OsgUtils::setNodeColor(_edgeGeom, flag ? Colors::kSolidEdge_Preview : this->getEdgeDefaultColor());

        // 预览时关闭深度测试使边线始终可见
        if (flag)
        {
            _edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            _edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Preview, "RenderBin");
        }
        else
        {
            _edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
            _edgeGeom->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
        }
    }
}

void SolidElementNode::setActiveImpl(bool flag)
{
    this->updateColorAndTransparent();
}

bool SolidElementNode::updateApperance(wydb::Database* pDb)
{
    if (!ElementNode::updateApperance(pDb))
    {
        assert(false);
        return false;
    }

    assert(pDb);
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(_id));
    if (!pSolid)
    {
        return false;
    }

    const wy3d::Color& solidColor = pSolid->getColor();
    constexpr float inv255 = 1.0f / 255.0f;
    _solidFaceColor.set(
        static_cast<float>(solidColor.red) * inv255,
        static_cast<float>(solidColor.green) * inv255,
        static_cast<float>(solidColor.blue) * inv255,
        1.0f);
    this->updateColorAndTransparent();
    return true;
}

void SolidElementNode::updateColorAndTransparent()
{
    // 面
    if (_shapeNode)
    {
        if (this->isActive())
        {
            OsgUtils::setNodeColor(_shapeNode, this->isHighlighted() ? Colors::kSolidFace_Highlight : this->getFaceDefaultColor());
        }
        else
        {
            OsgUtils::setNodeColor(_shapeNode, this->isHighlighted() ? Colors::kTransparent : this->getFaceDefaultColor());
        }

        if (_shapeGeom)
        {
            if (this->isActive())
            {
                if (this->hasFlag(Flag::Cut))
                {
                    _shapeGeom->getOrCreateStateSet()->setRenderBinDetails(1, "RenderBin");
                    // z-fighting
                    osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(-1.0f, 1.0f);
                    _shapeGeom->getOrCreateStateSet()->setAttributeAndModes(polyOffset, osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
                }
                else
                {
                    _shapeGeom->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
                    // Push faces slightly behind coplanar edge geometry to keep outlines crisp.
                    osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(1.0f, 1.0f);
                    _shapeGeom->getOrCreateStateSet()->setAttributeAndModes(
                        polyOffset,
                        osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
                }

                // remove transparent
                _shapeGeom->getOrCreateStateSet()->setRenderingHint(osg::StateSet::DEFAULT_BIN);
                _shapeGeom->getOrCreateStateSet()->removeAttribute(osg::StateAttribute::Type::BLENDFUNC);
            }
            else
            {
                // transparent
                _shapeGeom->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                _shapeGeom->getOrCreateStateSet()->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                // z-fighting
                osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(-1.0f, 1.0f);
                _shapeGeom->getOrCreateStateSet()->setAttributeAndModes(polyOffset, osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
            }
        }
    }

    // 边
    if (_edgeGeom)
    {
        if (this->isActive())
        {
            OsgUtils::setNodeColor(_edgeGeom, this->isHighlighted() ? Colors::kSolidEdge_Highlight : this->getEdgeDefaultColor());
        }
        else
        {
            OsgUtils::setNodeColor(_edgeGeom, this->isHighlighted() ? Colors::kTransparent : this->getEdgeDefaultColor());
        }

        if (this->isActive())
        {
            if (this->isHighlighted() || this->hasFlag(Flag::Cut))
            {
                _edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
                _edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");
            }
            else
            {
                _edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
                _edgeGeom->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
            }
        }
        else
        {
            if (this->isHighlighted())
            {
                _edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
                _edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");
            }
            else
            {
                _edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
                _edgeGeom->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
            }
        }
    }
}

unsigned int SolidElementNode::getFaceIndex(unsigned int primitiveIndex) const
{
    if (_faceInfos.empty()) return static_cast<unsigned int>(-1);

    unsigned int faceIndex(-1);
    unsigned int count(0);
    for (auto iter = _faceInfos.cbegin(); iter != _faceInfos.cend(); ++iter)
    {
        ++faceIndex;
        count += iter->numTriangles;
        if (primitiveIndex < count)
        {
            return faceIndex;
        }
    }

    return static_cast<unsigned int>(-1); // 没有找到
}

unsigned int SolidElementNode::getEdgeIndex(unsigned int primitiveIndex) const
{
    if (_edgeInfos.empty()) return static_cast<unsigned int>(-1);

    unsigned int edgeIndex(-1);
    unsigned int count(0);
    for (auto iter = _edgeInfos.cbegin(); iter != _edgeInfos.cend(); ++iter)
    {
        ++edgeIndex;
        count += iter->numLines;
        if (primitiveIndex < count)
        {
            return edgeIndex;
        }
    }

    return static_cast<unsigned int>(-1); // 没有找到
}

void SolidElementNode::highlightFace(unsigned int faceIndex, bool flag)
{
    if (faceIndex >= _faceInfos.size())
    {
        assert(false);
        return;
    }

    this->startNoBatchRender_Face();
    if (flag)
    {
        this->setFaceColor(faceIndex, Colors::kSolidFace_Highlight);
        _faceInfos[faceIndex].addFlag(FaceInfoFlag::Highlight);
    }
    else
    {
        this->setFaceColor(faceIndex, this->getFaceDefaultColor());
        _faceInfos[faceIndex].removeFlag(FaceInfoFlag::Highlight);
    }
}

void SolidElementNode::highlightFace(unsigned int faceIndex, const osg::Vec4& color)
{
    if (faceIndex >= _faceInfos.size())
    {
        assert(false);
        return;
    }

    this->startNoBatchRender_Face();
    this->setFaceColor(faceIndex, color);
    _faceInfos[faceIndex].addFlag(FaceInfoFlag::Highlight);
}

void SolidElementNode::previewFace(unsigned int faceIndex, bool flag)
{
    if (faceIndex >= _faceInfos.size())
    {
        return;
    }

    this->startNoBatchRender_Face();
    if (flag)
    {
        // 预览态是临时态(没有变量记录是否在该状态下),不能影响当前的高亮态.
        if (!_faceInfos[faceIndex].hasFlag(FaceInfoFlag::Highlight))
        {
            this->setFaceColor(faceIndex, Colors::kSolidFace_Preview);
        }
    }
    else
    {
        // 预览态是临时态(没有变量记录是否在该状态下),不能影响当前的高亮态.
        if (!_faceInfos[faceIndex].hasFlag(FaceInfoFlag::Highlight))
        {
            this->setFaceColor(faceIndex, this->getFaceDefaultColor());
        }
    }
}

void SolidElementNode::highlightEdge(unsigned int edgeIndex, bool flag)
{
    if (_edgeGeomHighlight)
    {
        _edgeNode->removeChild(_edgeGeomHighlight);
        _edgeGeomHighlight = nullptr;
    }

    if (edgeIndex >= _edgeInfos.size())
    {
        assert(false);
        return;
    }

    if (flag) _edgeInfos[edgeIndex].addFlag(EdgeInfoFlag::Highlight);
    else _edgeInfos[edgeIndex].removeFlag(EdgeInfoFlag::Highlight);

     osg::ref_ptr<osg::Geometry> edgeGeomHighlight = this->generateEdgeGeom_Highlight();
     if (edgeGeomHighlight)
     {
         _edgeGeomHighlight = edgeGeomHighlight;
         _edgeNode->addChild(_edgeGeomHighlight);
     }
}

void SolidElementNode::previewEdge(unsigned int edgeIndex, bool flag)
{
    if (_edgeGeomPreview)
    {
        _edgeNode->removeChild(_edgeGeomPreview);
        _edgeGeomPreview = nullptr;
    }

    if (edgeIndex >= _edgeInfos.size())
    {
        assert(false);
        return;
    }

    if (flag)
    {
        osg::ref_ptr<osg::Geometry> edgeGeomPreview = this->generateEdgeGeom_Preview(edgeIndex);
        assert(edgeGeomPreview);
        if (edgeGeomPreview)
        {
            _edgeGeomPreview = edgeGeomPreview;
            _edgeNode->addChild(_edgeGeomPreview);
        }
    }
}

void SolidElementNode::clearDynamicRenderGeometry()
{
    this->endNoBatchRender_Face();
    if (_edgeGeomHighlight)
    {
        _edgeNode->removeChild(_edgeGeomHighlight);
        _edgeGeomHighlight = nullptr;
    }
    if (_edgeGeomPreview)
    {
        _edgeNode->removeChild(_edgeGeomPreview);
        _edgeGeomPreview = nullptr;
    }
}

void SolidElementNode::startNoBatchRender_Face()
{
    if (this->hasFlag(Flag::NoBatchFace))
    {
        return;
    }

    assert(_shapeGeom);
    if (_shapeGeom) _shapeGeom->setNodeMask(0);
    assert(!_nobatchShapeGeom);
    _nobatchShapeGeom = this->generateShapeGeom(this->getElementId(), false);
    if (_nobatchShapeGeom)
    {
        assert(_shapeNode);
        if (_shapeNode) _shapeNode->addChild(_nobatchShapeGeom);
        this->updateColorAndTransparent();
    }
    else
    {
        assert(false);
    }
    this->addFlag(Flag::NoBatchFace);
}

void SolidElementNode::endNoBatchRender_Face()
{
    if (!this->hasFlag(Flag::NoBatchFace))
    {
        return;
    }

    assert(_shapeGeom);
    if (_shapeGeom) _shapeGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
    if (_nobatchShapeGeom)
    {
        assert(_shapeNode);
        if (_shapeNode) _shapeNode->removeChild(_nobatchShapeGeom);
        _nobatchShapeGeom = nullptr;
    }
    this->removeFlag(Flag::NoBatchFace);
}

bool SolidElementNode::setFaceColor(unsigned int faceIndex, const osg::Vec4& color)
{
    if (!this->hasFlag(Flag::NoBatchFace))
    {
        return false;
    }
    if (faceIndex >= _faceInfos.size())
    {
        return false;
    }
    assert(_nobatchShapeGeom);
    if (!_nobatchShapeGeom) return false;

    osg::Vec4Array* pColorVec4Arr = dynamic_cast<osg::Vec4Array*>(_nobatchShapeGeom->getColorArray());
    if (pColorVec4Arr)
    {
        assert(pColorVec4Arr->size() == _faceInfos.size());
        if (faceIndex < pColorVec4Arr->size())
        {
            (*pColorVec4Arr)[faceIndex] = color;
            _nobatchShapeGeom->dirtyGLObjects();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool SolidElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);

    const wydb::ElementId parentId = pCurElem->getParent();
    if (parentId.isNull()) return true;

    const wydb::Element* pParent = pCurElem->getDatabase()->getElement(parentId);
    if (wy3d::Boolean::cast(pParent))
    {
        return false;
    }
    else if (wy3d::Solid::cast(pParent))
    {
        return true;
    }
    else
    {
        assert(false);
        return true;
    }
}

osg::Vec4 SolidElementNode::getFaceDefaultColor() const
{
    if (this->hasFlag(Flag::Cut))
        return Colors::kCutFace;
    else
        return _solidFaceColor;
}

osg::Vec4 SolidElementNode::getEdgeDefaultColor() const
{
    if (this->hasFlag(Flag::Cut))
        return Colors::kCutEdge;
    else
        return Colors::kSolidEdge;
}
