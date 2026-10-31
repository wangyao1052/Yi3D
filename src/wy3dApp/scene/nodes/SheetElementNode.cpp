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

#include "SheetElementNode.h"

#include <cassert>
#include <string>

#include <Standard_Failure.hxx>
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

#include "select/PickCommon.h"

#include <wy3dApp/Scene.h>

#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wy3dFeature.h>
#include <wy3dSheet.h>

#include <osg/BlendFunc>
#include <osg/LightModel>
#include <osg/PolygonOffset>
#include <wy3dApp/ElementNodeType.h>
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/MeshRenderObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static osg::Matrix convertTopLocToOsgMatrix(const TopLoc_Location& loc)
{
    if (loc.IsIdentity())
    {
        return osg::Matrix::identity();
    }
    const gp_Trsf& trsf = loc.Transformation();

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

    gp_XYZ translation = trsf.TranslationPart();
    double tx = translation.X();
    double ty = translation.Y();
    double tz = translation.Z();

    return osg::Matrix(
        r11, r21, r31, 0.0,
        r12, r22, r32, 0.0,
        r13, r23, r33, 0.0,
        tx,  ty,  tz,  1.0
    );
}

std::unique_ptr<RenderObject> SheetElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    assert(pScene);
    assert(pElem);
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem);
    if (!pSheet)
    {
        assert(false);
        return nullptr;
    }

    MeshRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->normals || !data->triangleIndices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }
    assert(!data->faceInfos.empty());
    assert(!data->edgeInfos.empty());

    TopoDS_Shape shape = pSheet->getShape();
    _matrix = convertTopLocToOsgMatrix(shape.Location());

    auto renderObject = std::make_unique<MeshRenderObject>(
        pElem->getId(), this->getNodeType(), *data, _matrix);
    renderObject->build(this->makeStyle(), this->hasFlag(Flag::NoBatchFace));

    // 同步显示模式
    Scene::DisplayMode mode = pScene->getDisplayMode();
    renderObject->setWireframe(mode == Scene::DisplayMode::Wireframe);
    renderObject->setEdgesVisible(mode != Scene::DisplayMode::Shaded);

    return renderObject;
}

MeshRenderObject* SheetElementNode::renderObject() const
{
    return this->getRenderObjectAs<MeshRenderObject>();
}

MeshRenderStyle SheetElementNode::makeStyle() const
{
    MeshRenderStyle style;
    style.doubleSided = true;
    style.active = this->isActive();
    style.highlighted = this->isHighlighted();
    style.cut = false;
    style.faceColor = this->getFaceDefaultColor();
    style.edgeColor = this->getEdgeDefaultColor();
    style.inactiveColor = Colors::kTransparent;
    style.faceHighlightColor = Colors::kSheetFace_Highlight;
    style.facePreviewColor = Colors::kSheetFace_Preview;
    style.edgeHighlightColor = Colors::kSheetEdge_Highlight;
    style.edgePreviewColor = Colors::kSheetEdge_Preview;
    style.edgePreviewWireframeColor = Colors::kEdge_Preview;
    style.edgeHighlightOverlayColor = Colors::kEdge_Highlight;
    style.edgePreviewOverlayColor = Colors::kEdge_Preview;
    return style;
}

void SheetElementNode::generateRenderObjectFinished(const wydb::Element* pElem)
{
    this->updateColorAndTransparent();
}

void SheetElementNode::setWireframe(bool flag)
{
    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->setWireframe(flag);
    }
}

bool SheetElementNode::isWireframe() const
{
    MeshRenderObject* obj = this->renderObject();
    return obj ? obj->isWireframe() : false;
}

void SheetElementNode::setEdgesVisible(bool flag)
{
    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->setEdgesVisible(flag);
    }
}

bool SheetElementNode::isEdgesVisible() const
{
    MeshRenderObject* obj = this->renderObject();
    return obj ? obj->isEdgesVisible() : true;
}

std::unique_ptr<RenderData> SheetElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<MeshRenderData>();

        assert(pElement);
        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElement);
        if (!pSheet)
        {
            assert(false);
            return nullptr;
        }

        TopLoc_Location loc;
        TopoDS_Shape shape = pSheet->getShape();
        shape.Location(loc, Standard_False);
        if (!this->generateMeshData(shape, *data))
        {
            return nullptr;
        }
        return data;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

