///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "headers.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <memory>
#include <vector>

#include <wy3dOffsetSheet.h>
#include <utils/wy3dSheetOffsetUtil.h>
#include <wy3dDeleteFace.h>
#include <wy3dFillet.h>
#include <wy3dFilledSheet.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dBox.h>
#include <wy3dSheet.h>
#include <wy3dSolid.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dParamEnumDef.h>
#include <wy3dParamNames.h>

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <Bnd_Box.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Pnt.hxx>

// OffsetSheet appends: it hangs off a sheet's modification chain and leaves the sheet's own faces
// untouched, adding an offset copy of what it was pointed at beside them. Pointed at the whole
// sheet, that copy is the whole sheet; pointed at a few faces, only those faces come back - as one
// shell when they touch, as several when they do not. A sheet has no thickness to offset into, so
// it is the only host the feature accepts, the way DeleteFace is sheet-only too.
//
// A 100 x 50 rectangle sketch fills to a sheet of one face and 5000 of area; extruding it 10 deep
// gives the four walls without caps, 3000 of area (300 perimeter * 10): two of 1000 (x = 0, x =
// 100) and two of 500 (y = 0, y = 50). A wall is adjacent to the two walls next to it and opposite
// the one across the rectangle.

namespace
{
    static std::size_t countElements(wy3d::Database* pDb)
    {
        std::size_t n(0);
        wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
        while (!iter.isDone())
        {
            const wydb::Element* pElem = pDb->getElement(iter.current());
            if (pElem && !pElem->isErased())
            {
                ++n;
            }
            iter.moveNext();
        }
        return n;
    }

