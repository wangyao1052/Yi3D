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

#include "../headers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dErrorCode.h>

namespace
{
    // Regenerate the fixtures with:
    //   set YI3D_GENERATE_FUSE_FIXTURES=1
    //   unitTest.exe --gtest_filter=BooleanFuseFixtures.*
    constexpr double kPi = 3.14159265358979323846;

    enum class BodyKind
    {
        Box,
        Cylinder,
        Sphere,
        Cone,
        Torus,
        Tube
    };

    struct BodySpec
    {
        BodyKind kind;
        wy::Vector3 position;
        double arg1;
        double arg2;
        double arg3;
    };

    BodySpec box(double length, double width, double height, const wy::Vector3& position)
    {
        return BodySpec{ BodyKind::Box, position, length, width, height };
    }

    BodySpec cylinder(double radius, double height, const wy::Vector3& position)
    {
        return BodySpec{ BodyKind::Cylinder, position, radius, height, 0.0 };
    }

    BodySpec sphere(double radius, const wy::Vector3& position)
    {
        return BodySpec{ BodyKind::Sphere, position, radius, 0.0, 0.0 };
    }

    BodySpec cone(double radius, double height, const wy::Vector3& position)
    {
        return BodySpec{ BodyKind::Cone, position, radius, height, 0.0 };
    }

    BodySpec torus(double majorRadius, double minorRadius, const wy::Vector3& position)
    {
        return BodySpec{ BodyKind::Torus, position, majorRadius, minorRadius, 0.0 };
    }

    BodySpec tube(double outerRadius, double innerRadius, double height, const wy::Vector3& position)
    {
        return BodySpec{ BodyKind::Tube, position, outerRadius, innerRadius, height };
    }

    enum class FuseOutcome
    {
        // The kernel must produce a refined, correctly named body.
        MustSucceed,
        // Degenerate contact; OCCT may legally refuse the operation (feedback 1100, empty shape).
        MayFailGracefully
    };

    struct FuseCaseSpec
    {
        const char* name;
        const char* fileName;
        unsigned int bodyCount;
        FuseOutcome outcome;
        double volume;           // 0.0: not asserted
        unsigned int faceCount;  // 0: not asserted
        unsigned int edgeCount;  // 0: not asserted
        unsigned int solidCount; // 0: not asserted
        std::vector<BodySpec> bodies;
    };

    // The bodies are listed target first. The fixtures contain primitives only; the fuse is
    // performed by the tests, not at load time.
    const std::vector<FuseCaseSpec>& fuseCases()
    {
        static const std::vector<FuseCaseSpec> cases = {
            { "TangentCylinders", "fuse_extreme_tangent_cylinders.wy3dt", 2,
                FuseOutcome::MayFailGracefully, 251327.4121, 6, 8, 2, {
                    cylinder(20.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    cylinder(20.0, 100.0, wy::Vector3(40.0, 0.0, 0.0)) } },

            { "EdgeContactBoxes", "fuse_extreme_edge_contact_boxes.wy3dt", 2,
                FuseOutcome::MayFailGracefully, 2000000.0, 12, 23, 2, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    box(100.0, 100.0, 100.0, wy::Vector3(100.0, 100.0, 0.0)) } },

            { "CornerContactBoxes", "fuse_extreme_corner_contact_boxes.wy3dt", 2,
                FuseOutcome::MayFailGracefully, 2000000.0, 12, 24, 2, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    box(100.0, 100.0, 100.0, wy::Vector3(100.0, 100.0, 100.0)) } },

            // Two coaxial cylinders sharing a full end disc. Refinement must merge the lateral
            // faces and the caps into a single cylinder.
            { "CoaxialCylinders", "fuse_extreme_coaxial_cylinders.wy3dt", 2,
                FuseOutcome::MustSucceed, 282743.3388, 3, 3, 1, {
                    cylinder(30.0, 50.0, wy::Vector3(0.0, 0.0, 0.0)),
                    cylinder(30.0, 50.0, wy::Vector3(0.0, 0.0, 50.0)) } },

            // The tool is fully contained; its faces must be erased from the naming map.
            { "NestedBoxes", "fuse_extreme_nested_boxes.wy3dt", 2,
                FuseOutcome::MustSucceed, 1000000.0, 6, 12, 1, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    box(20.0, 20.0, 20.0, wy::Vector3(40.0, 40.0, 40.0)) } },

