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

#include <wy3dFilledSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketch3D.h>
#include <wy3dSketch3DProfile.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchSpline3D.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>

#include <algorithm>
#include <cstdio>
#include <set>

namespace
{
    std::size_t countElements(wy3d::Database* pDb)
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

    // Create a 2D sketch on the XY plane with a 100x50 closed rectangle
    static wydb::ElementId createRectSketch(wy3d::Database* pDb)
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

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // The same 100x50 rectangle drawn the other way round: same region, opposite winding
    static wydb::ElementId createReversedRectSketch(wy3d::Database* pDb)
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

            const wy::Vector2 pts[4] = {
                wy::Vector2(0.0, 0.0), wy::Vector2(0.0, 50.0),
                wy::Vector2(100.0, 50.0), wy::Vector2(100.0, 0.0) };
            for (int i = 0; i < 4; ++i)
            {
                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, pts[i], pts[(i + 1) % 4], pLine), wy::ErrorStatus::Ok);
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

    // Two 2D triangles that do not touch: a boundary of two regions, not one closed loop
    static wydb::ElementId createTwoLoopSketch(wy3d::Database* pDb)
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

            const wy::Vector2 tri1[3] = {
                wy::Vector2(0.0, 0.0), wy::Vector2(10.0, 0.0), wy::Vector2(0.0, 10.0) };
            const wy::Vector2 tri2[3] = {
                wy::Vector2(50.0, 50.0), wy::Vector2(60.0, 50.0), wy::Vector2(50.0, 60.0) };
            for (const wy::Vector2* tri : { tri1, tri2 })
            {
                for (int i = 0; i < 3; ++i)
                {
                    wy3d::SketchLine* pLine(nullptr);
                    EXPECT_EQ(wy3d::SketchLine::create(pTrans, tri[i], tri[(i + 1) % 3], pLine), wy::ErrorStatus::Ok);
                    if (!pLine)
                    {
                        pDb->getTransactionManager()->abortTransaction();
                        return wydb::ElementId::kNull;
                    }
                    EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
                }
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with a single line
    static wydb::ElementId createLineSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine3D* pLine(nullptr);
            EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(100.0, 0.0, 0.0), pLine), wy::ErrorStatus::Ok);
            if (!pLine)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with a closed coplanar 100x100 square
    static wydb::ElementId createClosedSquareSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 pts[4] = {
                wy::Vector3(0.0, 0.0, 0.0),
                wy::Vector3(100.0, 0.0, 0.0),
                wy::Vector3(100.0, 100.0, 0.0),
                wy::Vector3(0.0, 100.0, 0.0) };
            for (int i = 0; i < 4; ++i)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, pts[i], pts[(i + 1) % 4], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with a single full circle (radius 25)
    static wydb::ElementId createCircleSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchCircle3D* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis, 25.0, pCircle), wy::ErrorStatus::Ok);
            if (!pCircle)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with a single full ellipse (major 25, minor 15)
    static wydb::ElementId createEllipseSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchEllipse3D* pEllipse(nullptr);
            EXPECT_EQ(wy3d::SketchEllipse3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis,
                25.0, 0.6, pEllipse), wy::ErrorStatus::Ok);
            if (!pEllipse)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pEllipse), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with an open ellipse arc only (quarter arc)
    static wydb::ElementId createOpenEllipseArcSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchEllipseArc3D* pEllipseArc(nullptr);
            EXPECT_EQ(wy3d::SketchEllipseArc3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis,
                25.0, 0.6, 0.0, wy3d::PI / 3.0, pEllipseArc), wy::ErrorStatus::Ok);
            if (!pEllipseArc)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pEllipseArc), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with a half-disc loop: the upper semicircle (radius 25)
    // closed by its chord line
    static wydb::ElementId createSemicircleSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            // 0 -> PI counterclockwise: (25, 0, 0) to (-25, 0, 0)
            wy3d::SketchArc3D* pArc(nullptr);
            EXPECT_EQ(wy3d::SketchArc3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis,
                25.0, 0.0, wy3d::PI, pArc), wy::ErrorStatus::Ok);
            if (!pArc)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pArc), wy::ErrorStatus::Ok);

            wy3d::SketchLine3D* pLine(nullptr);
            EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(-25.0, 0.0, 0.0), wy::Vector3(25.0, 0.0, 0.0), pLine), wy::ErrorStatus::Ok);
            if (!pLine)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with the open semicircle arc only
    static wydb::ElementId createOpenArcSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchArc3D* pArc(nullptr);
            EXPECT_EQ(wy3d::SketchArc3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis,
                25.0, 0.0, wy3d::PI, pArc), wy::ErrorStatus::Ok);
            if (!pArc)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pArc), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with a non-planar closed quad (BRepFill boundary)
    static wydb::ElementId createNonPlanarQuadSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 pts[4] = {
                wy::Vector3(0.0, 0.0, 0.0),
                wy::Vector3(100.0, 0.0, 0.0),
                wy::Vector3(100.0, 100.0, 50.0),
                wy::Vector3(0.0, 100.0, 0.0) };
            for (int i = 0; i < 4; ++i)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, pts[i], pts[(i + 1) % 4], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with an open chain (3 lines)
    static wydb::ElementId createOpenChainSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 pts[4] = {
                wy::Vector3(0.0, 0.0, 0.0),
                wy::Vector3(100.0, 0.0, 0.0),
                wy::Vector3(100.0, 100.0, 0.0),
                wy::Vector3(0.0, 100.0, 0.0) };
            for (int i = 0; i < 3; ++i)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with three lines sharing one endpoint
    static wydb::ElementId createBranchSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 startPnt(0.0, 0.0, 0.0);
            const wy::Vector3 endPnts[3] = {
                wy::Vector3(100.0, 0.0, 0.0),
                wy::Vector3(0.0, 100.0, 0.0),
                wy::Vector3(0.0, 0.0, 100.0) };
            for (int i = 0; i < 3; ++i)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, startPnt, endPnts[i], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 3D sketch with two disjoint closed triangles
    static wydb::ElementId createTwoLoopSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 tri1[3] = {
                wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(0.0, 10.0, 0.0) };
            const wy::Vector3 tri2[3] = {
                wy::Vector3(50.0, 50.0, 0.0), wy::Vector3(60.0, 50.0, 0.0), wy::Vector3(50.0, 60.0, 0.0) };
            for (const wy::Vector3* tri : { tri1, tri2 })
            {
                for (int i = 0; i < 3; ++i)
                {
                    wy3d::SketchLine3D* pLine(nullptr);
                    EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, tri[i], tri[(i + 1) % 3], pLine), wy::ErrorStatus::Ok);
                    if (!pLine)
                    {
                        pDb->getTransactionManager()->abortTransaction();
                        return wydb::ElementId::kNull;
                    }
                    EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
                }
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create an empty 3D sketch
    static wydb::ElementId createEmptySketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Create a 2D sketch with an open profile (3 lines on XY)
    static wydb::ElementId createOpen2DSketch(wy3d::Database* pDb)
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

            const wy::Vector2 pts[4] = {
                wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0),
                wy::Vector2(100.0, 50.0), wy::Vector2(0.0, 50.0) };
            for (int i = 0; i < 3; ++i)
            {
                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
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

    static std::vector<wydb::ElementId> getCurveIds(const wy3d::Sketch3D* pSketch3D)
    {
        std::vector<wydb::ElementId> ids;
        for (wy::Iterator<wydb::ElementId> iter = pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
        {
            ids.emplace_back(iter.current());
        }
        return ids;
    }

    static std::uint32_t getChainErrorCode(wy3d::Database* pDb, const wydb::ElementId& id)
    {
        return wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(id).get());
    }

    static void getShapeBounds(const TopoDS_Shape& shape, double& xmin, double& xmax,
        double& ymin, double& ymax, double& zmin, double& zmax)
    {
        // Mesh first: without triangulations BRepBndLib falls back to the whole
        // underlying surface bounds (BRepFill patches extend past the boundary)
        BRepMesh_IncrementalMesh mesher(shape, 1e-4);
        Bnd_Box bndBox;
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
        {
            BRepBndLib::Add(exp.Current(), bndBox);
        }
        xmin = bndBox.CornerMin().X();
        xmax = bndBox.CornerMax().X();
        ymin = bndBox.CornerMin().Y();
        ymax = bndBox.CornerMax().Y();
        zmin = bndBox.CornerMin().Z();
        zmax = bndBox.CornerMax().Z();
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

    // Every face/edge of the built shape must resolve a topo name; the GUI
    // looks the name up by the shape's own sub-shapes (e.g. after BRepFill)
    static void expectAllTopoNamed(const wy3d::Sheet* pSheet)
    {
        const wy3d::TopoNaming* pTopoNaming = pSheet->getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);
        const TopoDS_Shape& shape = pSheet->getShape();

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
}

// --- Create ---

TEST(FilledSheet, CreateSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getBoundarySketch(), sketchId);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, 0.0, 1e-6);
        EXPECT_NEAR(xmax, 100.0, 1e-6);
        EXPECT_NEAR(ymin, 0.0, 1e-6);
        EXPECT_NEAR(ymax, 50.0, 1e-6);
        EXPECT_NEAR(zmin, 0.0, 1e-6);
        EXPECT_NEAR(zmax, 0.0, 1e-6);
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);
    EXPECT_EQ(pSketch->getParent(), pSheet->getId());

    const std::vector<wydb::ElementId> children = pSheet->getChildren();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0], sketchId);
}