    static std::uint32_t getChainErrorCode(wy3d::Database* pDb, const wydb::ElementId& id)
    {
        return wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(id).get());
    }

    static int countFaces(const TopoDS_Shape& shape)
    {
        int n(0);
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
        {
            ++n;
        }
        return n;
    }

    static int countShells(const TopoDS_Shape& shape)
    {
        int n(0);
        for (TopExp_Explorer exp(shape, TopAbs_SHELL); exp.More(); exp.Next())
        {
            ++n;
        }
        return n;
    }

    static double bodyArea(const TopoDS_Shape& shape)
    {
        GProp_GProps gprops;
        BRepGProp::SurfaceProperties(shape, gprops);
        return gprops.Mass();
    }

    // Index, as getShape() returns the faces, of the planar face perpendicular to the given
    // axis (0 = X, 1 = Y, 2 = Z) whose centre of mass sits at coord along that same axis
    static std::uint32_t findFaceIndexAt(const TopoDS_Shape& shape, int axis, double coord)
    {
        assert(0 <= axis && axis <= 2);

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            const TopoDS_Face& face = TopoDS::Face(faces(i));
            BRepAdaptor_Surface surface(face);
            if (GeomAbs_Plane != surface.GetType())
            {
                continue;
            }

            const gp_Dir& dir = surface.Plane().Axis().Direction();
            const double dirs[3] = { dir.X(), dir.Y(), dir.Z() };
            if (std::fabs(dirs[axis]) < 1.0 - 1e-9)
            {
                continue;
            }

            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            const gp_Pnt& centre = props.CentreOfMass();
            const double centres[3] = { centre.X(), centre.Y(), centre.Z() };
            if (std::fabs(centres[axis] - coord) > 1e-6)
            {
                continue;
            }

            return static_cast<std::uint32_t>(i - 1);
        }
        return UINT_MAX;
    }

    // Every face/edge of the built shape must resolve a topo name, or picking cannot find it
    template<typename Body>
    static void expectAllTopoNamed(const Body* pBody)
    {
        const wy3d::TopoNaming* pTopoNaming = pBody->getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);
        const TopoDS_Shape& shape = pBody->getShape();

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        EXPECT_GT(faces.Extent(), 0);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            EXPECT_FALSE(pTopoNaming->getTopoName(faces(i)).empty()) << "face " << i;
        }

        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        EXPECT_GT(edges.Extent(), 0);
        for (int i = 1; i <= edges.Extent(); ++i)
        {
            EXPECT_FALSE(pTopoNaming->getTopoName(edges(i)).empty()) << "edge " << i;
        }
    }

    // The topo names of the faces, in the order getShape() hands them out
    static std::vector<wy3d::TopoName> faceNames(const wy3d::Sheet* pSheet)
    {
        std::vector<wy3d::TopoName> names;
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            names.emplace_back(pSheet->getTopoNaming()->getTopoName(faces(i)));
        }
        return names;
    }

    // The topo names of the edges, in the order getShape() hands them out
    static std::vector<wy3d::TopoName> edgeNames(const wy3d::Sheet* pSheet)
    {
        std::vector<wy3d::TopoName> names;
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        for (int i = 1; i <= edges.Extent(); ++i)
        {
            names.emplace_back(pSheet->getTopoNaming()->getTopoName(edges(i)));
        }
        return names;
    }

    // The faces that were there before must still be at the front of the new shape, under the
    // very names they had: that is what the rest of the chain hangs off
    static void expectFaceNamesUnchanged(const wy3d::Sheet* pSheet,
        const std::vector<wy3d::TopoName>& namesBefore)
    {
        const std::vector<wy3d::TopoName> namesAfter = faceNames(pSheet);
        ASSERT_GE(namesAfter.size(), namesBefore.size());
        for (std::size_t i = 0; i < namesBefore.size(); ++i)
        {
            EXPECT_EQ(namesAfter[i], namesBefore[i]) << "face " << i;
        }
    }

    // Create a 2D sketch on the plane z = 0 with a 100x50 closed rectangle
    static wydb::ElementId createRectSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine* pLines[4] = { nullptr, nullptr, nullptr, nullptr };
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 0.0), wy::Vector2(100.0, 50.0), pLines[1]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 50.0), wy::Vector2(0.0, 50.0), pLines[2]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 50.0), wy::Vector2(0.0, 0.0), pLines[3]), wy::ErrorStatus::Ok);
            for (wy3d::SketchLine* pLine : pLines)
            {
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // A sheet of a single face: the rectangle filled
    static wydb::ElementId createFilledSheet(wy3d::Database* pDb, const wydb::ElementId& sketchId)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::FilledSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // The rectangle extruded 10 deep: four walls, no caps
    static wydb::ElementId createExtrudedSheet(wy3d::Database* pDb, const wydb::ElementId& sketchId, double depth)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::ExtrudedSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::ExtrudedSheet::create(pTrans, pSketch, depth, pSheet), wy::ErrorStatus::Ok);
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    static wydb::ElementId createNonParametricSheet(wy3d::Database* pDb, const TopoDS_Shape& shape)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::NonParametricSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::NonParametricSheet::create(pTrans, shape, pSheet), wy::ErrorStatus::Ok);
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // Fillet the first edge that has two faces on it: the vertical corners of an extruded sheet
    static wy3d::Fillet* filletSheetEdge(wy3d::Database* pDb, const wydb::ElementId& sheetId, double radius)
    {
        wy3d::Fillet* pFillet(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }

            TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
            TopExp::MapShapesAndAncestors(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_EDGE,
                TopAbs_ShapeEnum::TopAbs_FACE, edgeFaceMap);
            std::uint32_t edgeIndex(UINT_MAX);
            for (int i = 1; i <= edgeFaceMap.Extent(); ++i)
            {
                if (2 == edgeFaceMap.FindFromIndex(i).Extent())
                {
                    edgeIndex = static_cast<std::uint32_t>(i - 1);
                    break;
                }
            }
            if (UINT_MAX == edgeIndex)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }

            EXPECT_EQ(wy3d::Fillet::create(pTrans, pSheet, {}, { edgeIndex }, radius, pFillet),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pFillet;
    }

    // The sheet's blend face, the only one of them that is not a plane
    static std::uint32_t findBlendFaceIndex(const wy3d::Sheet* pSheet)
    {
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            BRepAdaptor_Surface surface(TopoDS::Face(faces(i)));
            if (GeomAbs_Cylinder == surface.GetType()) return static_cast<std::uint32_t>(i - 1);
        }
        return UINT_MAX;
    }

    // The indices of the faces the given one runs into along its edges
    static std::vector<std::uint32_t> findFacesAround(const wy3d::Sheet* pSheet, std::uint32_t faceIndex)
    {
        std::vector<std::uint32_t> indices;
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        if (faceIndex >= static_cast<std::uint32_t>(faces.Extent())) return indices;

        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_EDGE,
            TopAbs_ShapeEnum::TopAbs_FACE, edgeFaceMap);
        TopTools_IndexedMapOfShape faceEdges;
        TopExp::MapShapes(faces(faceIndex + 1), TopAbs_ShapeEnum::TopAbs_EDGE, faceEdges);
        for (int i = 1; i <= faceEdges.Extent(); ++i)
        {
            if (!edgeFaceMap.Contains(faceEdges(i))) continue;
            for (TopTools_ListIteratorOfListOfShape iter(edgeFaceMap.FindFromKey(faceEdges(i))); iter.More(); iter.Next())
            {
                for (int j = 1; j <= faces.Extent(); ++j)
                {
                    const std::uint32_t index = static_cast<std::uint32_t>(j - 1);
                    if (index == faceIndex) continue;
                    if (!iter.Value().IsSame(faces(j))) continue;
                    if (std::find(indices.cbegin(), indices.cend(), index) != indices.cend()) continue;
                    indices.emplace_back(index);
                }
            }
        }
        return indices;
    }

    // The blend among the faces the offset appended
    static TopoDS_Face findAppendedBlendFace(const wy3d::Sheet* pSheet, int keptFaceCount)
    {
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = keptFaceCount + 1; i <= faces.Extent(); ++i)
        {
            BRepAdaptor_Surface surface(TopoDS::Face(faces(i)));
            if (GeomAbs_Cylinder == surface.GetType()) return TopoDS::Face(faces(i));
        }
        return TopoDS_Face();
    }

    // A sketch with two separate loops: the 100x50 rectangle drawn counter-clockwise and a regular
    // pentagon of radius 15, drawn the way the caller asks for
    static wydb::ElementId createTwoLoopSketch(wy3d::Database* pDb, bool pentagonClockwise)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine* pLines[4] = { nullptr, nullptr, nullptr, nullptr };
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 0.0), wy::Vector2(100.0, 50.0), pLines[1]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 50.0), wy::Vector2(0.0, 50.0), pLines[2]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 50.0), wy::Vector2(0.0, 0.0), pLines[3]), wy::ErrorStatus::Ok);
            for (wy3d::SketchLine* pLine : pLines)
            {
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            const double pi = 3.14159265358979323846;
            const double centreX(200.0), centreY(25.0), radius(15.0);
            wy::Vector2 previous;
            for (int i = 0; i <= 5; ++i)
            {
                const double angle = pi / 2.0 + (pentagonClockwise ? -1.0 : 1.0) * 2.0 * pi * i / 5.0;
                const wy::Vector2 point(centreX + radius * std::cos(angle), centreY + radius * std::sin(angle));
                if (0 == i)
                {
                    previous = point;
                    continue;
                }

                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, previous, point, pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
                previous = point;
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // Signed volume of every shell the shape hands out, in that same order. A prism shell has no
    // caps, so only its walls weigh in and the sign is a position-independent reading of the way
    // the shell faces: positive means the faces look away from the loop they were swept from.
    static std::vector<double> shellVolumes(const TopoDS_Shape& shape)
    {
        std::vector<double> volumes;
        for (TopExp_Explorer exp(shape, TopAbs_SHELL); exp.More(); exp.Next())
        {
            GProp_GProps props;
            BRepGProp::VolumeProperties(exp.Current(), props);
            volumes.emplace_back(props.Mass());
        }
        return volumes;
    }

    // Diagonal of the bounding box of every shell, in the same order: this is what tells an offset
    // that went inwards from one that went out
    static std::vector<double> shellBoxSizes(const TopoDS_Shape& shape)
    {
        std::vector<double> sizes;
        for (TopExp_Explorer exp(shape, TopAbs_SHELL); exp.More(); exp.Next())
        {
            Bnd_Box box;
            BRepBndLib::Add(exp.Current(), box);
            const gp_Pnt& cornerMin = box.CornerMin();
            const gp_Pnt& cornerMax = box.CornerMax();
            sizes.emplace_back(cornerMin.Distance(cornerMax));
        }
        return sizes;
    }

    // Extent of a shape along one axis (0 = X, 1 = Y, 2 = Z)
    static void shapeExtent(const TopoDS_Shape& shape, int axis, double& minValue, double& maxValue)
    {
        assert(0 <= axis && axis <= 2);

        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        const gp_Pnt& cornerMin = box.CornerMin();
        const gp_Pnt& cornerMax = box.CornerMax();
        const double mins[3] = { cornerMin.X(), cornerMin.Y(), cornerMin.Z() };
        const double maxs[3] = { cornerMax.X(), cornerMax.Y(), cornerMax.Z() };
        minValue = mins[axis];
        maxValue = maxs[axis];
    }

    // Offset the whole sheet
    static wy3d::OffsetSheet* offsetWholeSheet(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        double offset)
    {
        wy3d::OffsetSheet* pOffset(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::OffsetSheet::create(pTrans, pSheet, offset, pOffset), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pOffset;
    }

    // Offset the given faces of the sheet
    static wy3d::OffsetSheet* offsetFaces(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const std::vector<std::uint32_t>& faceIndices, double offset)
    {
        wy3d::OffsetSheet* pOffset(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::OffsetSheet::create(pTrans, pSheet, faceIndices, offset, pOffset),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pOffset;
    }

    static wy3d::DeleteFace* deleteFace(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const std::vector<std::uint32_t>& faceIndices)
    {
        wy3d::DeleteFace* pDeleteFace(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::DeleteFace::create(pTrans, pSheet, faceIndices, pDeleteFace), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pDeleteFace;
    }
}

// --- The whole sheet ---

TEST(OffsetSheet, WholeSheetAppendsOffsetCopy)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);
    ASSERT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
    const std::vector<wy3d::TopoName> namesBefore = faceNames(pSheet);

    wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 3.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
    EXPECT_EQ(pOffset->getTarget(), wy3d::OffsetSheet::Target::WholeSheet);
    EXPECT_TRUE(pOffset->getFaceNames().empty());

    // The sheet's one face is still there and a whole offset copy has joined it
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 10000.0, 1e-6);

    // Same names, same indices: everything downstream of the sheet hangs off the old one
    expectFaceNamesUnchanged(pSheet, namesBefore);
    expectAllTopoNamed(pSheet);
}

TEST(OffsetSheet, WholeSheetOfExtrudedSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 4);

    const std::vector<wy3d::TopoName> namesBefore = faceNames(pSheet);
    const std::vector<wy3d::TopoName> edgeNamesBefore = edgeNames(pSheet);

    wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 3.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 8);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    expectFaceNamesUnchanged(pSheet, namesBefore);

    // The four walls are still the first four faces of the body - the sheet's own shape leads the
    // compound, so what the rest of the chain has indexed keeps its place
    TopTools_IndexedMapOfShape facesAfter;
    TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, facesAfter);
    ASSERT_GE(facesAfter.Extent(), 4);
    for (int i = 1; i <= 4; ++i)
    {
        const wy3d::TopoName& name = pSheet->getTopoNaming()->getTopoName(facesAfter(i));
        EXPECT_EQ(name, namesBefore[i - 1]) << "face " << i;
    }
    expectAllTopoNamed(pSheet);

    // Every edge of the offset copy is named after the edge it came from - the kernel hands the
    // comparer exactly one ancestor edge per new edge - and not after the new faces that meet
    // there. The old shape leads the compound, so its edges come first and the copy's follow.
    std::vector<wy3d::TopoName> expectedEdgeNames;
    expectedEdgeNames.reserve(edgeNamesBefore.size());
    for (const wy3d::TopoName& nameBefore : edgeNamesBefore)
    {
        expectedEdgeNames.emplace_back(nameBefore + "+@" + std::to_string(pOffset->getId().value()));
    }
    std::sort(expectedEdgeNames.begin(), expectedEdgeNames.end());

    TopTools_IndexedMapOfShape edgesAfter;
    TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_EDGE, edgesAfter);
    ASSERT_EQ(edgesAfter.Extent(), 2 * static_cast<int>(edgeNamesBefore.size()));
    std::vector<wy3d::TopoName> offsetEdgeNames;
    offsetEdgeNames.reserve(edgeNamesBefore.size());
    for (int i = static_cast<int>(edgeNamesBefore.size()) + 1; i <= edgesAfter.Extent(); ++i)
    {
        offsetEdgeNames.emplace_back(pSheet->getTopoNaming()->getTopoName(edgesAfter(i)));
    }
    std::sort(offsetEdgeNames.begin(), offsetEdgeNames.end());
    EXPECT_EQ(offsetEdgeNames, expectedEdgeNames);
}