            // Identical bodies: both histories map onto the same result faces.
            { "CoincidentBoxes", "fuse_extreme_coincident_boxes.wy3dt", 2,
                FuseOutcome::MustSucceed, 1000000.0, 6, 12, 1, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)) } },

            { "SphereBoxTangent", "fuse_extreme_sphere_box_tangent.wy3dt", 2,
                FuseOutcome::MayFailGracefully, 1033510.3217, 7, 15, 2, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    sphere(20.0, wy::Vector3(50.0, 50.0, 120.0)) } },

            // The cone base disc lies exactly on the box top face: a zero-volume face contact.
            { "ConeBoxCoplanar", "fuse_extreme_cone_box_coplanar.wy3dt", 2,
                FuseOutcome::MustSucceed, 1047123.8896, 7, 15, 1, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    cone(30.0, 50.0, wy::Vector3(50.0, 50.0, 100.0)) } },

            // A 0.001 thick overlap; the union is one 199.999 x 100 x 100 box.
            { "SliverOverlap", "fuse_extreme_sliver_overlap.wy3dt", 2,
                FuseOutcome::MustSucceed, 1999990.0, 6, 12, 1, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    box(100.0, 100.0, 100.0, wy::Vector3(99.999, 0.0, 0.0)) } },

            // The cylinder side is tangent to the torus inner equator along a circle.
            { "TorusCylinderTangent", "fuse_extreme_torus_cylinder_tangent.wy3dt", 2,
                FuseOutcome::MayFailGracefully, 299757.9738, 5, 7, 2, {
                    torus(50.0, 10.0, wy::Vector3(0.0, 0.0, 0.0)),
                    cylinder(40.0, 40.0, wy::Vector3(0.0, 0.0, -20.0)) } },

            // Sequential fuse: through cylinder, overlapping box, then a sphere fully contained
            // in that box (the last tool contributes no material).
            { "MultiTool", "fuse_extreme_multi_tool.wy3dt", 4,
                FuseOutcome::MustSucceed, 1118831.8534, 16, 36, 1, {
                    box(100.0, 100.0, 100.0, wy::Vector3(0.0, 0.0, 0.0)),
                    cylinder(20.0, 150.0, wy::Vector3(50.0, 50.0, -25.0)),
                    box(40.0, 40.0, 40.0, wy::Vector3(80.0, 80.0, 80.0)),
                    sphere(15.0, wy::Vector3(100.0, 100.0, 100.0)) } },

            // The cylinder fills the tube hole and shares the inner cylindrical face.
            { "TubeFilledHole", "fuse_extreme_tube_filled_hole.wy3dt", 2,
                FuseOutcome::MustSucceed, 471238.8980, 3, 3, 1, {
                    tube(50.0, 30.0, 60.0, wy::Vector3(0.0, 0.0, 0.0)),
                    cylinder(30.0, 60.0, wy::Vector3(0.0, 0.0, 0.0)) } },

            // Disjoint bodies: the union is a compound of two solids.
            { "DisjointBodies", "fuse_extreme_disjoint_bodies.wy3dt", 2,
                FuseOutcome::MustSucceed, 250000.0, 12, 24, 2, {
                    box(50.0, 50.0, 50.0, wy::Vector3(0.0, 0.0, 0.0)),
                    box(50.0, 50.0, 50.0, wy::Vector3(200.0, 0.0, 0.0)) } },
        };
        return cases;
    }

    std::string fixturePath(const char* fileName)
    {
        return std::string(YI3D_SOURCE_DIR) + "/src/unitTest/fuse_extreme/" + fileName;
    }

    double getVolume(const TopoDS_Shape& shape)
    {
        GProp_GProps props;
        BRepGProp::VolumeProperties(shape, props);
        return props.Mass();
    }

    double bodyVolume(const BodySpec& body)
    {
        switch (body.kind)
        {
        case BodyKind::Box:
            return body.arg1 * body.arg2 * body.arg3;
        case BodyKind::Cylinder:
            return kPi * body.arg1 * body.arg1 * body.arg2;
        case BodyKind::Sphere:
            return 4.0 / 3.0 * kPi * body.arg1 * body.arg1 * body.arg1;
        case BodyKind::Cone:
            return kPi * body.arg1 * body.arg1 * body.arg2 / 3.0;
        case BodyKind::Torus:
            return 2.0 * kPi * kPi * body.arg1 * body.arg2 * body.arg2;
        case BodyKind::Tube:
            return kPi * (body.arg1 * body.arg1 - body.arg2 * body.arg2) * body.arg3;
        default:
            return 0.0;
        }
    }

    wy3d::Primitive* createPrimitive(wydb::Transaction* pTrans, const BodySpec& body)
    {
        wy3d::Primitive* pPrimitive = nullptr;
        switch (body.kind)
        {
        case BodyKind::Box:
        {
            wy3d::Box* pBox = nullptr;
            if (wy::ErrorStatus::Ok != wy3d::Box::create(pTrans, body.arg1, body.arg2, body.arg3, pBox))
            {
                return nullptr;
            }
            pPrimitive = pBox;
            break;
        }
        case BodyKind::Cylinder:
        {
            wy3d::Cylinder* pCylinder = nullptr;
            if (wy::ErrorStatus::Ok != wy3d::Cylinder::create(pTrans, body.arg1, body.arg2, pCylinder))
            {
                return nullptr;
            }
            pPrimitive = pCylinder;
            break;
        }
        case BodyKind::Sphere:
        {
            wy3d::Sphere* pSphere = nullptr;
            if (wy::ErrorStatus::Ok != wy3d::Sphere::create(pTrans, body.arg1, pSphere))
            {
                return nullptr;
            }
            pPrimitive = pSphere;
            break;
        }
        case BodyKind::Cone:
        {
            wy3d::Cone* pCone = nullptr;
            if (wy::ErrorStatus::Ok != wy3d::Cone::create(pTrans, body.arg1, body.arg2, pCone))
            {
                return nullptr;
            }
            pPrimitive = pCone;
            break;
        }
        case BodyKind::Torus:
        {
            wy3d::Torus* pTorus = nullptr;
            if (wy::ErrorStatus::Ok != wy3d::Torus::create(pTrans, body.arg1, body.arg2, pTorus))
            {
                return nullptr;
            }
            pPrimitive = pTorus;
            break;
        }
        case BodyKind::Tube:
        {
            wy3d::Tube* pTube = nullptr;
            if (wy::ErrorStatus::Ok != wy3d::Tube::create(pTrans, body.arg1, body.arg2, body.arg3, pTube))
            {
                return nullptr;
            }
            pPrimitive = pTube;
            break;
        }
        default:
            return nullptr;
        }
        return pPrimitive;
    }

    std::vector<wydb::ElementId> collectTopLevelSolidIds(wydb::Database* pDb)
    {
        std::vector<wydb::ElementId> ids;
        wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
        for (; !iter.isDone(); iter.moveNext())
        {
            const wydb::ElementId id = iter.current();
            const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
            if (pSolid && pSolid->getParent().isNull())
            {
                ids.push_back(id);
            }
        }
        std::sort(ids.begin(), ids.end(),
            [](const wydb::ElementId& lhs, const wydb::ElementId& rhs)
            {
                return lhs.value() < rhs.value();
            });
        return ids;
    }

    struct LoadedFixture
    {
        std::unique_ptr<wy3d::Database> pDb;
        std::vector<wydb::ElementId> bodyIds;
    };

    void loadFixture(const FuseCaseSpec& spec, LoadedFixture& fixture)
    {
        fixture.pDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(wy::ErrorStatus::Ok,
            fixture.pDb->readFile(fixturePath(spec.fileName), { wydb::FileType::Text }))
            << spec.fileName;

        fixture.bodyIds = collectTopLevelSolidIds(fixture.pDb.get());
        ASSERT_EQ(spec.bodyCount, fixture.bodyIds.size()) << spec.fileName;
        ASSERT_GE(fixture.bodyIds.size(), 2u) << spec.fileName;

        for (const wydb::ElementId& id : fixture.bodyIds)
        {
            const wy3d::Solid* pSolid = wy3d::Solid::cast(fixture.pDb->getElement(id));
            ASSERT_NE(pSolid, nullptr) << spec.fileName;
            ASSERT_FALSE(pSolid->getShape().IsNull()) << spec.fileName;
        }
    }

    void expectVolumeNear(const TopoDS_Shape& shape, double expected, double relativeTolerance)
    {
        const double actual = getVolume(shape);
        const double tolerance = std::max(1.0, std::fabs(expected)) * relativeTolerance;
        EXPECT_NEAR(expected, actual, tolerance) << "volume";
    }

    // Every face and every non-degenerate edge of the result must be named, every name must
    // belong to the current shape (no stale names), and names must be unique per shape type.
    void expectNamingComplete(const wy3d::Solid& solid)
    {
        const TopoDS_Shape& shape = solid.getShape();
        ASSERT_FALSE(shape.IsNull());
        const wy3d::TopoNaming* pTopoNaming = solid.getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        int namedFaceCount = 0;
        std::unordered_set<std::string> faceNames;
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            const std::string name = pTopoNaming->getTopoName(faces(i));
            EXPECT_FALSE(name.empty()) << "face " << i;
            if (name.empty())
            {
                continue;
            }
            ++namedFaceCount;
            faceNames.insert(name);
        }
        EXPECT_EQ(static_cast<size_t>(namedFaceCount), faceNames.size()) << "duplicate face name";

        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        int namedEdgeCount = 0;
        int degenerateEdgeCount = 0;
        std::unordered_set<std::string> edgeNames;
        for (int i = 1; i <= edges.Extent(); ++i)
        {
            const TopoDS_Edge edge = TopoDS::Edge(edges(i));
            if (BRep_Tool::Degenerated(edge))
            {
                ++degenerateEdgeCount;
                continue;
            }
            const std::string name = pTopoNaming->getTopoName(edge);
            EXPECT_FALSE(name.empty()) << "edge " << i;
            if (name.empty())
            {
                continue;
            }
            ++namedEdgeCount;
            edgeNames.insert(name);
        }
        EXPECT_EQ(static_cast<size_t>(namedEdgeCount), edgeNames.size()) << "duplicate edge name";

        const wy3d::TopoNaming::NameMap& nameMap = pTopoNaming->getNameMap();
        for (const auto& kvp : nameMap)
        {
            const TopoDS_Shape& namedShape = kvp.first;
            if (TopAbs_ShapeEnum::TopAbs_FACE == namedShape.ShapeType())
            {
                EXPECT_TRUE(faces.Contains(namedShape)) << "stale face name: " << kvp.second;
            }
            else if (TopAbs_ShapeEnum::TopAbs_EDGE == namedShape.ShapeType())
            {
                EXPECT_TRUE(edges.Contains(namedShape)) << "stale edge name: " << kvp.second;
            }
        }

        // TopoNaming::check() counts named degenerate edges in NamingCount but excludes them from
        // RealCount (sphere poles, cone apex), so results with degenerate edges fail it by design.
        // The checks above cover the same invariants with degenerate edges skipped on both sides.
        if (0 == degenerateEdgeCount)
        {
            std::vector<std::string> info;
            EXPECT_TRUE(pTopoNaming->check(shape, info))
                << (info.empty() ? "" : info[0]);
        }
    }

    void checkFuseResult(const wy3d::Solid& solid, const FuseCaseSpec& spec, const char* pathName)
    {
        const TopoDS_Shape& shape = solid.getShape();
        ASSERT_FALSE(shape.IsNull());

        expectNamingComplete(solid);

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        TopTools_IndexedMapOfShape solids;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_SOLID, solids);

        if (0 != spec.faceCount)
        {
            EXPECT_EQ(spec.faceCount, static_cast<unsigned int>(faces.Extent())) << pathName;
        }
        if (0 != spec.edgeCount)
        {
            EXPECT_EQ(spec.edgeCount, static_cast<unsigned int>(edges.Extent())) << pathName;
        }
        if (0 != spec.solidCount)
        {
            EXPECT_EQ(spec.solidCount, static_cast<unsigned int>(solids.Extent())) << pathName;
        }
        if (0.0 != spec.volume)
        {
            const double relativeTolerance =
                (FuseOutcome::MustSucceed == spec.outcome) ? 1.0e-6 : 1.0e-4;
            expectVolumeNear(shape, spec.volume, relativeTolerance);
        }

        std::cout << "[boolean-fuse-extreme] " << pathName << " " << spec.fileName
            << " faces=" << faces.Extent()
            << " edges=" << edges.Extent()
            << " solids=" << solids.Extent()
            << " volume=" << getVolume(shape)
            << std::endl;
    }

    bool isGracefulFailure(std::uint32_t errorCode, const TopoDS_Shape& shape)
    {
        return (static_cast<std::uint32_t>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError) == errorCode)
            && shape.IsNull();
    }

    void runUnionFuse(const FuseCaseSpec& spec)
    {
        LoadedFixture fixture;
        loadFixture(spec, fixture);
        ASSERT_NE(fixture.pDb, nullptr) << spec.fileName;
        ASSERT_EQ(spec.bodyCount, fixture.bodyIds.size()) << spec.fileName;

        wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
        wy3d::Union* pUnion = nullptr;
        bool isPrepared = false;
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            EXPECT_NE(pTrans, nullptr);
            if (pTrans)
            {
                wy3d::Solid* pTarget =
                    wy3d::Solid::cast(pTrans->getElementForWrite(fixture.bodyIds.front()));
                std::vector<wy3d::Solid*> tools;
                tools.reserve(fixture.bodyIds.size() - 1);
                isPrepared = (nullptr != pTarget);
                for (size_t i = 1; i < fixture.bodyIds.size(); ++i)
                {
                    wy3d::Solid* pTool =
                        wy3d::Solid::cast(pTrans->getElementForWrite(fixture.bodyIds[i]));
                    if (pTool)
                    {
                        tools.push_back(pTool);
                    }
                    else
                    {
                        isPrepared = false;
                    }
                }

                if (isPrepared)
                {
                    EXPECT_EQ(wy::ErrorStatus::Ok, wy3d::Union::create(pTrans, pTarget, tools, pUnion));
                }
                else
                {
                    ADD_FAILURE() << "failed to get writable bodies: " << spec.fileName;
                }
                EXPECT_EQ(wy::ErrorStatus::Ok, pMgr->endTransaction());
            }
        }
        if (!isPrepared)
        {
            return;
        }
        ASSERT_NE(pUnion, nullptr) << spec.fileName;

        const std::uint32_t errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pMgr->getChainUpdateFeedback(pUnion->getId()).get());
        const TopoDS_Shape& shape = pUnion->getShape();

        if (0 != errorCode || shape.IsNull())
        {
            if (FuseOutcome::MayFailGracefully == spec.outcome && isGracefulFailure(errorCode, shape))
            {
                testing::Test::RecordProperty("outcome", "kernel_failure");
                std::cout << "[boolean-fuse-extreme] union " << spec.fileName
                    << " graceful_failure code=" << errorCode << std::endl;
                return;
            }

            ADD_FAILURE() << "union failed: " << spec.fileName
                << " errorCode=" << errorCode
                << " shapeNull=" << shape.IsNull();
            return;
        }

        testing::Test::RecordProperty("outcome", "success");
        checkFuseResult(*pUnion, spec, "union");
    }

    void runMergeFuse(const FuseCaseSpec& spec)
    {
        LoadedFixture fixture;
        loadFixture(spec, fixture);
        ASSERT_NE(fixture.pDb, nullptr) << spec.fileName;
        ASSERT_EQ(spec.bodyCount, fixture.bodyIds.size()) << spec.fileName;

        wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
        bool isPrepared = false;
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            EXPECT_NE(pTrans, nullptr);
            if (pTrans)
            {
                wy3d::Solid* pTarget =
                    wy3d::Solid::cast(pTrans->getElementForWrite(fixture.bodyIds.front()));
                isPrepared = (nullptr != pTarget);
                for (size_t i = 1; i < fixture.bodyIds.size(); ++i)
                {
                    wy3d::Solid* pTool =
                        wy3d::Solid::cast(pTrans->getElementForWrite(fixture.bodyIds[i]));
                    if (pTool && pTarget)
                    {
                        EXPECT_EQ(wy::ErrorStatus::Ok, pTarget->addModification(pTool));
                    }
                    else
                    {
                        isPrepared = false;
                    }
                }

                if (!isPrepared)
                {
                    ADD_FAILURE() << "failed to get writable bodies: " << spec.fileName;
                }
                EXPECT_EQ(wy::ErrorStatus::Ok, pMgr->endTransaction());
            }
        }
        if (!isPrepared)
        {
            return;
        }

        std::uint32_t errorCode = 0;
        for (size_t i = 1; i < fixture.bodyIds.size(); ++i)
        {
            const std::uint32_t toolErrorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
                pMgr->getChainUpdateFeedback(fixture.bodyIds[i]).get());
            if (0 != toolErrorCode)
            {
                errorCode = toolErrorCode;
                break;
            }
        }

        const wy3d::Solid* pOwner = wy3d::Solid::cast(fixture.pDb->getElement(fixture.bodyIds.front()));
        ASSERT_NE(pOwner, nullptr) << spec.fileName;
        const TopoDS_Shape& shape = pOwner->getShape();

        if (0 != errorCode)
        {
            if (FuseOutcome::MayFailGracefully == spec.outcome)
            {
                testing::Test::RecordProperty("outcome", "kernel_failure");
                std::cout << "[boolean-fuse-extreme] merge " << spec.fileName
                    << " graceful_failure code=" << errorCode << std::endl;

                // A failed merge keeps the owner's pre-merge shape and must not pollute its
                // naming map.
                EXPECT_FALSE(shape.IsNull()) << spec.fileName;
                expectVolumeNear(shape, bodyVolume(spec.bodies.front()), 1.0e-9);
                expectNamingComplete(*pOwner);
                return;
            }

            ADD_FAILURE() << "merge failed: " << spec.fileName << " errorCode=" << errorCode;
            return;
        }

        if (shape.IsNull())
        {
            ADD_FAILURE() << "merge produced a null shape: " << spec.fileName;
            return;
        }

        testing::Test::RecordProperty("outcome", "success");
        checkFuseResult(*pOwner, spec, "merge");
    }

    void checkFixtureRoundTrip(const FuseCaseSpec& spec)
    {
        LoadedFixture fixture;
        loadFixture(spec, fixture);
        ASSERT_NE(fixture.pDb, nullptr) << spec.fileName;
        ASSERT_EQ(spec.bodyCount, fixture.bodyIds.size()) << spec.fileName;

        for (size_t i = 0; i < fixture.bodyIds.size(); ++i)
        {
            const wy3d::Solid* pSolid = wy3d::Solid::cast(fixture.pDb->getElement(fixture.bodyIds[i]));
            ASSERT_NE(pSolid, nullptr) << spec.fileName;
            expectVolumeNear(pSolid->getShape(), bodyVolume(spec.bodies[i]), 1.0e-9);
        }
    }
}