bool SheetElementNode::generateMeshData(TopoDS_Shape& shape, MeshRenderData& data)
{
    if (shape.IsNull())
    {
        return false;
    }

    TopExp_Explorer exp(shape, TopAbs_FACE);
    bool hasFace = exp.More();
    if (!hasFace)
    {
        return false;
    }

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
    Standard_Real AngDeflectionRads = 0.5;

    BRepMesh_IncrementalMesh incrementalMesh(shape, deflection, Standard_False, AngDeflectionRads, Standard_True);
    TopLoc_Location aLoc;
    shape.Location(aLoc);

    unsigned int numTriangles = 0;
    unsigned int numNodes = 0;
    unsigned int numNorms = 0;
    unsigned int numFaces = 0, numEdges = 0;
    std::set<int> faceEdges;

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

    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    std::set<int> edgeIdxSet;
    std::map<int, std::vector<int>> lineSetMap;
    for (int i = 1; i <= edgeMap.Extent(); i++)
    {
        edgeIdxSet.insert(i);
        const TopoDS_Edge& aEdge = TopoDS::Edge(edgeMap(i));
        int hash = aEdge.HashCode(INT_MAX);
        TopLoc_Location aLoc;
        if (faceEdges.find(hash) == faceEdges.end())
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

    TopTools_IndexedMapOfShape vertexMap;
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertexMap);
    numNodes += vertexMap.Extent();

    data.vertices->resize(numNodes, osg::Vec3(0.0f, 0.0f, 0.0f));
    data.normals->resize(numNorms, osg::Vec3(0.0f, 0.0f, 1.0f));
    data.triangleIndices->resize(static_cast<size_t>(numTriangles) * 3, 0);
    data.faceInfos.resize(numFaces);

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
            data.faceInfos[ii].numTriangles = 0;
            continue;
        }

        gp_Trsf myTransf;
        Standard_Boolean identity = true;
        if (!aLoc.IsIdentity())
        {
            identity = false;
            myTransf = aLoc.Transformation();
        }

        unsigned int nbNodesInFace = mesh->NbNodes();
        for (unsigned int k = 1; k <= nbNodesInFace; ++k)
        {
            gp_Pnt pnt = mesh->Node(k);
            if (!identity)
            {
                pnt.Transform(myTransf);
            }
            (*data.vertices)[idxVertices++].set(pnt.X(), pnt.Y(), pnt.Z());
        }
        assert(idxVertices == faceNodeOffset + nbNodesInFace);

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

        for (unsigned int k = 1; k <= nbNodesInFace; ++k)
        {
            gp_Dir normal = Normals(k);
            if (!identity)
            {
                normal.Transform(myTransf);
            }
            (*data.normals)[idxNormals++].set(normal.X(), normal.Y(), normal.Z());
        }
        assert(idxNormals == faceNodeOffset + nbNodesInFace);

        unsigned int nbTriInFace = mesh->NbTriangles();
        for (int g = 1; g <= nbTriInFace; g++)
        {
            Standard_Integer N1, N2, N3;
            mesh->Triangle(g).Get(N1, N2, N3);
            if (actFace.Orientation() != TopAbs_FORWARD)
            {
                std::swap(N1, N2);
            }
            (*data.triangleIndices)[idxTriIndices++] = N1 + faceNodeOffset - 1;
            (*data.triangleIndices)[idxTriIndices++] = N2 + faceNodeOffset - 1;
            (*data.triangleIndices)[idxTriIndices++] = N3 + faceNodeOffset - 1;
        }
        assert(idxTriIndices == faceTriaOffset * 3 + nbTriInFace * 3);

        TopExp_Explorer Exp;
        std::vector<int> edgeIndices;
        edgeIndices.reserve(20);
        for (Exp.Init(actFace, TopAbs_EDGE); Exp.More(); Exp.Next())
        {
            const TopoDS_Edge& curEdge = TopoDS::Edge(Exp.Current());
            int edgeIndex = edgeMap.FindIndex(curEdge);
            edgeIndices.emplace_back(edgeIndex - 1);
            if (edgeIdxSet.find(edgeIndex) != edgeIdxSet.end())
            {
                Handle(Poly_PolygonOnTriangulation) aPoly = BRep_Tool::PolygonOnTriangulation(curEdge, mesh, aLoc);
                if (aPoly.IsNull())
                {
                    assert(false);
                    continue;
                }
                const TColStd_Array1OfInteger& indices = aPoly->Nodes();
                lineSetMap[edgeIndex].reserve(indices.Size());
                for (Standard_Integer m = indices.Lower(); m <= indices.Upper(); m++)
                {
                    int index = faceNodeOffset + indices(m) - 1;
                    lineSetMap[edgeIndex].push_back(index);
                }
                edgeIdxSet.erase(edgeIndex);
            }
        }

        data.faceInfos[ii].numTriangles = nbTriInFace;
        data.faceInfos[ii].edgeIndices.reserve(edgeIndices.size());
        data.faceInfos[ii].edgeIndices = edgeIndices;
        faceNodeOffset += nbNodesInFace;
        faceTriaOffset += nbTriInFace;
    }

    assert(idxVertices == faceNodeOffset);
    idxVertices = faceNodeOffset;
    for (int i = 1; i <= edgeMap.Extent(); i++)
    {
        const TopoDS_Edge& aEdge = TopoDS::Edge(edgeMap(i));
        int hash = aEdge.HashCode(INT_MAX);
        if (faceEdges.find(hash) != faceEdges.end())
        {
            continue;
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
            (*data.vertices)[idxVertices++].set(pnt.X(), pnt.Y(), pnt.Z());
            lineSetMap[i].push_back(index);
        }
        faceNodeOffset += nbNodesInEdge;
    }

    assert(idxVertices == faceNodeOffset);
    idxVertices = faceNodeOffset;
    for (int i = 1; i <= vertexMap.Extent(); i++)
    {
        const TopoDS_Vertex& aVertex = TopoDS::Vertex(vertexMap(i));
        gp_Pnt pnt = BRep_Tool::Pnt(aVertex);
        (*data.vertices)[idxVertices++].set(pnt.X(), pnt.Y(), pnt.Z());
    }

    assert(idxVertices == numNodes);
    assert(idxNormals == numNorms);
    assert(idxTriIndices == numTriangles * 3);

    unsigned int numOfLineIndices(0);
    data.edgeInfos.reserve(lineSetMap.size());
    for (const auto& kvp : lineSetMap)
    {
        unsigned int size = kvp.second.size();
        assert(size == 0 || size > 1);
        if (size > 1)
        {
            EdgeInfo edgeInfo;
            edgeInfo.numLines = size - 1;
            data.edgeInfos.emplace_back(edgeInfo);
            numOfLineIndices += (size - 1) * 2;
        }
    }
    data.lineIndices->resize(numOfLineIndices);
    unsigned int idxLine(0);
    for (const auto& kvp : lineSetMap)
    {
        const std::vector<int>& indices = kvp.second;
        if (indices.size() <= 1) continue;
        for (size_t i = 0; i < indices.size() - 1; ++i)
        {
            (*data.lineIndices)[idxLine++] = indices[i];
            (*data.lineIndices)[idxLine++] = indices[i + 1];
        }
    }
    assert(idxLine == numOfLineIndices);

    return true;
}