// --- A few faces of the sheet ---

TEST(OffsetSheet, FaceSubsetSingleWall)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t wallIndex = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    ASSERT_NE(wallIndex, UINT_MAX);

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { wallIndex }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
    EXPECT_EQ(pOffset->getTarget(), wy3d::OffsetSheet::Target::SelectedFaces);
    EXPECT_EQ(pOffset->getFaceNames().size(), 1u);

    // Just that one wall came back, as a shell of its own
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    expectAllTopoNamed(pSheet);
}

TEST(OffsetSheet, FaceSubsetAdjacentWalls)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t wallX = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    const std::uint32_t wallY = findFaceIndexAt(pSheet->getShape(), 1, 0.0);
    ASSERT_NE(wallX, UINT_MAX);
    ASSERT_NE(wallY, UINT_MAX);

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { wallX, wallY }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
    EXPECT_EQ(pOffset->getFaceNames().size(), 2u);

    // Two walls that meet come back as one shell of two faces, not as two loose sheets
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    expectAllTopoNamed(pSheet);
}

TEST(OffsetSheet, FaceSubsetOppositeWalls)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t wallX0 = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    const std::uint32_t wallX100 = findFaceIndexAt(pSheet->getShape(), 0, 100.0);
    ASSERT_NE(wallX0, UINT_MAX);
    ASSERT_NE(wallX100, UINT_MAX);

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { wallX0, wallX100 }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);

    // Two walls that do not meet come back as two shells of their own
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    EXPECT_EQ(countShells(pSheet->getShape()), 3);
    expectAllTopoNamed(pSheet);
}