TEST(FilledSheet, CreateSketch3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createLineSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getBoundarySketch(), sketchId);
    // A single open line cannot fill a surface
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));

    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_EQ(pSketch3D->getParent(), pSheet->getId());

    const std::vector<wydb::ElementId> children = pSheet->getChildren();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0], sketchId);
}

// --- Occupied sketch ---

TEST(FilledSheet, CreateWithOccupiedSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    wy3d::FilledSheet* pSheet1(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet1), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet1, nullptr);

    wy3d::FilledSheet* pSheet2(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet2), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSheet2, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(countElements(pDb.get()), 6u); // 2D sketch + 4 lines + sheet1
}

TEST(FilledSheet, CreateWithOccupiedSketch3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createLineSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet1(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet1), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet1, nullptr);

    wy3d::FilledSheet* pSheet2(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet2), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSheet2, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(countElements(pDb.get()), 3u); // sketch3d + 1 line + sheet1
}

// --- Null args ---

TEST(FilledSheet, NullArgs)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sketch3DId = createLineSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketch3DId));
        ASSERT_NE(pSketch, nullptr);
        ASSERT_NE(pSketch3D, nullptr);

        EXPECT_EQ(wy3d::FilledSheet::create(nullptr, pSketch, pSheet), wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, static_cast<wy3d::Sketch*>(nullptr), pSheet), wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(wy3d::FilledSheet::create(nullptr, pSketch3D, pSheet), wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, static_cast<wy3d::Sketch3D*>(nullptr), pSheet), wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(pSheet, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(countElements(pDb.get()), 7u); // 2D sketch + 4 lines + sketch3d + 1 line
}

// --- IO ---

TEST(FilledSheet, IO)
{
    std::string filePath("./test_filled_sheet.wy3dt");
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId sketch3DId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId sheet3DId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        sketchId = createRectSketch(pDb.get());
        sketch3DId = createLineSketch3D(pDb.get());

        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketch3DId));
            ASSERT_NE(pSketch, nullptr);
            ASSERT_NE(pSketch3D, nullptr);
            wy3d::FilledSheet* pSheet(nullptr);
            wy3d::FilledSheet* pSheet3D(nullptr);
            EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet3D), wy::ErrorStatus::Ok);
            EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
            sheet3DId = pSheet3D->getId();
        }

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(pSheet->getBoundarySketch(), sketchId);
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(pSketch->getParent(), sheetId);

        const wy3d::FilledSheet* pSheet3D = wy3d::FilledSheet::cast(pDb->getElement(sheet3DId));
        ASSERT_NE(pSheet3D, nullptr);
        EXPECT_EQ(pSheet3D->getBoundarySketch(), sketch3DId);
        const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketch3DId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(pSketch3D->getParent(), sheet3DId);
    }
}

// --- Erasure ---

TEST(FilledSheet, EraseSketchErasesSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketchWrite = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketchWrite, nullptr);
        EXPECT_EQ(pSketchWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pSheet->isErased());
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(FilledSheet, EraseSketch3DErasesSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createLineSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3DWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3DWrite, nullptr);
        EXPECT_EQ(pSketch3DWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pSheet->isErased());
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

// --- Generate ---

TEST(FilledSheet, Generate2DRectangle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmax, 100.0, 1e-6);
        EXPECT_NEAR(ymax, 50.0, 1e-6);
        EXPECT_NEAR(zmin, 0.0, 1e-6);
        EXPECT_NEAR(zmax, 0.0, 1e-6);
    }
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, Generate2DOpenProfileFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createOpen2DSketch(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::PROFILE_ExistCurveNotInClosedLoop));
}

TEST(FilledSheet, Generate3DSquare)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createClosedSquareSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, 0.0, 1e-3);
        EXPECT_NEAR(xmax, 100.0, 1e-3);
        EXPECT_NEAR(ymin, 0.0, 1e-3);
        EXPECT_NEAR(ymax, 100.0, 1e-3);
        EXPECT_NEAR(zmin, 0.0, 1e-3);
        EXPECT_NEAR(zmax, 0.0, 1e-3);
    }
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, Generate3DCircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createCircleSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, -25.0, 1e-3);
        EXPECT_NEAR(xmax, 25.0, 1e-3);
        EXPECT_NEAR(ymin, -25.0, 1e-3);
        EXPECT_NEAR(ymax, 25.0, 1e-3);
        EXPECT_NEAR(zmin, 0.0, 1e-3);
        EXPECT_NEAR(zmax, 0.0, 1e-3);
    }
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, Generate3DEllipse)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createEllipseSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, -25.0, 1e-3);
        EXPECT_NEAR(xmax, 25.0, 1e-3);
        EXPECT_NEAR(ymin, -15.0, 1e-3);
        EXPECT_NEAR(ymax, 15.0, 1e-3);
        EXPECT_NEAR(zmin, 0.0, 1e-3);
        EXPECT_NEAR(zmax, 0.0, 1e-3);
    }
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, Generate3DClosedSpline)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wydb::ElementId sketchId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D(nullptr);
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        ASSERT_NE(pSketch3D, nullptr);
        // 首尾重合的闭合插值样条(单曲线成环)
        wy3d::SketchSpline3D* pSpline(nullptr);
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, {
            wy::Vector3(-20.0, 0.0, 0.0),
            wy::Vector3(0.0, 20.0, 0.0),
            wy::Vector3(20.0, 0.0, 0.0),
            wy::Vector3(0.0, -20.0, 0.0),
            wy::Vector3(-20.0, 0.0, 0.0) }, pSpline), wy::ErrorStatus::Ok);
        ASSERT_NE(pSpline, nullptr);
        EXPECT_EQ(pSketch3D->addEntity(pSpline), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        sketchId = pSketch3D->getId();
    }

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
}

