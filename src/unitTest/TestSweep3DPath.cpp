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

#include <wy3dSweep.h>
#include <wy3dSweptSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchSpline3D.h>
#include <wy3dTopoName.h>
#include <wy3dTopoNaming.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

namespace
{
    // ------------------------------------------------------------------
    // 3D sketch builders
    // ------------------------------------------------------------------

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

    static bool addLines3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const std::vector<std::pair<wy::Vector3, wy::Vector3>>& segments)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D) { pMgr->abortTransaction(); return false; }
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
        if (!pSketch3D) { pMgr->abortTransaction(); return false; }
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

    static bool addSpline3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const std::vector<wy::Vector3>& fitPoints)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D) { pMgr->abortTransaction(); return false; }
        wy3d::SketchSpline3D* pSpline(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchSpline3D::create(pTrans, fitPoints, pSpline) || !pSpline)
        {
            pMgr->abortTransaction();
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSpline))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
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

    // ------------------------------------------------------------------
    // 2D sketch builders (profiles)
    // ------------------------------------------------------------------

    // Create a 2D sketch on the given plane holding a single circle
    static wydb::ElementId createCircleSketch(wy3d::Database* pDb, const wy3d::SketchPlane& plane, double radius)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchCircle* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle::create(pTrans, wy::Vector2::kZero, radius, pCircle), wy::ErrorStatus::Ok);
            if (!pCircle)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // Create a 2D sketch with an L shaped path: (0,0) -> (100,0) -> (100,100)
    static wydb::ElementId createLPathSketch(wy3d::Database* pDb, const wy3d::SketchPlane& plane)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine* pLines[2] = { nullptr, nullptr };
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 0.0), wy::Vector2(100.0, 100.0), pLines[1]), wy::ErrorStatus::Ok);
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

    // ------------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------------

    static std::uint32_t getChainErrorCode(wy3d::Database* pDb, const wydb::ElementId& id)
    {
        return wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(id).get());
    }

    // Mesh first when the shape has curved faces: without triangulations
    // BRepBndLib may fall back to the bounds of the whole underlying surface.
    // Pass deflection <= 0 for shapes whose bounding box is already exact
    static void getShapeBounds(const TopoDS_Shape& shape, double deflection, double& xmin, double& xmax,
        double& ymin, double& ymax, double& zmin, double& zmax)
    {
        if (deflection > 0.0)
        {
            BRepMesh_IncrementalMesh mesher(shape, deflection);
        }
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

    static double getSurfaceArea(const TopoDS_Shape& shape)
    {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(shape, props);
        return props.Mass();
    }

    static double getVolume(const TopoDS_Shape& shape)
    {
        GProp_GProps props;
        BRepGProp::VolumeProperties(shape, props);
        return props.Mass();
    }

    // Every face/edge the shape actually has must resolve a topo name; the GUI
    // looks the name up by the shape's own sub-shapes
    static void expectAllSubShapesNamed(const TopoDS_Shape& shape, const wy3d::TopoNaming* pTopoNaming)
    {
        ASSERT_NE(pTopoNaming, nullptr);

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            EXPECT_FALSE(pTopoNaming->getTopoName(faces(i)).empty()) << "face " << i;
        }

        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        for (int i = 1; i <= edges.Extent(); ++i)
        {
            EXPECT_FALSE(pTopoNaming->getTopoName(edges(i)).empty()) << "edge " << i;
        }
    }

    // ------------------------------------------------------------------
    // Standard setups
    // ------------------------------------------------------------------

    // Profile: circle of radius 10 on the XY plane centered at the origin; the
    // 3D path of every test below starts there, exactly like sweep.py does
    static const double kProfileRadius = 10.0;

    // Curved faces are approximated by their triangulation, so the bounds of a
    // swept body carry roughly half the deflection as an error
    static const double kMeshDeflection = 1e-3;

    // A torus is far too large to triangulate at that deflection
    static const double kNoMeshing = 0.0;

    static wydb::ElementId createXyCircleProfile(wy3d::Database* pDb)
    {
        wy3d::SketchPlane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
        return createCircleSketch(pDb, plane, kProfileRadius);
    }

    // A 3D sketch holding a single straight path from (0,0,0) to (0,0,100)
    static wydb::ElementId createVerticalLinePath3D(wy3d::Database* pDb)
    {
        wydb::ElementId pathId = createEmptySketch3D(pDb);
        if (pathId.isNull()) return pathId;
        if (!addLines3D(pDb, pathId, { { wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 100.0) } }))
        {
            return wydb::ElementId::kNull;
        }
        return pathId;
    }
}