// A blend face sits between two walls it runs into smoothly. Offsetting it on its own carries the
// band along and leaves it there: every edge of the region is free, so nothing trims or extends the
// band, and it keeps the parallels of its own tangent edges as its boundary - it no longer meets
// the walls, which were not offset. Put those two walls in the region and the junctions come with
// them: the band is then trimmed against the offset of each wall, which is what closes the corner
// up. The band itself comes out the same surface either way.
TEST(OffsetSheet, BlendFaceAloneCarriesNoJoins)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);
    ASSERT_NE(filletSheetEdge(pDb.get(), sheetId, 5.0), nullptr);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const int keptFaceCount = countFaces(pSheet->getShape());
    ASSERT_EQ(keptFaceCount, 5);
    const std::uint32_t blendIndex = findBlendFaceIndex(pSheet);
    ASSERT_NE(blendIndex, UINT_MAX);
    ASSERT_EQ(findFacesAround(pSheet, blendIndex).size(), 2u);

    TopTools_IndexedMapOfShape keptFaces;
    TopExp::MapShapes(pSheet->getShape(), TopAbs_FACE, keptFaces);
    BRepAdaptor_Surface bandSurface(TopoDS::Face(keptFaces(static_cast<int>(blendIndex) + 1)));
    ASSERT_EQ(bandSurface.GetType(), GeomAbs_Cylinder);

    ASSERT_NE(offsetFaces(pDb.get(), sheetId, { blendIndex }, 3.0), nullptr);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);

    const TopoDS_Face bandAlone = findAppendedBlendFace(pSheet, keptFaceCount);
    ASSERT_FALSE(bandAlone.IsNull());
    BRepAdaptor_Surface aloneSurface(bandAlone);
    EXPECT_EQ(aloneSurface.GetType(), GeomAbs_Cylinder);
    // A positive distance moves the band off its own surface, away from the axis it bends around
    EXPECT_NEAR(aloneSurface.Cylinder().Radius(), bandSurface.Cylinder().Radius() + 3.0, 1e-6);
    EXPECT_NEAR(aloneSurface.Cylinder().Axis().Location().X(),
        bandSurface.Cylinder().Axis().Location().X(), 1e-6);
    EXPECT_NEAR(aloneSurface.Cylinder().Axis().Location().Y(),
        bandSurface.Cylinder().Axis().Location().Y(), 1e-6);
    GProp_GProps aloneProps;
    BRepGProp::SurfaceProperties(bandAlone, aloneProps);

    // The same blend, now with the two walls it runs into
    std::unique_ptr<wy3d::Database> pDb2 = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId2 = createRectSketch(pDb2.get());
    wydb::ElementId sheetId2 = createExtrudedSheet(pDb2.get(), sketchId2, 10.0);
    ASSERT_NE(filletSheetEdge(pDb2.get(), sheetId2, 5.0), nullptr);

    const wy3d::Sheet* pSheet2 = wy3d::Sheet::cast(pDb2->getElement(sheetId2));
    ASSERT_NE(pSheet2, nullptr);
    const int keptFaceCount2 = countFaces(pSheet2->getShape());
    const std::uint32_t blendIndex2 = findBlendFaceIndex(pSheet2);
    ASSERT_NE(blendIndex2, UINT_MAX);
    std::vector<std::uint32_t> region = findFacesAround(pSheet2, blendIndex2);
    ASSERT_EQ(region.size(), 2u);
    region.emplace_back(blendIndex2);

    ASSERT_NE(offsetFaces(pDb2.get(), sheetId2, region, 3.0), nullptr);
    pSheet2 = wy3d::Sheet::cast(pDb2->getElement(sheetId2));
    ASSERT_NE(pSheet2, nullptr);
    EXPECT_EQ(countFaces(pSheet2->getShape()), 8);
    EXPECT_EQ(countShells(pSheet2->getShape()), 2);

    const TopoDS_Face bandWithWalls = findAppendedBlendFace(pSheet2, keptFaceCount2);
    ASSERT_FALSE(bandWithWalls.IsNull());
    BRepAdaptor_Surface withWallsSurface(bandWithWalls);
    GProp_GProps withWallsProps;
    BRepGProp::SurfaceProperties(bandWithWalls, withWallsProps);

    // The band is the same surface down to its centroid; what the walls add is the two faces that
    // meet it, and that is the whole of the difference between the two results
    EXPECT_NEAR(withWallsSurface.Cylinder().Radius(), aloneSurface.Cylinder().Radius(), 1e-9);
    EXPECT_NEAR(withWallsProps.Mass(), aloneProps.Mass(), 1e-6);
    EXPECT_NEAR(withWallsProps.CentreOfMass().X(), aloneProps.CentreOfMass().X(), 1e-6);
    EXPECT_NEAR(withWallsProps.CentreOfMass().Y(), aloneProps.CentreOfMass().Y(), 1e-6);
    expectAllTopoNamed(pSheet2);
}

// --- The two loops of one sketch ---