TEST(FilledSheet, Generate3DOpenEllipseArcFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createOpenEllipseArcSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));
}

TEST(FilledSheet, Generate3DArcLineLoop)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createSemicircleSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, -25.0, 1e-3);
        EXPECT_NEAR(xmax, 25.0, 1e-3);
        EXPECT_NEAR(ymin, 0.0, 1e-3);
        EXPECT_NEAR(ymax, 25.0, 1e-3);
        EXPECT_NEAR(zmin, 0.0, 1e-3);
        EXPECT_NEAR(zmax, 0.0, 1e-3);
    }
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, Generate3DOpenArcFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createOpenArcSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));
}

TEST(FilledSheet, Generate3DNonPlanarQuad)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createNonPlanarQuadSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, 0.0, 1e-3);
        EXPECT_NEAR(xmax, 100.0, 1e-3);
        EXPECT_NEAR(ymin, 0.0, 1e-3);
        EXPECT_NEAR(ymax, 100.0, 1e-3);
        EXPECT_GT(zmin, -1.0);
        EXPECT_LT(zmax, 51.0);
    }
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, Generate3DOpenChainFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createOpenChainSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));
}

TEST(FilledSheet, Generate3DTwoLoopsFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createTwoLoopSketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));
}

// A filled surface takes one boundary wire, and a 2D sketch with a second region would need a
// second one: refused the way the 3D two-loop sketch already is
TEST(FilledSheet, Generate2DTwoLoopsFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createTwoLoopSketch(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));
}

TEST(FilledSheet, Generate3DEmptySketchFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_InvalidData));
}

TEST(FilledSheet, Generate3DDegenerateLineFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D(nullptr);
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        ASSERT_NE(pSketch3D, nullptr);
        wy3d::SketchLine3D* pLine(nullptr);
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 0.0, 0.0), wy::Vector3(1.0, 0.0, 0.0), pLine), wy::ErrorStatus::Ok);
        ASSERT_NE(pLine, nullptr);
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        sketchId = pSketch3D->getId();
    }

    wy3d::FilledSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_InvalidData));
}

// --- Profile ---

TEST(FilledSheet, Sketch3DProfile_ClosedSquare)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createClosedSquareSketch3D(pDb.get());
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());
    ASSERT_EQ(profile.getLoop().size(), 4u);
    for (const wy3d::BiCurve3D& biCurve : profile.getLoop())
    {
        ASSERT_NE(biCurve.curve, nullptr);
    }
}

TEST(FilledSheet, Sketch3DProfile_OpenChainReportsEndCurves)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createOpenChainSketch3D(pDb.get());
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch3D, nullptr);

    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_FALSE(profile.check());
    std::shared_ptr<wy3d::SketchError> pError = profile.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed);

    // Only the free ends of the chain are reported
    const std::set<wydb::ElementId> expectedIds{ curveIds[0], curveIds[2] };
    const std::set<wydb::ElementId> reportedIds(pError->ids.cbegin(), pError->ids.cend());
    EXPECT_EQ(reportedIds, expectedIds);
}

TEST(FilledSheet, Sketch3DProfile_BranchReportsAllCurves)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createBranchSketch3D(pDb.get());
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch3D, nullptr);

    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_FALSE(profile.check());
    std::shared_ptr<wy3d::SketchError> pError = profile.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed);

    // The 3-curve junction reports every line
    const std::set<wydb::ElementId> expectedIds(curveIds.cbegin(), curveIds.cend());
    const std::set<wydb::ElementId> reportedIds(pError->ids.cbegin(), pError->ids.cend());
    EXPECT_EQ(reportedIds, expectedIds);
}

// --- Profile: adversarial inputs (must report an error, never crash) ---

namespace
{
    static bool addLines3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const std::vector<std::pair<wy::Vector3, wy::Vector3>>& segments)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return false;
        }
        for (const std::pair<wy::Vector3, wy::Vector3>& segment : segments)
        {
            wy3d::SketchLine3D* pLine(nullptr);
            if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, segment.first, segment.second, pLine) || !pLine)
            {
                pMgr->abortTransaction();
                return false;
            }
            if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pLine))
            {
                pMgr->abortTransaction();
                return false;
            }
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool addCircle3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return false;
        }
        wy3d::SketchCircle3D* pCircle(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchCircle3D::create(pTrans, center, normal, xDir, radius, pCircle) || !pCircle)
        {
            pMgr->abortTransaction();
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pCircle))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool erase3DEntity(wy3d::Database* pDb, const wydb::ElementId& entityId)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wydb::Element* pElem = pTrans->getElementForWrite(entityId);
        if (!pElem || wy::ErrorStatus::Ok != pElem->erase(true))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static const wy3d::Sketch3D* getSketch3D(wy3d::Database* pDb, const wydb::ElementId& sketchId)
    {
        return wy3d::Sketch3D::cast(pDb->getElement(sketchId));
    }

    // 2D sketch holding two coincident lines (the 2D counterpart of
    // Sketch3DProfile_DuplicateCoincidentLines)
    static wydb::ElementId createDuplicateLines2DSketch(wy3d::Database* pDb)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchPlane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
        wy3d::Sketch* pSketch(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::Sketch::create(pTrans, plane, pSketch) || !pSketch)
        {
            pMgr->abortTransaction();
            return wydb::ElementId::kNull;
        }
        for (int i = 0; i < 2; ++i)
        {
            wy3d::SketchLine* pLine(nullptr);
            if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans,
                wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLine) || !pLine)
            {
                pMgr->abortTransaction();
                return wydb::ElementId::kNull;
            }
            if (wy::ErrorStatus::Ok != pSketch->addEntity(pLine))
            {
                pMgr->abortTransaction();
                return wydb::ElementId::kNull;
            }
        }
        if (wy::ErrorStatus::Ok != pMgr->endTransaction())
        {
            return wydb::ElementId::kNull;
        }
        return pSketch->getId();
    }

    // Build a FilledSheet from the sketch and report its chain update error code
    // plus whether a shape came out of it
    static std::uint32_t makeFilledSheetErrorCode(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        bool& shapeIsNull)
    {
        shapeIsNull = false;
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return 0;
        }
        wy3d::FilledSheet* pSheet(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet) || !pSheet)
        {
            pMgr->abortTransaction();
            return 0;
        }
        if (wy::ErrorStatus::Ok != pMgr->endTransaction())
        {
            return 0;
        }
        shapeIsNull = pSheet->getShape().IsNull();
        return getChainErrorCode(pDb, pSheet->getId());
    }
}

TEST(FilledSheet, Sketch3DProfile_SingleCircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createCircleSketch3D(pDb.get());
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());
    ASSERT_EQ(profile.getLoop().size(), 1u);
    EXPECT_NE(profile.getLoop().front().curve, nullptr);
    EXPECT_TRUE(profile.getLoop().front().orient);
}