void SheetElementNode::highlightImpl(bool flag)
{
    this->updateColorAndTransparent();
}

void SheetElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }

    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->setPreview(flag);
    }
}

void SheetElementNode::setActiveImpl(bool flag)
{
    this->updateColorAndTransparent();
}

bool SheetElementNode::updateApperance(wydb::Database* pDb)
{
    if (!ElementNode::updateApperance(pDb))
    {
        assert(false);
        return false;
    }

    assert(pDb);
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(_id));
    if (!pSheet)
    {
        return false;
    }

    const wy3d::Color& sheetColor = pSheet->getColor();
    constexpr float inv255 = 1.0f / 255.0f;
    _sheetFaceColor.set(
        static_cast<float>(sheetColor.red) * inv255,
        static_cast<float>(sheetColor.green) * inv255,
        static_cast<float>(sheetColor.blue) * inv255,
        1.0f);
    this->updateColorAndTransparent();
    return true;
}

void SheetElementNode::updateColorAndTransparent()
{
    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->applyAppearance(this->makeStyle());
    }
}

unsigned int SheetElementNode::getFaceIndex(unsigned int primitiveIndex) const
{
    const MeshRenderData* data = this->renderData();
    if (!data || data->faceInfos.empty()) return static_cast<unsigned int>(-1);

    unsigned int faceIndex(-1);
    unsigned int count(0);
    for (auto iter = data->faceInfos.cbegin(); iter != data->faceInfos.cend(); ++iter)
    {
        ++faceIndex;
        count += iter->numTriangles;
        if (primitiveIndex < count)
        {
            return faceIndex;
        }
    }

    return static_cast<unsigned int>(-1);
}