// The direction an offset takes is the normal of the face as its shell carries it, and a shell
// carries the direction the loop was drawn in. ExtrudedSheet sweeps one prism per loop and used to
// leave the drawn winding alone, so a rectangle drawn counter-clockwise and a pentagon drawn the
// other way came out facing opposite ways: the whole-body offset took one inwards and the other
// outwards. The profile normalizes closed loops now, so both come the same way - outwards, and with
// the pentagon drawn either way the result is the same.
TEST(OffsetSheet, WholeSheetWithOppositeWoundLoops)
{
    auto runCase = [](bool pentagonClockwise, const std::vector<double>& expectedVolumes)
    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::ElementId sketchId = createTwoLoopSketch(pDb.get(), pentagonClockwise);
        ASSERT_FALSE(sketchId.isNull());
        wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        ASSERT_EQ(countShells(pSheet->getShape()), 2);
        ASSERT_EQ(shellVolumes(pSheet->getShape()).size(), 2u);
        const std::vector<double> volumesBefore = shellVolumes(pSheet->getShape());
        const std::vector<double> boxesBefore = shellBoxSizes(pSheet->getShape());
        EXPECT_NEAR(volumesBefore[0], expectedVolumes[0], 1.0e-3);
        EXPECT_NEAR(volumesBefore[1], expectedVolumes[1], 1.0e-3);

        wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 3.0);
        ASSERT_NE(pOffset, nullptr);
        EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);

        pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 18);
        ASSERT_EQ(countShells(pSheet->getShape()), 4);
        const std::vector<double> volumesAfter = shellVolumes(pSheet->getShape());
        const std::vector<double> boxesAfter = shellBoxSizes(pSheet->getShape());

        // Both loops went the same way: outwards, whatever direction they were drawn in, so a
        // positive distance grew each of them
        EXPECT_GT(volumesAfter[2], 0.0);
        EXPECT_GT(volumesAfter[3], 0.0);
        EXPECT_GT(boxesAfter[2], boxesBefore[0]);
        EXPECT_GT(boxesAfter[3], boxesBefore[1]);
        expectAllTopoNamed(pSheet);
    };

    // 2 x area x height / 3: 100x50x10 rectangle and 5/2 r^2 sin 72 pentagon, both swept 10 deep
    const double rectangleVolume = 2.0 * 100.0 * 50.0 * 10.0 / 3.0; // +33333.333
    const double pentagonVolume = 2.0 * 534.9693 * 10.0 / 3.0;       // +3566.462
    runCase(false, { rectangleVolume, pentagonVolume });
    runCase(true, { rectangleVolume, pentagonVolume });
}

TEST(OffsetSheet, RepeatedWholeSheetOffset)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    ASSERT_NE(offsetWholeSheet(pDb.get(), sheetId, 3.0), nullptr);
    ASSERT_NE(offsetWholeSheet(pDb.get(), sheetId, 3.0), nullptr);

    // The second one offsets what the first left behind, additions included: a sheet becomes a
    // compound, and a whole-body offset of a compound takes every shell in it
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 4);
    expectAllTopoNamed(pSheet);
}

// --- Failing updates ---

TEST(OffsetSheet, FaceNotExistsFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t wallIndex = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    ASSERT_NE(wallIndex, UINT_MAX);

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { wallIndex }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 5);
    ASSERT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);

    // The run that just succeeded recorded the faces it generated, so the failing one below has
    // something to drop
    const std::vector<std::uint32_t> newFaceIndices = pOffset->getNewFaceIndices();
    ASSERT_EQ(newFaceIndices.size(), 1u);

    // Re-cut the profile: three sides instead of four, so the wall the offset was pointed at is
    // not among the ones the sheet brings back and its name cannot be generated any more
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        const std::vector<wydb::ElementId> entities = pSketch->getChildren();
        ASSERT_EQ(entities.size(), 4u);
        for (const wydb::ElementId& entityId : entities)
        {
            wydb::Element* pEntity = pTrans->getElementForWrite(entityId);
            ASSERT_NE(pEntity, nullptr);
            EXPECT_EQ(pEntity->erase(true), wy::ErrorStatus::Ok);
        }

        wy3d::SketchLine* pLines[3] = { nullptr, nullptr, nullptr };
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 0.0), wy::Vector2(0.0, 50.0), pLines[1]), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 50.0), wy::Vector2(0.0, 0.0), pLines[2]), wy::ErrorStatus::Ok);
        for (wy3d::SketchLine* pLine : pLines)
        {
            ASSERT_NE(pLine, nullptr);
            EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
        }
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::OFFSETSHEET_FaceNotExists));

    // The sheet keeps what the upstream gave it, with nothing of the offset added to it
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_EQ(faceNames(pSheet).size(), 3u);

    // A failed update must not keep the faces the previous run recorded: the shape here is the
    // upstream one and those faces are not in it any more
    EXPECT_TRUE(pOffset->getNewFaceIndices().empty());
}

// --- Bad arguments ---

TEST(OffsetSheet, NullArgsAndEmptyInput)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const std::size_t numElements = countElements(pDb.get());

    wy3d::OffsetSheet* pOffset(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);

        EXPECT_EQ(wy3d::OffsetSheet::create(nullptr, pSheet, 3.0, pOffset),
            wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::OffsetSheet::create(pTrans, nullptr, 3.0, pOffset),
            wy::ErrorStatus::NullElementPointer);
        // Faces mode with nothing picked, and a distance of zero, are both refused up front
        EXPECT_EQ(wy3d::OffsetSheet::create(pTrans, pSheet, std::vector<std::uint32_t>(), 3.0, pOffset),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(wy3d::OffsetSheet::create(pTrans, pSheet, 0.0, pOffset),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pOffset, nullptr);

        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), numElements);
}

// --- Downstream of the offset ---

TEST(OffsetSheet, DownstreamFeatureStillResolves)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t wallIndex = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    ASSERT_NE(wallIndex, UINT_MAX);

    wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 3.0);
    ASSERT_NE(pOffset, nullptr);
    ASSERT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);

    // A face of the sheet is still named and still where the caller left it, so a feature made
    // after the offset can go on working from it
    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId, { wallIndex });
    ASSERT_NE(pDeleteFace, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pDeleteFace->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 7);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    expectAllTopoNamed(pSheet);
}