TEST(FilledSheet, Sketch3DProfile_DuplicateCoincidentLines)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    // Two coincident lines: every vertex has degree 2 and the walk goes out and
    // back, so the curve graph looks like a closed loop with no area
    const wy::Vector3 startPnt(0.0, 0.0, 0.0);
    const wy::Vector3 endPnt(10.0, 0.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { startPnt, endPnt }, { startPnt, endPnt } }));
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    // Accepted by the curve-level check (a known, deliberate limit)
    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());
    EXPECT_EQ(profile.getLoop().size(), 2u);

    // The degenerate loop is not rejected downstream either: the face build
    // succeeds and yields a zero-area face (the 2D path behaves the same, see
    // Generate2DDuplicateCoincidentLines). Known and left alone; it must not crash
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::Transaction* pTrans = pMgr->startTransaction();
    wy3d::Sketch3D* pSketch3DForWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
    ASSERT_NE(pSketch3DForWrite, nullptr);
    wy3d::FilledSheet* pSheet(nullptr);
    ASSERT_EQ(wy3d::FilledSheet::create(pTrans, pSketch3DForWrite, pSheet), wy::ErrorStatus::Ok);
    ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    ASSERT_NE(pSheet, nullptr);
    ASSERT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);

    GProp_GProps gprops;
    BRepGProp::SurfaceProperties(pSheet->getShape(), gprops);
    EXPECT_NEAR(gprops.Mass(), 0.0, 1e-9);
}

TEST(FilledSheet, Sketch3DProfile_NearCoincidentGap)
{

    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    // A square whose two meeting endpoints straddle a fusion bin centre (-4e-6
    // and +4e-6 both quantize to bin 0 at tol 1e-5): the gap is 8e-6, far above
    // Precision::Confusion (1e-7), so the builder cannot join the two vertices
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, {
        { wy::Vector3(0.0, -4e-6, 0.0), wy::Vector3(10.0, 0.0, 0.0) },
        { wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(10.0, 10.0, 0.0) },
        { wy::Vector3(10.0, 10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0) },
        { wy::Vector3(0.0, 10.0, 0.0), wy::Vector3(0.0, 4e-6, 0.0) } }));
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    // Known wart: the curve-level fusion is quantized (not a distance), so this
    // sketch passes the hover check and then fails at generation time
    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());
    EXPECT_EQ(profile.getLoop().size(), 4u);

    bool shapeIsNull = false;
    EXPECT_EQ(makeFilledSheetErrorCode(pDb.get(), sketchId, shapeIsNull),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed));
    EXPECT_TRUE(shapeIsNull);
}

TEST(FilledSheet, Sketch3DProfile_TwoTrianglesShareVertex)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    // Two triangles meeting at the origin: that vertex has degree 4
    const wy::Vector3 p0(0.0, 0.0, 0.0);
    const wy::Vector3 p1(10.0, 0.0, 0.0);
    const wy::Vector3 p2(0.0, 10.0, 0.0);
    const wy::Vector3 p3(20.0, 0.0, 0.0);
    const wy::Vector3 p4(0.0, 20.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, {
        { p0, p1 }, { p1, p2 }, { p2, p0 },
        { p0, p3 }, { p3, p4 }, { p4, p0 } }));
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_FALSE(profile.check());
    std::shared_ptr<wy3d::SketchError> pError = profile.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed);
    EXPECT_EQ(pError->ids.size(), 4u); // every curve meeting at the shared vertex
}

TEST(FilledSheet, Sketch3DProfile_CircleSeamTouchedByLine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    // The circle seam is center + xDir * radius = (10, 0, 0)
    ASSERT_TRUE(addCircle3D(pDb.get(), sketchId, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis, 10.0));
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(20.0, 0.0, 0.0) } }));
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_FALSE(profile.check());
    std::shared_ptr<wy3d::SketchError> pError = profile.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed);
    EXPECT_EQ(pError->ids.size(), 2u); // the circle (degree 2 at its seam) plus the line
}

TEST(FilledSheet, Sketch3DProfile_ArcLineLoop)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createSemicircleSketch3D(pDb.get());
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());
    ASSERT_EQ(profile.getLoop().size(), 2u);

    // The arc endpoints fuse with the chord line's endpoints, so the walk keeps
    // both curves in their natural direction
    const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(profile.getLoop()[0].curve);
    ASSERT_NE(pArc, nullptr);
    EXPECT_TRUE(profile.getLoop()[0].orient);
    EXPECT_NEAR(pArc->getStartPoint().x(), 25.0, 1e-9);
    EXPECT_NEAR(pArc->getEndPoint().x(), -25.0, 1e-9);

    const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(profile.getLoop()[1].curve);
    ASSERT_NE(pLine, nullptr);
    EXPECT_TRUE(profile.getLoop()[1].orient);
    EXPECT_NEAR(pLine->getStartPoint().x(), -25.0, 1e-9);
    EXPECT_NEAR(pLine->getEndPoint().x(), 25.0, 1e-9);
}

TEST(FilledSheet, Sketch3DProfile_ReusedAfterSketchEdit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createClosedSquareSketch3D(pDb.get());
    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 4u);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());
    ASSERT_EQ(profile.getLoop().size(), 4u);

    // Break the loop behind the profile's back
    ASSERT_TRUE(erase3DEntity(pDb.get(), curveIds[0]));

    EXPECT_FALSE(profile.check());
    EXPECT_TRUE(profile.getLoop().empty());
    ASSERT_NE(profile.getError(), nullptr);
    EXPECT_EQ(profile.getError()->type, wy3d::ErrorCode::FILLEDSHEET_EdgesNotClosed);
}