// --- Sweep ---

// The 3D sketch is consumed as the path: it is owned by the sweep, it is listed
// among its children and the swept solid is a straight cylinder
TEST(Sweep3DPath, CreateWithLinePath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);
    EXPECT_EQ(pSweep->getPath(), pathId);
    EXPECT_EQ(pSweep->getProfile(), profileId);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSweep->getId()), 0u);

    const wy3d::Sketch3D* pPathSketch3D = wy3d::Sketch3D::cast(pDb->getElement(pathId));
    ASSERT_NE(pPathSketch3D, nullptr);
    EXPECT_EQ(pPathSketch3D->getParent(), pSweep->getId());

    const wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pDb->getElement(profileId));
    ASSERT_NE(pProfileSketch, nullptr);
    EXPECT_EQ(pProfileSketch->getParent(), pSweep->getId());

    const std::vector<wydb::ElementId> children = pSweep->getChildren();
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], profileId);
    EXPECT_EQ(children[1], pathId);

    ASSERT_FALSE(pSweep->getShape().IsNull());
    double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
    getShapeBounds(pSweep->getShape(), kMeshDeflection, xmin, xmax, ymin, ymax, zmin, zmax);
    EXPECT_NEAR(xmin, -kProfileRadius, 1e-2);
    EXPECT_NEAR(xmax, kProfileRadius, 1e-2);
    EXPECT_NEAR(ymin, -kProfileRadius, 1e-2);
    EXPECT_NEAR(ymax, kProfileRadius, 1e-2);
    EXPECT_NEAR(zmin, 0.0, 1e-2);
    EXPECT_NEAR(zmax, 100.0, 1e-2);
    EXPECT_NEAR(getVolume(pSweep->getShape()), wy3d::PI * kProfileRadius * kProfileRadius * 100.0, 1.0);
}

// The side face of a sweep is named after the profile curve and the path curve
TEST(Sweep3DPath, SideFaceNamesUseProfileAndPathCurveIds)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    const wy3d::Sketch3D* pPathSketch3D = wy3d::Sketch3D::cast(pDb->getElement(pathId));
    ASSERT_NE(pPathSketch3D, nullptr);
    const std::vector<wydb::ElementId> pathCurveIds = getCurveIds(pPathSketch3D);
    ASSERT_EQ(pathCurveIds.size(), 1u);

    const wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pDb->getElement(profileId));
    ASSERT_NE(pProfileSketch, nullptr);
    std::vector<wydb::ElementId> profileCurveIds;
    for (wy::Iterator<wydb::ElementId> iter = pProfileSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        profileCurveIds.emplace_back(iter.current());
    }
    ASSERT_EQ(profileCurveIds.size(), 1u);

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);
    const wy3d::TopoNaming* pTopoNaming = pSweep->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);

    // A cylinder: one side face, one bottom face, one top face and three edges
    const TopoDS_Shape& shape = pSweep->getShape();
    ASSERT_FALSE(shape.IsNull());
    EXPECT_EQ(countFaces(shape), 3);
    expectAllSubShapesNamed(shape, pTopoNaming);

    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
    int sideFaceCount(0);
    for (int i = 1; i <= faces.Extent(); ++i)
    {
        const wy3d::TopoName name = pTopoNaming->getTopoName(faces(i));
        std::vector<std::uint32_t> ids;
        ASSERT_TRUE(wy3d::TopoNameCodec::extractIds(name, ids)) << name;
        if (ids.size() == 2u && ids[1] == pathCurveIds[0].value())
        {
            ++sideFaceCount;
            EXPECT_EQ(ids[0], profileCurveIds[0].value());
        }
    }
    EXPECT_EQ(sideFaceCount, 1);
}