TEST(OffsetSheet, EraseSheetErasesFeature)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 3.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_FALSE(pOffset->isErased());

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        EXPECT_EQ(pSheetWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pOffset->isErased());
}

// 命令的预览靠"擦旧建新"换目标(OffsetSheet 没有改目标的接口), 所以擦掉修改特征这条路必须干净:
// 宿主回到自己的形体, _modifications 里不留悬空 id (留了链上就是 assert)
TEST(OffsetSheet, ErasedFeatureLeavesTheHostClean)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::vector<wy3d::TopoName> namesBefore = faceNames(pSheet);

    wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 3.0);
    ASSERT_NE(pOffset, nullptr);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 8);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wydb::Element* pOffsetWrite = pTrans->getElementForWrite(pOffset->getId());
        ASSERT_NE(pOffsetWrite, nullptr);
        EXPECT_EQ(pOffsetWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_TRUE(pOffset->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    expectFaceNamesUnchanged(pSheet, namesBefore);
    expectAllTopoNamed(pSheet);
}

TEST(OffsetSheet, UndoRedo)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t wallIndex = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    ASSERT_NE(wallIndex, UINT_MAX);
    ASSERT_NE(offsetFaces(pDb.get(), sheetId, { wallIndex }, 3.0), nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 5);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    expectAllTopoNamed(pSheet);
}

// --- Files ---

TEST(OffsetSheet, IO)
{
    std::string filePath("./test_offset_sheet.wy3dt");
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId offsetId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId,
            { findFaceIndexAt(pSheet->getShape(), 0, 0.0) }, 3.0);
        ASSERT_NE(pOffset, nullptr);
        offsetId = pOffset->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::OffsetSheet* pOffset = wy3d::OffsetSheet::cast(pDb->getElement(offsetId));
        ASSERT_NE(pOffset, nullptr);
        EXPECT_EQ(pOffset->getParent(), sheetId);
        EXPECT_EQ(pOffset->getTarget(), wy3d::OffsetSheet::Target::SelectedFaces);
        EXPECT_EQ(pOffset->getFaceNames().size(), 1u);
        EXPECT_NEAR(pOffset->getOffset(), 3.0, 1e-9);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        std::vector<wydb::ElementId> children = pSheet->getChildren();
        EXPECT_NE(std::find(children.cbegin(), children.cend(), offsetId), children.cend());

        EXPECT_EQ(countFaces(pSheet->getShape()), 5);
        EXPECT_EQ(countShells(pSheet->getShape()), 2);
        expectAllTopoNamed(pSheet);
    }
}

// --- Faces of a solid offset into a standalone sheet ---
//
// Picking solid faces offsets them into one non-parametric sheet that stands on its own. The shape
// comes from SheetOffsetUtil::makeOffsetShape, which reads each face straight out of its source shape -
// the orientation there is what drives the offset direction - and never writes to the source. The
// command groups the picked faces by their owner and hangs a single sheet on the lot.

TEST(OffsetSheet, MakeOffsetShapeFromSolidFace)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const std::uint32_t topIndex = findFaceIndexAt(box, 2, 30.0);
    ASSERT_NE(topIndex, UINT_MAX);

    // A positive distance moves along the face normal, which points away from the material
    TopoDS_Shape offsetShape;
    ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { topIndex }, 3.0, offsetShape),
        wy3d::ErrorCode::NoError);
    ASSERT_EQ(offsetShape.ShapeType(), TopAbs_ShapeEnum::TopAbs_SHELL);
    EXPECT_EQ(countFaces(offsetShape), 1);

    double minValue(0.0);
    double maxValue(0.0);
    shapeExtent(offsetShape, 2, minValue, maxValue);
    EXPECT_NEAR(minValue, 33.0, 1e-6);
    EXPECT_NEAR(maxValue, 33.0, 1e-6);
    shapeExtent(offsetShape, 0, minValue, maxValue);
    EXPECT_NEAR(minValue, 0.0, 1e-6);
    EXPECT_NEAR(maxValue, 10.0, 1e-6);

    ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { topIndex }, -3.0, offsetShape),
        wy3d::ErrorCode::NoError);
    shapeExtent(offsetShape, 2, minValue, maxValue);
    EXPECT_NEAR(minValue, 27.0, 1e-6);

    // A distance below the modelling tolerance is no offsetting at all: the picked face is copied as it is
    ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { topIndex }, 0.0, offsetShape),
        wy3d::ErrorCode::NoError);
    ASSERT_EQ(offsetShape.ShapeType(), TopAbs_ShapeEnum::TopAbs_SHELL);
    EXPECT_EQ(countFaces(offsetShape), 1);
    shapeExtent(offsetShape, 2, minValue, maxValue);
    EXPECT_NEAR(minValue, 30.0, 1e-6);
    EXPECT_NEAR(maxValue, 30.0, 1e-6);
    shapeExtent(offsetShape, 0, minValue, maxValue);
    EXPECT_NEAR(minValue, 0.0, 1e-6);
    EXPECT_NEAR(maxValue, 10.0, 1e-6);

    ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { topIndex }, 0.0005, offsetShape),
        wy3d::ErrorCode::NoError);
    shapeExtent(offsetShape, 2, minValue, maxValue);
    EXPECT_NEAR(minValue, 30.0, 1e-6);

    // The source is not touched
    EXPECT_EQ(countFaces(box), 6);
    shapeExtent(box, 2, minValue, maxValue);
    EXPECT_NEAR(maxValue, 30.0, 1e-6);
}