TEST(FilledSheet, Generate2DDuplicateCoincidentLines)
{
    // The 2D counterpart of the duplicate-line case: does the 2D path accept the
    // degenerate loop and hand out a sheet too? (3D-only defect vs shared wart)
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createDuplicateLines2DSketch(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::Transaction* pTrans = pMgr->startTransaction();
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    ASSERT_NE(pSketch, nullptr);
    wy3d::FilledSheet* pSheet(nullptr);
    EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
    EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    ASSERT_NE(pSheet, nullptr);

    // Same as the 3D path: a zero-area face, no error reported
    ASSERT_FALSE(pSheet->getShape().IsNull());
    GProp_GProps gprops;
    BRepGProp::SurfaceProperties(pSheet->getShape(), gprops);
    EXPECT_NEAR(gprops.Mass(), 0.0, 1e-9);
}

// --- Constraint curves ---

namespace
{
    // A constraint arching over the middle of the non-planar quad: both ends land on the
    // boundary edges (x=0 and x=100 at y=50), the middle rides at z=80
    static wydb::ElementId createArchConstraintSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 pts[3] = {
                wy::Vector3(0.0, 50.0, 0.0),
                wy::Vector3(50.0, 50.0, 80.0),
                wy::Vector3(100.0, 50.0, 25.0) };
            for (int i = 0; i < 2; ++i)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // Two arches crossing at the apex, each one a chain of its own, all four ends landing on
    // the r=25 boundary circle: the shape test-001.wy3dt draws
    static wydb::ElementId createCrossingArchesSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 pts[5] = {
                wy::Vector3(0.0, -25.0, 0.0), wy::Vector3(0.0, 0.0, 10.0), wy::Vector3(0.0, 25.0, 0.0),
                wy::Vector3(25.0, 0.0, 0.0), wy::Vector3(-25.0, 0.0, 0.0) };
            const int segs[4][2] = { { 0, 1 }, { 1, 2 }, { 3, 1 }, { 1, 4 } };
            for (const int* seg : segs)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, pts[seg[0]], pts[seg[1]], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // An arch as a 2D sketch on the y=30 plane, both ends again on the boundary: at
    // y=30 the x=100 edge sits at z=15. yDir = normal x xDir = -Z, so the local v of
    // an upward arch is negative: (u, v) -> (u, 30, -v)
    static wydb::ElementId createArchConstraintSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 30.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector2 pts[3] = {
                wy::Vector2(0.0, 0.0),
                wy::Vector2(50.0, -70.0),
                wy::Vector2(100.0, -15.0) };
            for (int i = 0; i < 2; ++i)
            {
                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
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

    // The 3D arch above as a 2D sketch on the y=50 plane: the same arch in space,
    // drawn twice, so the plate gets one constraint twice over
    static wydb::ElementId createTwinArchConstraintSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 50.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector2 pts[3] = {
                wy::Vector2(0.0, 0.0),
                wy::Vector2(50.0, -80.0),
                wy::Vector2(100.0, -25.0) };
            for (int i = 0; i < 2; ++i)
            {
                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
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

    // A sketch holding a single point and nothing else: on the arch's plane (y = 30), so
    // (u, v) -> (u, 30, -v) puts it at (50, 30, 70)
    static wydb::ElementId createPointConstraintSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 30.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchPoint* pPoint(nullptr);
            EXPECT_EQ(wy3d::SketchPoint::create(pTrans, wy::Vector2(50.0, -70.0), pPoint), wy::ErrorStatus::Ok);
            if (!pPoint)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch->addEntity(pPoint), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // The arch above and a point riding higher than it, in one sketch: the point sits at
    // (25, 30, 70), while the arch at u=25 is only at v=-35, that is z=35
    static wydb::ElementId createArchAndPointConstraintSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 30.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector2 pts[3] = {
                wy::Vector2(0.0, 0.0),
                wy::Vector2(50.0, -70.0),
                wy::Vector2(100.0, -15.0) };
            for (int i = 0; i < 2; ++i)
            {
                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            wy3d::SketchPoint* pPoint(nullptr);
            EXPECT_EQ(wy3d::SketchPoint::create(pTrans, wy::Vector2(25.0, -70.0), pPoint), wy::ErrorStatus::Ok);
            if (!pPoint)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch->addEntity(pPoint), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // How far the built surface stays from a point it was asked to pass through
    static bool distanceFromPointToShape(const wy::Vector3& position, const TopoDS_Shape& shape, double& distance)
    {
        TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(gp_Pnt(position.x(), position.y(), position.z()));
        BRepExtrema_DistShapeShape dist(vertex, shape);
        dist.Perform();
        if (!dist.IsDone()) return false;
        distance = dist.Value();
        return true;
    }

    // Two 2D lines that do not touch: not a single chain, hence no constraint curve
    static wydb::ElementId createTwoLineSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 50.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine* pLines[2] = { nullptr, nullptr };
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(10.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(40.0, 20.0), wy::Vector2(60.0, 20.0), pLines[1]), wy::ErrorStatus::Ok);
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

    // A closed 40x40 loop floating far above the quad: the plate solver gives up on
    // constraints it cannot reach in a reasonable way (measured: the built face stays
    // 108~124 away from every one of its edges) instead of failing Build()
    static wydb::ElementId createFloatingLoopSketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector3 pts[4] = {
                wy::Vector3(30.0, 30.0, 150.0),
                wy::Vector3(70.0, 30.0, 150.0),
                wy::Vector3(70.0, 70.0, 150.0),
                wy::Vector3(30.0, 70.0, 150.0) };
            for (int i = 0; i < 4; ++i)
            {
                wy3d::SketchLine3D* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, pts[i], pts[(i + 1) % 4], pLine), wy::ErrorStatus::Ok);
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    // A FilledSheet on the non-planar quad, with no constraint curve yet
    static wydb::ElementId createNonPlanarSheet(wy3d::Database* pDb, const wydb::ElementId& boundaryId)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pBoundary = wy3d::Sketch3D::cast(pTrans->getElementForWrite(boundaryId));
            if (!pBoundary)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            wy3d::FilledSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pBoundary, pSheet), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            if (!pSheet) return wydb::ElementId::kNull;
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // Add a constraint sketch the way the command layer does: the sheet and the sketch opened
    // for write in one transaction
    static bool addConstraintSketch(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const wydb::ElementId& constraintSketchId)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pTrans->getElementForWrite(sheetId));
        wydb::Element* pConstraintElem = pTrans->getElementForWrite(constraintSketchId);
        if (!pSheet || !pConstraintElem)
        {
            pMgr->abortTransaction();
            return false;
        }

        wy::ErrorStatus error(wy::ErrorStatus::InvalidInput);
        if (wy3d::Sketch* pSketch = wy3d::Sketch::cast(pConstraintElem))
        {
            error = pSheet->addConstraintSketch(pSketch);
        }
        else if (wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pConstraintElem))
        {
            error = pSheet->addConstraintSketch(pSketch3D);
        }
        if (wy::ErrorStatus::Ok != error)
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool removeConstraintSketch(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const wydb::ElementId& constraintSketchId)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pTrans->getElementForWrite(sheetId));
        wydb::Element* pConstraintElem = pTrans->getElementForWrite(constraintSketchId);
        if (!pSheet || !pConstraintElem)
        {
            pMgr->abortTransaction();
            return false;
        }
        wy::ErrorStatus error(wy::ErrorStatus::InvalidInput);
        if (wy3d::Sketch* pSketch = wy3d::Sketch::cast(pConstraintElem))
        {
            error = pSheet->removeConstraintSketch(pSketch);
        }
        else if (wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pConstraintElem))
        {
            error = pSheet->removeConstraintSketch(pSketch3D);
        }
        if (wy::ErrorStatus::Ok != error)
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    // Bounds of the meshed shape, taken from the mesh nodes themselves: they sit on
    // the surface, while BRepBndLib widens the box (and, with no triangulation, falls
    // back to the whole underlying surface, which for a BRepFill patch runs past the
    // boundary). Coarser than getShapeBounds on purpose: a constraint ridge is not a
    // topological edge, so meshing it at 1e-4 buys nothing but triangles.
    static void getMeshBounds(const TopoDS_Shape& shape, double& xmin, double& xmax,
        double& ymin, double& ymax, double& zmin, double& zmax)
    {
        BRepMesh_IncrementalMesh mesher(shape, 0.05);
        xmin = ymin = zmin = 1.0e300;
        xmax = ymax = zmax = -1.0e300;
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
        {
            TopLoc_Location loc;
            const Handle(Poly_Triangulation)& pTri =
                BRep_Tool::Triangulation(TopoDS::Face(exp.Current()), loc);
            if (pTri.IsNull()) continue;
            const gp_Trsf& trsf = loc.Transformation();
            for (int i = 1; i <= pTri->NbNodes(); ++i)
            {
                const gp_Pnt p = pTri->Node(i).Transformed(trsf);
                xmin = std::min(xmin, p.X());
                xmax = std::max(xmax, p.X());
                ymin = std::min(ymin, p.Y());
                ymax = std::max(ymax, p.Y());
                zmin = std::min(zmin, p.Z());
                zmax = std::max(zmax, p.Z());
            }
        }
    }

    // The constraint is soft: the plate passes through it when it can, and
    // the surface must then be visibly pulled up to the arch (z=80) and fall back to
    // the constraint-free patch (z<=50) once it is removed
    static void expectConstraintSketchLiftsTheSurface(wy3d::Database* pDb, const wydb::ElementId& constraintSketchId)
    {
        wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb);
        ASSERT_FALSE(boundaryId.isNull());
        wydb::ElementId sheetId = createNonPlanarSheet(pDb, boundaryId);
        ASSERT_FALSE(sheetId.isNull());

        const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        {
            double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
            getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
            EXPECT_NEAR(xmin, 0.0, 1e-3);
            EXPECT_NEAR(xmax, 100.0, 1e-3);
            EXPECT_NEAR(ymin, 0.0, 1e-3);
            EXPECT_NEAR(ymax, 100.0, 1e-3);
            EXPECT_GT(zmax, 49.0);  // the patch does reach the quad's z=50 corner
            EXPECT_LT(zmax, 51.0);
        }

        // Take it as a constraint
        ASSERT_TRUE(addConstraintSketch(pDb, sheetId, constraintSketchId));
        EXPECT_EQ(getChainErrorCode(pDb, sheetId), 0u);
        pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        const std::vector<wydb::ElementId>& constraintSketchIds = pSheet->getConstraintSketches();
        ASSERT_EQ(constraintSketchIds.size(), 1u);
        EXPECT_EQ(constraintSketchIds[0], constraintSketchId);
        EXPECT_NE(constraintSketchIds.cend(),
            std::find(constraintSketchIds.cbegin(), constraintSketchIds.cend(), constraintSketchId));
        const wydb::Element* pConstraintSketch = pDb->getElement(constraintSketchId);
        ASSERT_NE(pConstraintSketch, nullptr);
        EXPECT_EQ(pConstraintSketch->getParent(), sheetId);
        {
            double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
            getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
            // The lift is up, not sideways: the patch still covers the boundary quad.
            // The constrained plate is solved and re-trimmed by approximation, so it
            // now overshoots the outline by ~0.1 (measured 0.135) where the constraint-free
            // one lands on it exactly. The plate is also pulled through the arch and
            // bulges well past its top (measured 129.4 with the z=80 arch)
            EXPECT_NEAR(xmin, 0.0, 0.5);
            EXPECT_NEAR(xmax, 100.0, 0.5);
            EXPECT_NEAR(ymin, 0.0, 0.5);
            EXPECT_NEAR(ymax, 100.0, 0.5);
            EXPECT_GT(zmax, 60.0);
        }
        expectAllTopoNamed(pSheet);

        // Clear it: the surface returns to the constraint-free patch
        ASSERT_TRUE(removeConstraintSketch(pDb, sheetId, constraintSketchId));
        EXPECT_EQ(getChainErrorCode(pDb, sheetId), 0u);
        pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        const std::vector<wydb::ElementId>& remainingConstraintSketchIds = pSheet->getConstraintSketches();
        EXPECT_TRUE(remainingConstraintSketchIds.empty());
        EXPECT_EQ(remainingConstraintSketchIds.cend(),
            std::find(remainingConstraintSketchIds.cbegin(), remainingConstraintSketchIds.cend(), constraintSketchId));
        {
            double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
            getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
            EXPECT_GT(zmax, 49.0);
            EXPECT_LT(zmax, 51.0);
        }
        expectAllTopoNamed(pSheet);

        // Released: the sketch can be handed to another feature
        pConstraintSketch = pDb->getElement(constraintSketchId);
        ASSERT_NE(pConstraintSketch, nullptr);
        EXPECT_TRUE(pConstraintSketch->getParent().isNull());
    }

    // A FilledSheet on a 2D sketch, with no constraint curve yet
    static wydb::ElementId createPlanarSheet(wy3d::Database* pDb, const wydb::ElementId& boundaryId)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pBoundary = wy3d::Sketch::cast(pTrans->getElementForWrite(boundaryId));
            if (!pBoundary)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            wy3d::FilledSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pBoundary, pSheet), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            if (!pSheet) return wydb::ElementId::kNull;
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // An arch as a 2D sketch over the 100x50 rectangle's own plane (y = 25), both ends on the
    // rectangle: at y=25 the x=0 and x=100 edges sit at z=0. yDir = normal x xDir = -Z, so the
    // local v of an upward arch is negative: (u, v) -> (u, 25, -v)
    static wydb::ElementId createArchConstraintSketchOverRect(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 25.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            const wy::Vector2 pts[3] = {
                wy::Vector2(0.0, 0.0),
                wy::Vector2(50.0, -80.0),
                wy::Vector2(100.0, 0.0) };
            for (int i = 0; i < 2; ++i)
            {
                wy3d::SketchLine* pLine(nullptr);
                EXPECT_EQ(wy3d::SketchLine::create(pTrans, pts[i], pts[i + 1], pLine), wy::ErrorStatus::Ok);
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

    // Area-weighted normal of the meshed shape, taken the way the face points: the direction a
    // sheet grows in, which is what the offset and thicken directions read
    static gp_XYZ meshAreaNormal(const TopoDS_Shape& shape)
    {
        BRepMesh_IncrementalMesh mesher(shape, 0.05);
        gp_XYZ sum(0.0, 0.0, 0.0);
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
        {
            const TopoDS_Face& face = TopoDS::Face(exp.Current());
            TopLoc_Location loc;
            const Handle(Poly_Triangulation)& pTri = BRep_Tool::Triangulation(face, loc);
            if (pTri.IsNull()) continue;
            const bool isReversed = (TopAbs_REVERSED == face.Orientation());
            for (int i = 1; i <= pTri->NbTriangles(); ++i)
            {
                Standard_Integer n1(0), n2(0), n3(0);
                pTri->Triangle(i).Get(n1, n2, n3);
                if (isReversed) std::swap(n2, n3);
                const gp_Pnt p1 = pTri->Node(n1).Transformed(loc.Transformation());
                const gp_Pnt p2 = pTri->Node(n2).Transformed(loc.Transformation());
                const gp_Pnt p3 = pTri->Node(n3).Transformed(loc.Transformation());
                sum += (p2.XYZ() - p1.XYZ()).Crossed(p3.XYZ() - p1.XYZ());
            }
        }
        return sum;
    }

    // The 2D boundary twin of expectConstraintSketchLiftsTheSurface: a flat patch off the
    // rectangle, which the arch then lifts off its plane. It also pins the winding: the plate
    // solver reads the boundary chain, not the sketch plane, so it must land on the same side
    // as the flat face does
    static void expectConstraintSketchLiftsThePlanarSurface(wy3d::Database* pDb,
        const wydb::ElementId& boundaryId, const wydb::ElementId& constraintSketchId)
    {
        ASSERT_FALSE(boundaryId.isNull());
        wydb::ElementId sheetId = createPlanarSheet(pDb, boundaryId);
        ASSERT_FALSE(sheetId.isNull());

        const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 1);
        {
            double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
            getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
            EXPECT_NEAR(xmin, 0.0, 1e-3);
            EXPECT_NEAR(xmax, 100.0, 1e-3);
            EXPECT_NEAR(ymin, 0.0, 1e-3);
            EXPECT_NEAR(ymax, 50.0, 1e-3);
            EXPECT_NEAR(zmax, 0.0, 1e-3);
        }
        EXPECT_GT(meshAreaNormal(pSheet->getShape()).Z(), 0.0);
        expectAllTopoNamed(pSheet);

        // Take it as a constraint
        ASSERT_TRUE(addConstraintSketch(pDb, sheetId, constraintSketchId));
        EXPECT_EQ(getChainErrorCode(pDb, sheetId), 0u);
        pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        ASSERT_EQ(pSheet->getConstraintSketches().size(), 1u);
        EXPECT_EQ(pSheet->getConstraintSketches()[0], constraintSketchId);
        const wydb::Element* pConstraintSketch = pDb->getElement(constraintSketchId);
        ASSERT_NE(pConstraintSketch, nullptr);
        EXPECT_EQ(pConstraintSketch->getParent(), sheetId);
        EXPECT_EQ(countFaces(pSheet->getShape()), 1);
        {
            double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
            getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
            // The lift is up, not sideways: the patch still covers the rectangle
            EXPECT_NEAR(xmin, 0.0, 0.5);
            EXPECT_NEAR(xmax, 100.0, 0.5);
            EXPECT_NEAR(ymin, 0.0, 0.5);
            EXPECT_NEAR(ymax, 50.0, 0.5);
            EXPECT_GT(zmax, 60.0);
        }
        EXPECT_GT(meshAreaNormal(pSheet->getShape()).Z(), 0.0);
        expectAllTopoNamed(pSheet);

        // Clear it: the surface returns to the flat patch
        ASSERT_TRUE(removeConstraintSketch(pDb, sheetId, constraintSketchId));
        EXPECT_EQ(getChainErrorCode(pDb, sheetId), 0u);
        pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_TRUE(pSheet->getConstraintSketches().empty());
        {
            double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
            getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
            EXPECT_NEAR(zmax, 0.0, 1e-3);
        }
        expectAllTopoNamed(pSheet);

        // Released: the sketch can be handed to another feature
        pConstraintSketch = pDb->getElement(constraintSketchId);
        ASSERT_NE(pConstraintSketch, nullptr);
        EXPECT_TRUE(pConstraintSketch->getParent().isNull());
    }
}