// A closed 3D circle path: the pipe shell has no start/end face and its section
// naming goes through the WIRE branch. The result is a torus
TEST(Sweep3DPath, ClosedCirclePathMakesTorus)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    ASSERT_TRUE(addCircle3D(pDb.get(), pathId, wy::Vector3::kZero, wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 100.0));

    // The path circle starts at (100,0,0), so the profile is placed there and
    // turned to be perpendicular to the path tangent
    wy3d::SketchPlane profilePlane(wy::Vector3(100.0, 0.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
    wydb::ElementId profileId = createCircleSketch(pDb.get(), profilePlane, kProfileRadius);
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSweep->getId()), 0u);
    ASSERT_FALSE(pSweep->getShape().IsNull());

    const double majorRadius(100.0);
    EXPECT_EQ(countFaces(pSweep->getShape()), 1);
    // BRepBndLib overestimates the box of an untriangulated full torus, so the
    // volume is what pins the major radius here
    double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
    getShapeBounds(pSweep->getShape(), kNoMeshing, xmin, xmax, ymin, ymax, zmin, zmax);
    EXPECT_NEAR(zmin, -kProfileRadius, 1e-6);
    EXPECT_NEAR(zmax, kProfileRadius, 1e-6);
    EXPECT_NEAR(getVolume(pSweep->getShape()),
        2.0 * wy3d::PI * wy3d::PI * majorRadius * kProfileRadius * kProfileRadius, 1e-3);
    expectAllSubShapesNamed(pSweep->getShape(), pSweep->getTopoNaming());
}

// A spline path is a single BSpline edge and the most fragile one for the
// generated/extent bookkeeping behind the topology names
TEST(Sweep3DPath, SplinePath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    ASSERT_TRUE(addSpline3D(pDb.get(), pathId, { wy::Vector3(0.0, 0.0, 0.0),
        wy::Vector3(0.0, 0.0, 50.0), wy::Vector3(0.0, 0.0, 100.0) }));
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSweep->getId()), 0u);
    ASSERT_FALSE(pSweep->getShape().IsNull());

    double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
    getShapeBounds(pSweep->getShape(), kMeshDeflection, xmin, xmax, ymin, ymax, zmin, zmax);
    EXPECT_NEAR(xmax - xmin, 2.0 * kProfileRadius, 0.1);
    EXPECT_NEAR(ymax - ymin, 2.0 * kProfileRadius, 0.1);
    EXPECT_NEAR(zmin, 0.0, 1e-2);
    EXPECT_NEAR(zmax, 100.0, 1e-2);
    expectAllSubShapesNamed(pSweep->getShape(), pSweep->getTopoNaming());
}

// A 3D sketch already owned by another feature cannot be picked up again
TEST(Sweep3DPath, CreateWithOccupiedSketch3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());
    wydb::ElementId secondProfileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(secondProfileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        ASSERT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(secondProfileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        wy3d::Sweep* pSecondSweep(nullptr);
        EXPECT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSecondSweep), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSecondSweep, nullptr);
        pMgr->abortTransaction();
    }
}

// Erasing the path sketch takes the sweep with it
TEST(Sweep3DPath, ErasePathSketchErasesSweep)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        ASSERT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        ASSERT_NE(pPath, nullptr);
        EXPECT_EQ(pPath->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pSweep->isErased());
    // The path sketch goes with it, the profile sketch stays behind
    const wy3d::Sketch3D* pErasedPath = wy3d::Sketch3D::cast(pDb->getElement(pathId));
    EXPECT_TRUE(pErasedPath == nullptr || pErasedPath->isErased());
    EXPECT_NE(wy3d::Sketch::cast(pDb->getElement(profileId)), nullptr);
}