// Regenerates the .wy3dt fixtures. Skipped unless YI3D_GENERATE_FUSE_FIXTURES is set.
TEST(BooleanFuseFixtures, Generate)
{
    if (nullptr == std::getenv("YI3D_GENERATE_FUSE_FIXTURES"))
    {
        GTEST_SKIP() << "Set YI3D_GENERATE_FUSE_FIXTURES=1 to regenerate the fixtures.";
    }

    for (const FuseCaseSpec& spec : fuseCases())
    {
        SCOPED_TRACE(spec.fileName);

        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);

        for (const BodySpec& body : spec.bodies)
        {
            wy3d::Primitive* pPrimitive = createPrimitive(pTrans, body);
            ASSERT_NE(pPrimitive, nullptr) << spec.fileName;
            ASSERT_EQ(wy::ErrorStatus::Ok, pPrimitive->setPosition(body.position)) << spec.fileName;
        }
        ASSERT_EQ(wy::ErrorStatus::Ok, pMgr->endTransaction());

        const std::string filePath = fixturePath(spec.fileName);
        ASSERT_EQ(wy::ErrorStatus::Ok, pDb->writeFile(filePath, { wydb::FileType::Text }))
            << filePath;

        checkFixtureRoundTrip(spec);
    }
}

class BooleanFuseCase : public ::testing::TestWithParam<FuseCaseSpec>
{
};

TEST_P(BooleanFuseCase, Union)
{
    runUnionFuse(GetParam());
}

TEST_P(BooleanFuseCase, Merge)
{
    runMergeFuse(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    BooleanFuseExtreme,
    BooleanFuseCase,
    ::testing::ValuesIn(fuseCases()),
    [](const ::testing::TestParamInfo<FuseCaseSpec>& info)
    {
        return std::string(info.param.name);
    });