TEST(FilledSheet, ConstraintCurveLiftsTheSurfaceFrom3DSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId constraintSketchId = createArchConstraintSketch3D(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    expectConstraintSketchLiftsTheSurface(pDb.get(), constraintSketchId);
}

TEST(FilledSheet, ConstraintCurveLiftsTheSurfaceFrom2DSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId constraintSketchId = createArchConstraintSketch(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    expectConstraintSketchLiftsTheSurface(pDb.get(), constraintSketchId);
}

// The same, with the boundary itself a 2D sketch: the planar patch has to take the constraint too
TEST(FilledSheet, ConstraintCurveLifts2DBoundarySurface)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId constraintSketchId = createArchConstraintSketchOverRect(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    expectConstraintSketchLiftsThePlanarSurface(pDb.get(), createRectSketch(pDb.get()), constraintSketchId);
}

// The boundary drawn the other way round must land on the same side: the plate solver reads the
// boundary chain rather than the sketch plane, so the winding has to be normalised either way
TEST(FilledSheet, ConstraintCurveLiftsReversed2DBoundarySurface)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId constraintSketchId = createArchConstraintSketchOverRect(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    expectConstraintSketchLiftsThePlanarSurface(pDb.get(), createReversedRectSketch(pDb.get()), constraintSketchId);
}

// Several constraint sketches at once: each one is a constraint of its own, so the plate has to
// pass through all of them, and one going away leaves the others in place
TEST(FilledSheet, ConstraintCurvesAccumulate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketch3DId = createArchConstraintSketch3D(pDb.get()); // arch at y=50, apex z=80
    ASSERT_FALSE(constraintSketch3DId.isNull());
    wydb::ElementId constraintSketch2DId = createArchConstraintSketch(pDb.get()); // arch at y=30, apex z=70
    ASSERT_FALSE(constraintSketch2DId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch3DId));
    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch2DId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::vector<wydb::ElementId>& constraintSketchIds = pSheet->getConstraintSketches();
    ASSERT_EQ(constraintSketchIds.size(), 2u);
    EXPECT_EQ(constraintSketchIds[0], constraintSketch3DId);
    EXPECT_EQ(constraintSketchIds[1], constraintSketch2DId);
    EXPECT_NE(constraintSketchIds.cend(),
        std::find(constraintSketchIds.cbegin(), constraintSketchIds.cend(), constraintSketch2DId));
    for (const wydb::ElementId& constraintSketchId : { constraintSketch3DId, constraintSketch2DId })
    {
        const wydb::Element* pConstraintSketch = pDb->getElement(constraintSketchId);
        ASSERT_NE(pConstraintSketch, nullptr);
        EXPECT_EQ(pConstraintSketch->getParent(), sheetId);
    }
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(zmax, 60.0);
    }
    expectAllTopoNamed(pSheet);

    // Taking the same sketch twice changes nothing
    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch3DId));
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getConstraintSketches().size(), 2u);

    // Drop the 3D one: the 2D arch still holds the surface up
    ASSERT_TRUE(removeConstraintSketch(pDb.get(), sheetId, constraintSketch3DId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(pSheet->getConstraintSketches().size(), 1u);
    EXPECT_EQ(pSheet->getConstraintSketches()[0], constraintSketch2DId);
    {
        const wydb::Element* pConstraintSketch3D = pDb->getElement(constraintSketch3DId);
        ASSERT_NE(pConstraintSketch3D, nullptr);
        EXPECT_TRUE(pConstraintSketch3D->getParent().isNull());
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(zmax, 60.0);
    }
    expectAllTopoNamed(pSheet);
}

// Two geometrically coincident constraint sketches ask the plate for the same thing twice: it has
// no face to give, and the pair has to be broken up again for the surface to come back
TEST(FilledSheet, CoincidentConstraintCurvesOverConstrain)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketch3DId = createArchConstraintSketch3D(pDb.get());
    ASSERT_FALSE(constraintSketch3DId.isNull());
    wydb::ElementId twinId = createTwinArchConstraintSketch(pDb.get());
    ASSERT_FALSE(twinId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch3DId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, twinId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_GenerateError));
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getConstraintSketches().size(), 2u);
    EXPECT_TRUE(pSheet->getShape().IsNull());

    // The twin is the one to go: one arch is a solvable problem again
    ASSERT_TRUE(removeConstraintSketch(pDb.get(), sheetId, twinId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(pSheet->getConstraintSketches().size(), 1u);
    EXPECT_EQ(pSheet->getConstraintSketches()[0], constraintSketch3DId);
    EXPECT_FALSE(pSheet->getShape().IsNull());
}

// The boundary is the wire the surface is built on, so it cannot be taken as a constraint on top
// of that; the command layer never offers it, the core refuses it anyway
TEST(FilledSheet, BoundaryIsNotAConstraintSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    EXPECT_FALSE(addConstraintSketch(pDb.get(), sheetId, boundaryId));
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getConstraintSketches().empty());
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
}