unsigned int SheetElementNode::getEdgeIndex(unsigned int primitiveIndex) const
{
    const MeshRenderData* data = this->renderData();
    if (!data || data->edgeInfos.empty()) return static_cast<unsigned int>(-1);

    unsigned int edgeIndex(-1);
    unsigned int count(0);
    for (auto iter = data->edgeInfos.cbegin(); iter != data->edgeInfos.cend(); ++iter)
    {
        ++edgeIndex;
        count += iter->numLines;
        if (primitiveIndex < count)
        {
            return edgeIndex;
        }
    }

    return static_cast<unsigned int>(-1);
}

wyap::Selection SheetElementNode::createSelection(
    osg::Drawable* pDrawable,
    unsigned int primitiveIndex,
    wy3d::SelectionType selType) const
{
    switch (PickCommon::getDrawableMode(pDrawable))
    {
    case DrawMode::Face:
    {
        if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Face))
        {
            unsigned int faceIndex = this->getFaceIndex(primitiveIndex);
            if (-1 == faceIndex)
            {
                assert(false);
                return wyap::Selection(wydb::ElementId::kNull);
            }
            return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::Face),
                this->getElementId(), std::to_string(faceIndex));
        }
        else if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Element))
        {
            return wyap::Selection(this->getElementId());
        }
        return wyap::Selection(wydb::ElementId::kNull);
    }
    break;

    case DrawMode::Edge:
    {
        if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Edge))
        {
            unsigned int edgeIndex = this->getEdgeIndex(primitiveIndex);
            if (-1 == edgeIndex)
            {
                assert(false);
                return wyap::Selection(wydb::ElementId::kNull);
            }
            return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::Edge),
                this->getElementId(), std::to_string(edgeIndex));
        }
        else if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Element))
        {
            return wyap::Selection(this->getElementId());
        }
        return wyap::Selection(wydb::ElementId::kNull);
    }
    break;

    case DrawMode::Vertex:
    {
        if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Element))
        {
            return wyap::Selection(this->getElementId());
        }
        return wyap::Selection(wydb::ElementId::kNull);
    }
    break;

    default:
    {
        assert(false);
        return wyap::Selection(wydb::ElementId::kNull);
    }
    break;
    }
}

void SheetElementNode::highlightFace(unsigned int faceIndex, bool flag)
{
    MeshRenderData* data = this->renderData();
    if (!data || faceIndex >= data->faceInfos.size())
    {
        // 形体重建后(如删除面)选择集里可能残留旧序号, 取消高亮时它已不存在, 无需处理
        assert(!flag);
        return;
    }

    MeshRenderObject* obj = this->renderObject();
    if (!obj)
    {
        assert(false);
        return;
    }
    obj->highlightFace(faceIndex, flag);
    if (obj->isNoBatchFace()) this->addFlag(Flag::NoBatchFace);
}

void SheetElementNode::highlightFace(unsigned int faceIndex, const osg::Vec4& color)
{
    MeshRenderData* data = this->renderData();
    if (!data || faceIndex >= data->faceInfos.size())
    {
        assert(false);
        return;
    }

    MeshRenderObject* obj = this->renderObject();
    if (!obj)
    {
        assert(false);
        return;
    }
    obj->highlightFace(faceIndex, color);
    if (obj->isNoBatchFace()) this->addFlag(Flag::NoBatchFace);
}

void SheetElementNode::previewFace(unsigned int faceIndex, bool flag)
{
    MeshRenderData* data = this->renderData();
    if (!data || faceIndex >= data->faceInfos.size())
    {
        return;
    }

    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->previewFace(faceIndex, flag);
        if (obj->isNoBatchFace()) this->addFlag(Flag::NoBatchFace);
    }
}

void SheetElementNode::highlightEdge(unsigned int edgeIndex, bool flag)
{
    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->highlightEdge(edgeIndex, flag);
    }
    else
    {
        assert(false);
    }
}

void SheetElementNode::previewEdge(unsigned int edgeIndex, bool flag)
{
    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->previewEdge(edgeIndex, flag);
    }
    else
    {
        assert(false);
    }
}

void SheetElementNode::clearDynamicRenderGeometry()
{
    if (MeshRenderObject* obj = this->renderObject())
    {
        obj->clearDynamicRenderGeometry();
    }
    this->removeFlag(Flag::NoBatchFace);
}

osg::Vec4 SheetElementNode::getFaceDefaultColor() const
{
    return _sheetFaceColor;
}

osg::Vec4 SheetElementNode::getEdgeDefaultColor() const
{
    return Colors::kSheetEdge;
}

} // namespace wy3dApp