// Editing the 3D sketch recomputes the sweep
TEST(Sweep3DPath, RegenerateAfterEditingPathSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        ASSERT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);
    const wydb::ElementId sweepId = pSweep->getId();

    // Lengthen the path in place: the old geometry element is replaced by a new one
    const wy3d::Sketch3D* pPathSketch3D = wy3d::Sketch3D::cast(pDb->getElement(pathId));
    ASSERT_NE(pPathSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pPathSketch3D);
    ASSERT_EQ(curveIds.size(), 1u);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        ASSERT_NE(pPath, nullptr);
        wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(curveIds[0]));
        ASSERT_NE(pLine, nullptr);
        pLine->setEndPoint(wy::Vector3(0.0, 0.0, 200.0));
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::Sweep* pUpdatedSweep = wy3d::Sweep::cast(pDb->getElement(sweepId));
    ASSERT_NE(pUpdatedSweep, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sweepId), 0u);
    double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
    getShapeBounds(pUpdatedSweep->getShape(), kMeshDeflection, xmin, xmax, ymin, ymax, zmin, zmax);
    EXPECT_NEAR(zmax, 200.0, 1e-2);
    EXPECT_NEAR(getVolume(pUpdatedSweep->getShape()), wy3d::PI * kProfileRadius * kProfileRadius * 200.0, 1.0);
}

// The 2D path rule "the path plane and the profile plane must be orthogonal"
// (1310) has no input for a 3D path -- a 3D sketch has no plane -- and the check
// itself only fires for a path sketch parallel to the profile plane, which is
// always a degenerate sweep anyway. What a 3D path really drops is SolidWorks'
// "the path must start on the profile plane": here the path starts 50 away from
// the profile plane and the sweep goes through
TEST(Sweep3DPath, PathStartMayLieOffTheProfilePlane)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    ASSERT_TRUE(addLines3D(pDb.get(), pathId, { { wy::Vector3(0.0, 0.0, 50.0), wy::Vector3(0.0, 0.0, 150.0) } }));
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pSweep(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSweep, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSweep->getId()), 0u);
    ASSERT_FALSE(pSweep->getShape().IsNull());
    // The profile is carried along the path, so only its own size is pinned here
    EXPECT_NEAR(getVolume(pSweep->getShape()), wy3d::PI * kProfileRadius * kProfileRadius * 100.0, 1.0);
}