// A constraint sketch may hold more than one loop: every curve of it becomes a constraint
// of its own, so two disjoint triangles are as usable as one chain would be
TEST(FilledSheet, ConstraintCurveAcceptsSeveralLoops3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createTwoLoopSketch3D(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    expectAllTopoNamed(pSheet);
}

// The 2D counterpart: two lines that do not touch are several chains, and still a constraint
TEST(FilledSheet, ConstraintCurveAcceptsSeveralChains2D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createTwoLineSketch(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    expectAllTopoNamed(pSheet);
}

// The two arches of test-001.wy3dt cross at the apex, so no single chain runs through all
// four lines; every curve of the constraint sketch being a constraint of its own, the dome comes out
TEST(FilledSheet, ConstraintCurveCrossingArches3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createCircleSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createCrossingArchesSketch3D(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->getShape().IsNull());
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, -25.0, 0.5);
        EXPECT_NEAR(xmax, 25.0, 0.5);
        EXPECT_NEAR(ymin, -25.0, 0.5);
        EXPECT_NEAR(ymax, 25.0, 0.5);
        EXPECT_GT(zmax, 8.0); // the plate is pulled up to the z=10 apex
        EXPECT_LT(zmax, 40.0);
    }
    expectAllTopoNamed(pSheet);
}