TEST(OffsetSheet, MakeOffsetShapeJoinsPickedFaces)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const std::uint32_t topIndex = findFaceIndexAt(box, 2, 30.0);
    const std::uint32_t sideIndex = findFaceIndexAt(box, 0, 10.0);
    ASSERT_NE(topIndex, UINT_MAX);
    ASSERT_NE(sideIndex, UINT_MAX);

    // Faces that share an edge come back as one shell of two faces...
    TopoDS_Shape offsetShape;
    ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { topIndex, sideIndex }, 3.0, offsetShape),
        wy3d::ErrorCode::NoError);
    ASSERT_EQ(offsetShape.ShapeType(), TopAbs_ShapeEnum::TopAbs_SHELL);
    EXPECT_EQ(countFaces(offsetShape), 2);

    // ...faces that do not touch, as one compound of shells
    const std::uint32_t bottomIndex = findFaceIndexAt(box, 2, 0.0);
    ASSERT_NE(bottomIndex, UINT_MAX);
    ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { topIndex, bottomIndex }, 3.0, offsetShape),
        wy3d::ErrorCode::NoError);
    EXPECT_EQ(offsetShape.ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countShells(offsetShape), 2);
    EXPECT_EQ(countFaces(offsetShape), 2);
}

TEST(OffsetSheet, MakeOffsetShapeErrors)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const std::uint32_t topIndex = findFaceIndexAt(box, 2, 30.0);
    ASSERT_NE(topIndex, UINT_MAX);

    TopoDS_Shape offsetShape;
    EXPECT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, {}, 3.0, offsetShape),
        wy3d::ErrorCode::OFFSETSHEET_NoFaceSelected);
    EXPECT_TRUE(offsetShape.IsNull());

    // A stale pick: the index is off the shape's own face map
    EXPECT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(box, { 999u }, 3.0, offsetShape),
        wy3d::ErrorCode::OFFSETSHEET_FaceNotExists);
    EXPECT_TRUE(offsetShape.IsNull());

    EXPECT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(TopoDS_Shape(), { topIndex }, 3.0, offsetShape),
        wy3d::ErrorCode::OFFSETSHEET_InvalidData);

    EXPECT_EQ(countFaces(box), 6);
}

TEST(OffsetSheet, SolidFacesGiveOneNonParametricSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // Two boxes of different heights, so their offset faces land at different levels
    wydb::ElementId boxIds[2] = { wydb::ElementId::kNull, wydb::ElementId::kNull };
    const double heights[2] = { 10.0, 20.0 };
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        for (int i = 0; i < 2; ++i)
        {
            wy3d::Box* pBox(nullptr);
            ASSERT_EQ(wy3d::Box::create(pTrans, heights[i], heights[i], heights[i], pBox),
                wy::ErrorStatus::Ok);
            ASSERT_NE(pBox, nullptr);
            boxIds[i] = pBox->getId();
        }
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const std::size_t elementsBefore = countElements(pDb.get());

    // What the command does: one offset per solid owner, all of them into one compound
    TopoDS_Shape sheetShape;
    {
        std::vector<TopoDS_Shape> shells;
        for (int i = 0; i < 2; ++i)
        {
            const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxIds[i]));
            ASSERT_NE(pSolid, nullptr);
            const std::uint32_t topIndex = findFaceIndexAt(pSolid->getShape(), 2, heights[i]);
            ASSERT_NE(topIndex, UINT_MAX);

            TopoDS_Shape one;
            ASSERT_EQ(wy3d::SheetOffsetUtil::makeOffsetShape(pSolid->getShape(), { topIndex }, 3.0, one),
                wy3d::ErrorCode::NoError);
            for (TopExp_Explorer ex(one, TopAbs_SHELL); ex.More(); ex.Next())
            {
                shells.emplace_back(ex.Current());
            }
        }
        ASSERT_EQ(shells.size(), 2u);

        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        for (const TopoDS_Shape& shell : shells)
        {
            builder.Add(compound, shell);
        }
        sheetShape = compound;
    }

    wydb::ElementId sheetId(wydb::ElementId::kNull);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::NonParametricSheet* pSheet(nullptr);
        ASSERT_EQ(wy3d::NonParametricSheet::create(pTrans, sheetShape, pSheet), wy::ErrorStatus::Ok);
        ASSERT_NE(pSheet, nullptr);
        sheetId = pSheet->getId();
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    // One element for the lot, standing on its own rather than hanging off a solid
    EXPECT_EQ(countElements(pDb.get()), elementsBefore + 1);
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getParent().isNull());
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);

    std::vector<double> levels;
    for (TopExp_Explorer ex(pSheet->getShape(), TopAbs_FACE); ex.More(); ex.Next())
    {
        double minValue(0.0);
        double maxValue(0.0);
        shapeExtent(ex.Current(), 2, minValue, maxValue);
        levels.emplace_back(minValue);
    }
    ASSERT_EQ(levels.size(), 2u);
    std::sort(levels.begin(), levels.end());
    EXPECT_NEAR(levels[0], 13.0, 1e-6);
    EXPECT_NEAR(levels[1], 23.0, 1e-6);

    // Neither solid was written to
    for (int i = 0; i < 2; ++i)
    {
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxIds[i]));
        ASSERT_NE(pSolid, nullptr);
        EXPECT_EQ(countFaces(pSolid->getShape()), 6);
        double minValue(0.0);
        double maxValue(0.0);
        shapeExtent(pSolid->getShape(), 2, minValue, maxValue);
        EXPECT_NEAR(maxValue, heights[i], 1e-6);
    }

    // The sheet is a host like any other: offsetting it adds its own copy beside it
    wy3d::OffsetSheet* pOffset = offsetWholeSheet(pDb.get(), sheetId, 1.0);
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 4);
    expectAllTopoNamed(pSheet);
}

// --- Changing the distance on a non-parametric host ---