// Cutting a solid with a 3D path
TEST(Sweep3DPath, CreateCutWith3DPath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Box* pBox(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_EQ(Box::create(pTrans, 100.0, 100.0, 100.0, pBox), wy::ErrorStatus::Ok);
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pBox, nullptr);

    wydb::ElementId pathId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    // Down the middle of the box, from below it to above it. The profile sits on
    // the same axis, so the cut is a full cylinder whether or not the sweep
    // repositions the profile onto the path
    ASSERT_TRUE(addLines3D(pDb.get(), pathId, { { wy::Vector3(50.0, 50.0, -10.0), wy::Vector3(50.0, 50.0, 110.0) } }));
    wydb::ElementId profileId = createCircleSketch(pDb.get(),
        wy3d::SketchPlane(wy::Vector3(50.0, 50.0, 0.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis), kProfileRadius);
    ASSERT_FALSE(profileId.isNull());

    wy3d::Sweep* pCut(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        wy3d::Solid* pSolidToCut = wy3d::Solid::cast(pTrans->getElementForWrite(pBox->getId()));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        ASSERT_NE(pSolidToCut, nullptr);
        EXPECT_EQ(wy3d::Sweep::createCut(pTrans, pPath, pProfile, pSolidToCut, pCut), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCut, nullptr);
    EXPECT_TRUE(pCut->isCut());
    EXPECT_EQ(getChainErrorCode(pDb.get(), pCut->getId()), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pBox->getId()), 0u);

    const wy3d::Solid* pUpdatedBox = wy3d::Solid::cast(pDb->getElement(pBox->getId()));
    ASSERT_NE(pUpdatedBox, nullptr);
    ASSERT_FALSE(pUpdatedBox->getShape().IsNull());
    EXPECT_NEAR(getVolume(pUpdatedBox->getShape()),
        100.0 * 100.0 * 100.0 - wy3d::PI * kProfileRadius * kProfileRadius * 100.0, 1.0);
}

// --- SweptSheet ---

// The same 3D path as a swept sheet: a tube without caps
TEST(SweptSheet3DPath, CreateWithLinePath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::SweptSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::SweptSheet::create(pTrans, pPath, pProfile, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getPath(), pathId);
    EXPECT_EQ(pSheet->getProfile(), profileId);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);

    const wy3d::Sketch3D* pPathSketch3D = wy3d::Sketch3D::cast(pDb->getElement(pathId));
    ASSERT_NE(pPathSketch3D, nullptr);
    EXPECT_EQ(pPathSketch3D->getParent(), pSheet->getId());

    const std::vector<wydb::ElementId> children = pSheet->getChildren();
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], profileId);
    EXPECT_EQ(children[1], pathId);

    const TopoDS_Shape& shape = pSheet->getShape();
    ASSERT_FALSE(shape.IsNull());
    EXPECT_EQ(countFaces(shape), 1);
    double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
    getShapeBounds(shape, kMeshDeflection, xmin, xmax, ymin, ymax, zmin, zmax);
    EXPECT_NEAR(xmin, -kProfileRadius, 1e-2);
    EXPECT_NEAR(xmax, kProfileRadius, 1e-2);
    EXPECT_NEAR(zmin, 0.0, 1e-2);
    EXPECT_NEAR(zmax, 100.0, 1e-2);
    expectAllSubShapesNamed(shape, pSheet->getTopoNaming());
}