// An empty constraint sketch is the one thing a constraint may not be
TEST(FilledSheet, ConstraintCurveRejectsEmptySketch3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_ConstraintCurveInvalid));
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
}

TEST(FilledSheet, ConstraintCurveNotSatisfied)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createFloatingLoopSketch3D(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLEDSHEET_ConstraintCurveNotSatisfied));
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());
}

// A point is a constraint the way a curve is: the plate is pulled onto it, and it is no edge
// of the result
TEST(FilledSheet, ConstraintPointLiftsTheSurface)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createPointConstraintSketch(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmin, 0.0, 0.5);
        EXPECT_NEAR(xmax, 100.0, 0.5);
        EXPECT_NEAR(ymin, 0.0, 0.5);
        EXPECT_NEAR(ymax, 100.0, 0.5);
        EXPECT_GT(zmax, 60.0);
    }
    double distance(0.0);
    ASSERT_TRUE(distanceFromPointToShape(wy::Vector3(50.0, 30.0, 70.0), pSheet->getShape(), distance));
    EXPECT_LT(distance, 1.0);
    expectAllTopoNamed(pSheet);

    // Clear it: the surface returns to the constraint-free patch
    ASSERT_TRUE(removeConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getConstraintSketches().empty());
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(zmax, 49.0);
        EXPECT_LT(zmax, 51.0);
    }
    expectAllTopoNamed(pSheet);
}

// The same on a 2D boundary: a sketch holding nothing but a point is a constraint, so the
// planar shortcut (which solves unconstrained and would ignore it) must not be taken
TEST(FilledSheet, ConstraintPointLifts2DBoundarySurface)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createRectSketch(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createPointConstraintSketch(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(zmax, 0.0, 1e-3);
    }

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(xmax, 100.0, 0.5);
        EXPECT_NEAR(ymax, 50.0, 0.5);
        EXPECT_GT(zmax, 20.0);
    }
    double distance(0.0);
    ASSERT_TRUE(distanceFromPointToShape(wy::Vector3(50.0, 30.0, 70.0), pSheet->getShape(), distance));
    EXPECT_LT(distance, 1.0);
    expectAllTopoNamed(pSheet);

    // Clear it: the flat patch is back
    ASSERT_TRUE(removeConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_NEAR(zmax, 0.0, 1e-3);
    }
    expectAllTopoNamed(pSheet);
}

// A sketch may hold curves and points at once, and each one is a constraint of its own
TEST(FilledSheet, ConstraintPointAndCurveInOneSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketchId = createArchAndPointConstraintSketch(pDb.get());
    ASSERT_FALSE(constraintSketchId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketchId));
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(zmax, 60.0);
    }
    // The arch holds its apex (50, 30, 70) and the point holds (25, 30, 70), which the arch
    // alone leaves 35 below it
    double distance(0.0);
    ASSERT_TRUE(distanceFromPointToShape(wy::Vector3(50.0, 30.0, 70.0), pSheet->getShape(), distance));
    EXPECT_LT(distance, 1.0);
    ASSERT_TRUE(distanceFromPointToShape(wy::Vector3(25.0, 30.0, 70.0), pSheet->getShape(), distance));
    EXPECT_LT(distance, 1.0);
    expectAllTopoNamed(pSheet);
}

TEST(FilledSheet, EraseConstraintSketchKeepsTheSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boundaryId = createNonPlanarQuadSketch3D(pDb.get());
    ASSERT_FALSE(boundaryId.isNull());
    wydb::ElementId constraintSketch3DId = createArchConstraintSketch3D(pDb.get());
    ASSERT_FALSE(constraintSketch3DId.isNull());
    wydb::ElementId sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
    ASSERT_FALSE(sheetId.isNull());

    ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch3DId));
    const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::vector<wydb::ElementId>& constraintSketchIds = pSheet->getConstraintSketches();
    ASSERT_NE(constraintSketchIds.cend(),
        std::find(constraintSketchIds.cbegin(), constraintSketchIds.cend(), constraintSketch3DId));

    // A constraint sketch going away costs one constraint, not the feature
    ASSERT_TRUE(erase3DEntity(pDb.get(), constraintSketch3DId));
    pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_FALSE(pSheet->isErased());
    EXPECT_TRUE(pSheet->getConstraintSketches().empty());
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    {
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(zmax, 49.0); // back to the constraint-free patch
        EXPECT_LT(zmax, 51.0);
    }
    expectAllTopoNamed(pSheet);
}

// The constraint set is written as a count followed by the ids, so several of them and their
// order have to come back the way they went in
TEST(FilledSheet, ConstraintSketchesRoundTrip)
{
    std::string filePath("./test_filled_sheet_constraints.wy3dt");
    wydb::ElementId boundaryId = wydb::ElementId::kNull;
    wydb::ElementId constraintSketch3DId = wydb::ElementId::kNull;
    wydb::ElementId constraintSketch2DId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        boundaryId = createNonPlanarQuadSketch3D(pDb.get());
        constraintSketch3DId = createArchConstraintSketch3D(pDb.get());
        constraintSketch2DId = createArchConstraintSketch(pDb.get());
        ASSERT_FALSE(boundaryId.isNull());
        ASSERT_FALSE(constraintSketch3DId.isNull());
        ASSERT_FALSE(constraintSketch2DId.isNull());
        sheetId = createNonPlanarSheet(pDb.get(), boundaryId);
        ASSERT_FALSE(sheetId.isNull());

        ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch3DId));
        ASSERT_TRUE(addConstraintSketch(pDb.get(), sheetId, constraintSketch2DId));
        const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        ASSERT_EQ(pSheet->getConstraintSketches().size(), 2u);

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const wy3d::FilledSheet* pSheet = wy3d::FilledSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(pSheet->getBoundarySketch(), boundaryId);
        ASSERT_EQ(pSheet->getConstraintSketches().size(), 2u);
        EXPECT_EQ(pSheet->getConstraintSketches()[0], constraintSketch3DId);
        EXPECT_EQ(pSheet->getConstraintSketches()[1], constraintSketch2DId);

        const wy3d::Sketch3D* pConstraintSketch3D = wy3d::Sketch3D::cast(pDb->getElement(constraintSketch3DId));
        ASSERT_NE(pConstraintSketch3D, nullptr);
        EXPECT_EQ(pConstraintSketch3D->getParent(), sheetId);
        const wy3d::Sketch* pConstraintSketch2D = wy3d::Sketch::cast(pDb->getElement(constraintSketch2DId));
        ASSERT_NE(pConstraintSketch2D, nullptr);
        EXPECT_EQ(pConstraintSketch2D->getParent(), sheetId);

        const std::vector<wydb::ElementId> children = pSheet->getChildren();
        ASSERT_EQ(children.size(), 3u);
        EXPECT_EQ(children[0], boundaryId);
        EXPECT_EQ(children[1], constraintSketch3DId);
        EXPECT_EQ(children[2], constraintSketch2DId);

        // The reloaded feature rebuilt the same surface: the constraints still hold it up
        EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getMeshBounds(pSheet->getShape(), xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(zmax, 60.0);
        expectAllTopoNamed(pSheet);
    }

    std::remove(filePath.c_str());
}