// A non-parametric sheet has no upstream feature to rebuild it, so the copy the offset added last
// time is still part of what the next update starts from. Reported 2026-09-17: every distance
// change left another offset face beside the ones already there.
TEST(OffsetSheet, DistanceChangeOnNonParametricHostKeepsOneCopy)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId filledId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pFilled = wy3d::Sheet::cast(pDb->getElement(filledId));
    ASSERT_NE(pFilled, nullptr);
    wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), pFilled->getShape());
    ASSERT_FALSE(sheetId.isNull());

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);
    ASSERT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { 0 }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    ASSERT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
    const wydb::ElementId offsetId = pOffset->getId();

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 2);
    ASSERT_NEAR(bodyArea(pSheet->getShape()), 10000.0, 1e-6);

    // The picked face with its one offset copy, however often the distance is changed
    for (double offset : { 5.0, 7.0, 4.0 })
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::OffsetSheet* pWrite = wy3d::OffsetSheet::cast(pTrans->getElementForWrite(offsetId));
        ASSERT_NE(pWrite, nullptr);
        pWrite->upgradeForWrite();
        pWrite->setOffset(offset);
        ASSERT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);

        pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 2) << "offset = " << offset;
        EXPECT_EQ(countShells(pSheet->getShape()), 2) << "offset = " << offset;
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 10000.0, 1e-6) << "offset = " << offset;
    }
}

// What a non-parametric host re-runs from is creation-time data, so undoing and redoing the
// feature stacked on top of it must leave both that base and the result alone
TEST(OffsetSheet, UndoRedoKeepsTheNonParametricBase)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId filledId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pFilled = wy3d::Sheet::cast(pDb->getElement(filledId));
    ASSERT_NE(pFilled, nullptr);
    wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), pFilled->getShape());
    ASSERT_FALSE(sheetId.isNull());

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { 0 }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    ASSERT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
    const wydb::ElementId offsetId = pOffset->getId();

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 2);

    // Undo takes the copy away, redo puts it back
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    ASSERT_EQ(pTransMgr->undo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);

    ASSERT_EQ(pTransMgr->redo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);

    // The base came through the round trip intact: another distance change still leaves one copy
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::OffsetSheet* pWrite = wy3d::OffsetSheet::cast(pTrans->getElementForWrite(offsetId));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setOffset(6.0), wy::ErrorStatus::Ok);
        ASSERT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 10000.0, 1e-6);
}

// Saving a non-parametric host only stores the shape it was made with; the copy the feature adds
// has to come back from the chain re-running on load, and a later distance change must still not
// stack onto it
TEST(OffsetSheet, ReloadKeepsTheNonParametricCopy)
{
    const std::string filePath("./test_offset_sheet_nonparam.wy3dt");
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId offsetId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::ElementId sketchId = createRectSketch(pDb.get());
        wydb::ElementId filledId = createFilledSheet(pDb.get(), sketchId);

        const wy3d::Sheet* pFilled = wy3d::Sheet::cast(pDb->getElement(filledId));
        ASSERT_NE(pFilled, nullptr);
        sheetId = createNonParametricSheet(pDb.get(), pFilled->getShape());
        ASSERT_FALSE(sheetId.isNull());

        wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { 0 }, 3.0);
        ASSERT_NE(pOffset, nullptr);
        ASSERT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
        offsetId = pOffset->getId();

        ASSERT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 2);

        // Changing the distance right after the reload still leaves the one copy
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::OffsetSheet* pWrite = wy3d::OffsetSheet::cast(pTrans->getElementForWrite(offsetId));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setOffset(6.0), wy::ErrorStatus::Ok);
        ASSERT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);

        pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 2);
        EXPECT_EQ(countShells(pSheet->getShape()), 2);
    }

    std::remove(filePath.c_str());
}


// The target is settled when the feature is created: the property panel shows it read-only and a
// write to it is refused, while the offset distance stays an ordinary editable parameter
TEST(OffsetSheet, TargetIsFixedAfterCreation)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createRectSketch(pDb.get());
    const wydb::ElementId filledId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pFilled = wy3d::Sheet::cast(pDb->getElement(filledId));
    ASSERT_NE(pFilled, nullptr);
    const wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), pFilled->getShape());

    wy3d::OffsetSheet* pOffset = offsetFaces(pDb.get(), sheetId, { 0 }, 3.0);
    ASSERT_NE(pOffset, nullptr);
    ASSERT_EQ(getChainErrorCode(pDb.get(), pOffset->getId()), 0u);
    EXPECT_EQ(pOffset->getTarget(), wy3d::OffsetSheet::Target::SelectedFaces);

    const std::string className = wy3d::OffsetSheet::classInfo()->className();

    // Read-only is what greys the combo box out in the panel
    bool foundTarget(false);
    for (const wydb::ParameterDefinition* pDef : pOffset->listParameters())
    {
        ASSERT_NE(pDef, nullptr);
        if (pDef->getName() != wy3d::ParamNames::OFFSETSHEET_PARAM_TARGET) continue;
        foundTarget = true;
        EXPECT_TRUE(pDef->isReadonly());
    }
    EXPECT_TRUE(foundTarget);

    const wy3d::Sheet* pHost = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pHost, nullptr);
    const int facesBefore = countFaces(pHost->getShape());

    {
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::OffsetSheet* pWrite = wy3d::OffsetSheet::cast(pTrans->getElementForWrite(pOffset->getId()));
        ASSERT_NE(pWrite, nullptr);

        // Offsetting the whole body instead of the picked face would be a different feature
        wy3d::ParamEnumDef enumDef(
            {{static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet), "Whole Sheet"},
             {static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces), "Selected Faces"}},
            static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet));
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::OFFSETSHEET_PARAM_TARGET,
            *wydb::ParameterValue::createAny(enumDef)), wy::ErrorStatus::ParameterReadonly);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::OFFSETSHEET_PARAM_TARGET,
            *wydb::ParameterValue::createInteger(static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet))),
            wy::ErrorStatus::ParameterReadonly);

        // The distance is an ordinary parameter and stays editable
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::OFFSETSHEET_PARAM_OFFSET,
            *wydb::ParameterValue::createDouble(5.0)), wy::ErrorStatus::Ok);

        ASSERT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(pOffset->getTarget(), wy3d::OffsetSheet::Target::SelectedFaces);
    EXPECT_DOUBLE_EQ(pOffset->getOffset(), 5.0);
    pHost = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_EQ(countFaces(pHost->getShape()), facesBefore);
}