// A 3D sketch already owned by another feature cannot be picked up again
TEST(SweptSheet3DPath, CreateWithOccupiedSketch3D)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());
    wydb::ElementId secondProfileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(secondProfileId.isNull());

    wy3d::SweptSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        ASSERT_EQ(wy3d::SweptSheet::create(pTrans, pPath, pProfile, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(secondProfileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        wy3d::SweptSheet* pSecondSheet(nullptr);
        EXPECT_EQ(wy3d::SweptSheet::create(pTrans, pPath, pProfile, pSecondSheet), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSecondSheet, nullptr);
        pMgr->abortTransaction();
    }
}

// Erasing the path sketch takes the swept sheet with it
TEST(SweptSheet3DPath, ErasePathSketchErasesSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createVerticalLinePath3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    wydb::ElementId profileId = createXyCircleProfile(pDb.get());
    ASSERT_FALSE(profileId.isNull());

    wy3d::SweptSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        ASSERT_EQ(wy3d::SweptSheet::create(pTrans, pPath, pProfile, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        ASSERT_NE(pPath, nullptr);
        EXPECT_EQ(pPath->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pSheet->isErased());
    // The path sketch goes with it, the profile sketch stays behind
    const wy3d::Sketch3D* pErasedPath = wy3d::Sketch3D::cast(pDb->getElement(pathId));
    EXPECT_TRUE(pErasedPath == nullptr || pErasedPath->isErased());
    EXPECT_NE(wy3d::Sketch::cast(pDb->getElement(profileId)), nullptr);
}

// A closed 3D circle path on a sheet goes through the section edge naming
// branch that only exists when no solid is built
TEST(SweptSheet3DPath, ClosedCirclePath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId pathId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(pathId.isNull());
    ASSERT_TRUE(addCircle3D(pDb.get(), pathId, wy::Vector3::kZero, wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 100.0));

    wy3d::SketchPlane profilePlane(wy::Vector3(100.0, 0.0, 0.0), wy::Vector3::kYAxis, wy::Vector3::kXAxis);
    wydb::ElementId profileId = createCircleSketch(pDb.get(), profilePlane, kProfileRadius);
    ASSERT_FALSE(profileId.isNull());

    wy3d::SweptSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
        wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        ASSERT_NE(pPath, nullptr);
        ASSERT_NE(pProfile, nullptr);
        EXPECT_EQ(wy3d::SweptSheet::create(pTrans, pPath, pProfile, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSheet->getId()), 0u);

    const TopoDS_Shape& shape = pSheet->getShape();
    ASSERT_FALSE(shape.IsNull());
    EXPECT_EQ(countFaces(shape), 1);
    double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
    getShapeBounds(shape, kNoMeshing, xmin, xmax, ymin, ymax, zmin, zmax);
    EXPECT_NEAR(zmin, -kProfileRadius, 1e-6);
    EXPECT_NEAR(zmax, kProfileRadius, 1e-6);
    // The torus surface, which pins both radii where the bounds cannot
    EXPECT_NEAR(getSurfaceArea(shape), 4.0 * wy3d::PI * wy3d::PI * 100.0 * kProfileRadius, 1e-3);
    expectAllSubShapesNamed(shape, pSheet->getTopoNaming());
}

// Save and load a document holding sweeps whose paths are genuinely spatial 3D
// sketches. The path is a bare ElementId on disk, so nothing here is version gated
TEST(Sweep3DPath, IO)
{
    const std::string filePath("./test_sweep3d.wy3dt");
    wydb::ElementId pathId = wydb::ElementId::kNull;
    wydb::ElementId profileId = wydb::ElementId::kNull;
    wydb::ElementId sweepId = wydb::ElementId::kNull;
    wydb::ElementId splinePathId = wydb::ElementId::kNull;
    wydb::ElementId splineProfileId = wydb::ElementId::kNull;
    wydb::ElementId splineSweepId = wydb::ElementId::kNull;
    wydb::ElementId sheetPathId = wydb::ElementId::kNull;
    wydb::ElementId sheetProfileId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();

        // 1: a polyline that turns into a different axis at every vertex, so it
        // lies in no plane at all. The profile is perpendicular to the first
        // segment, which is the only thing the sweep needs
        pathId = createEmptySketch3D(pDb.get());
        ASSERT_FALSE(pathId.isNull());
        ASSERT_TRUE(addLines3D(pDb.get(), pathId,
            { { wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 80.0) },
              { wy::Vector3(0.0, 0.0, 80.0), wy::Vector3(80.0, 0.0, 80.0) },
              { wy::Vector3(80.0, 0.0, 80.0), wy::Vector3(80.0, 80.0, 80.0) } }));
        profileId = createXyCircleProfile(pDb.get());
        ASSERT_FALSE(profileId.isNull());
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pathId));
            wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
            ASSERT_NE(pPath, nullptr);
            ASSERT_NE(pProfile, nullptr);
            wy3d::Sweep* pSweep(nullptr);
            ASSERT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
            ASSERT_NE(pSweep, nullptr);
            sweepId = pSweep->getId();
            EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        }

        // 2: a spline through points that leave every plane. Its start tangent is
        // towards the second fit point, so the profile plane is turned to match
        splinePathId = createEmptySketch3D(pDb.get());
        ASSERT_FALSE(splinePathId.isNull());
        ASSERT_TRUE(addSpline3D(pDb.get(), splinePathId, { wy::Vector3(0.0, 200.0, 0.0),
            wy::Vector3(25.0, 200.0, 50.0), wy::Vector3(25.0, 250.0, 100.0), wy::Vector3(0.0, 300.0, 150.0) }));
        const wy::Vector3 splineTangent(0.4472135955, 0.0, 0.8944271910);
        const wy::Vector3 splineXDir(0.8944271910, 0.0, -0.4472135955);
        splineProfileId = createCircleSketch(pDb.get(),
            wy3d::SketchPlane(wy::Vector3(0.0, 200.0, 0.0), splineTangent, splineXDir), kProfileRadius);
        ASSERT_FALSE(splineProfileId.isNull());
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(splinePathId));
            wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(splineProfileId));
            ASSERT_NE(pPath, nullptr);
            ASSERT_NE(pProfile, nullptr);
            wy3d::Sweep* pSweep(nullptr);
            ASSERT_EQ(wy3d::Sweep::create(pTrans, pPath, pProfile, pSweep), wy::ErrorStatus::Ok);
            ASSERT_NE(pSweep, nullptr);
            splineSweepId = pSweep->getId();
            EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        }

        // 3: the same idea on a swept sheet
        sheetPathId = createEmptySketch3D(pDb.get());
        ASSERT_FALSE(sheetPathId.isNull());
        ASSERT_TRUE(addLines3D(pDb.get(), sheetPathId,
            { { wy::Vector3(0.0, 400.0, 0.0), wy::Vector3(0.0, 400.0, 80.0) },
              { wy::Vector3(0.0, 400.0, 80.0), wy::Vector3(0.0, 480.0, 80.0) },
              { wy::Vector3(0.0, 480.0, 80.0), wy::Vector3(80.0, 480.0, 80.0) } }));
        sheetProfileId = createCircleSketch(pDb.get(),
            wy3d::SketchPlane(wy::Vector3(0.0, 400.0, 0.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis), kProfileRadius);
        ASSERT_FALSE(sheetProfileId.isNull());
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sheetPathId));
            wy3d::Sketch* pProfile = wy3d::Sketch::cast(pTrans->getElementForWrite(sheetProfileId));
            ASSERT_NE(pPath, nullptr);
            ASSERT_NE(pProfile, nullptr);
            wy3d::SweptSheet* pSheet(nullptr);
            ASSERT_EQ(wy3d::SweptSheet::create(pTrans, pPath, pProfile, pSheet), wy::ErrorStatus::Ok);
            ASSERT_NE(pSheet, nullptr);
            sheetId = pSheet->getId();
            EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        }

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::Sweep* pSweep = wy3d::Sweep::cast(pDb->getElement(sweepId));
        ASSERT_NE(pSweep, nullptr);
        EXPECT_EQ(pSweep->getPath(), pathId);
        EXPECT_EQ(pSweep->getProfile(), profileId);
        ASSERT_FALSE(pSweep->getShape().IsNull());
        // The whole point of the file: the path lies in no plane, so the tube
        // reaches far along all three axes
        double xmin(0.0), xmax(0.0), ymin(0.0), ymax(0.0), zmin(0.0), zmax(0.0);
        getShapeBounds(pSweep->getShape(), kMeshDeflection, xmin, xmax, ymin, ymax, zmin, zmax);
        EXPECT_GT(xmax - xmin, 90.0);
        EXPECT_GT(ymax - ymin, 90.0);
        EXPECT_GT(zmax - zmin, 90.0);
        // The 3D sketch is still the sweep's child after a round trip
        const wy3d::Sketch3D* pPath = wy3d::Sketch3D::cast(pDb->getElement(pathId));
        ASSERT_NE(pPath, nullptr);
        EXPECT_EQ(pPath->getParent(), sweepId);

        const wy3d::Sweep* pSplineSweep = wy3d::Sweep::cast(pDb->getElement(splineSweepId));
        ASSERT_NE(pSplineSweep, nullptr);
        EXPECT_EQ(pSplineSweep->getPath(), splinePathId);
        EXPECT_EQ(pSplineSweep->getProfile(), splineProfileId);
        EXPECT_FALSE(pSplineSweep->getShape().IsNull());

        const wy3d::SweptSheet* pSheet = wy3d::SweptSheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(pSheet->getPath(), sheetPathId);
        EXPECT_EQ(pSheet->getProfile(), sheetProfileId);
        EXPECT_FALSE(pSheet->getShape().IsNull());
        const wy3d::Sketch3D* pSheetPath = wy3d::Sketch3D::cast(pDb->getElement(sheetPathId));
        ASSERT_NE(pSheetPath, nullptr);
        EXPECT_EQ(pSheetPath->getParent(), sheetId);
    }
}
