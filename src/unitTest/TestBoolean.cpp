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

#include "headers.h"

#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

// --- helpers ---

static void createTwoBoxes(
    wy3d::Database* pDb, wydb::Transaction* pTrans,
    Box*& pBox1, Box*& pBox2)
{
    Box::create(pTrans, 100.0, 100.0, 100.0, pBox1);
    Box::create(pTrans, 80.0, 80.0, 80.0, pBox2);
}

// --- Union ---

TEST(Boolean, UnionTwoSolids)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Union* pUnion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pBox1(nullptr), *pBox2(nullptr);
        createTwoBoxes(pDb.get(), pTrans, pBox1, pBox2);
        wy::ErrorStatus error = Union::create(pTrans, pBox1, pBox2, pUnion);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pUnion, nullptr);
    EXPECT_EQ(pUnion->getBooleanType(), BooleanType::Union);
    EXPECT_FALSE(pUnion->getShape().IsNull());
}

TEST(Boolean, UnionMultiTools)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Union* pUnion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pTarget(nullptr), *pTool1(nullptr), *pTool2(nullptr);
        Box::create(pTrans, 200.0, 200.0, 200.0, pTarget);
        Box::create(pTrans, 50.0, 50.0, 50.0, pTool1);
        Box::create(pTrans, 60.0, 60.0, 60.0, pTool2);
        std::vector<Solid*> tools = { pTool1, pTool2 };
        wy::ErrorStatus error = Union::create(pTrans, pTarget, tools, pUnion);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pUnion, nullptr);
    EXPECT_EQ(pUnion->getTools().size(), 2u);
    EXPECT_FALSE(pUnion->getShape().IsNull());
}

TEST(Boolean, UnionRefinesRedundantFaces)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Union* pUnion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box* pBox1(nullptr);
        Box* pBox2(nullptr);
        Box::create(pTrans, 100.0, 100.0, 100.0, pBox1);
        Box::create(pTrans, 100.0, 100.0, 100.0, pBox2);
        EXPECT_EQ(pBox2->setPosition(wy::Vector3(0.0, 0.0, 100.0)), wy::ErrorStatus::Ok);
        wy::ErrorStatus error = Union::create(pTrans, pBox1, pBox2, pUnion);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }

    ASSERT_NE(pUnion, nullptr);
    const TopoDS_Shape& shape = pUnion->getShape();
    ASSERT_FALSE(shape.IsNull());

    // Two stacked boxes fuse into one box. Without refinement the side and cap faces keep the
    // split at the common plane (10 faces).
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
    EXPECT_EQ(faces.Extent(), 6);

    const wy3d::TopoNaming* pTopoNaming = pUnion->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);
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

    std::vector<std::string> info;
    EXPECT_TRUE(pTopoNaming->check(shape, info)) << (info.empty() ? "" : info[0]);
}

// --- Difference ---

TEST(Boolean, DifferenceTwoSolids)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Difference* pDiff(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pBox1(nullptr), *pBox2(nullptr);
        createTwoBoxes(pDb.get(), pTrans, pBox1, pBox2);
        wy::ErrorStatus error = Difference::create(pTrans, pBox1, pBox2, pDiff);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pDiff, nullptr);
    EXPECT_EQ(pDiff->getBooleanType(), BooleanType::Difference);
    EXPECT_FALSE(pDiff->getShape().IsNull());
}

TEST(Boolean, DifferenceMultiTools)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Difference* pDiff(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pTarget(nullptr), *pTool1(nullptr), *pTool2(nullptr);
        Box::create(pTrans, 200.0, 200.0, 200.0, pTarget);
        Box::create(pTrans, 40.0, 40.0, 300.0, pTool1);
        Box::create(pTrans, 50.0, 300.0, 50.0, pTool2);
        std::vector<Solid*> tools = { pTool1, pTool2 };
        wy::ErrorStatus error = Difference::create(pTrans, pTarget, tools, pDiff);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pDiff, nullptr);
    EXPECT_EQ(pDiff->getTools().size(), 2u);
}

// --- Intersection ---

TEST(Boolean, IntersectionTwoSolids)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Intersection* pIntersection(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pBox1(nullptr), *pBox2(nullptr);
        createTwoBoxes(pDb.get(), pTrans, pBox1, pBox2);
        wy::ErrorStatus error = Intersection::create(pTrans, pBox1, pBox2, pIntersection);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pIntersection, nullptr);
    EXPECT_EQ(pIntersection->getBooleanType(), BooleanType::Intersection);
    EXPECT_FALSE(pIntersection->getShape().IsNull());
}

TEST(Boolean, IntersectionMultiTools)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Intersection* pIntersection(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pTarget(nullptr), *pTool1(nullptr), *pTool2(nullptr);
        Box::create(pTrans, 200.0, 200.0, 200.0, pTarget);
        Box::create(pTrans, 150.0, 150.0, 150.0, pTool1);
        Box::create(pTrans, 180.0, 180.0, 180.0, pTool2);
        std::vector<Solid*> tools = { pTool1, pTool2 };
        wy::ErrorStatus error = Intersection::create(pTrans, pTarget, tools, pIntersection);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pIntersection, nullptr);
    EXPECT_FALSE(pIntersection->getShape().IsNull());
}

// --- Mixed Primitive Types ---

TEST(Boolean, UnionBoxAndCylinder)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Union* pUnion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box* pBox(nullptr);
        Cylinder* pCylinder(nullptr);
        Box::create(pTrans, 100.0, 100.0, 100.0, pBox);
        Cylinder::create(pTrans, 40.0, 100.0, pCylinder);
        wy::ErrorStatus error = Union::create(pTrans, pBox, pCylinder, pUnion);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pUnion, nullptr);
    EXPECT_FALSE(pUnion->getShape().IsNull());
}

TEST(Boolean, DifferenceSphereAndBox)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Difference* pDiff(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Sphere* pSphere(nullptr);
        Box* pBox(nullptr);
        Sphere::create(pTrans, 80.0, pSphere);
        Box::create(pTrans, 100.0, 100.0, 100.0, pBox);
        wy::ErrorStatus error = Difference::create(pTrans, pSphere, pBox, pDiff);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pDiff, nullptr);
    EXPECT_FALSE(pDiff->getShape().IsNull());
}

// --- Cast ---

TEST(Boolean, Cast)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Union* pUnion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pBox1(nullptr), *pBox2(nullptr);
        createTwoBoxes(pDb.get(), pTrans, pBox1, pBox2);
        Union::create(pTrans, pBox1, pBox2, pUnion);
        pMgr->endTransaction();
    }
    ASSERT_NE(pUnion, nullptr);

    const wydb::Element* pElem = pDb->getElement(pUnion->getId());
    ASSERT_NE(pElem, nullptr);

    const Union* pUnion2 = Union::cast(pElem);
    ASSERT_NE(pUnion2, nullptr);

    const Boolean* pBoolean = Boolean::cast(pElem);
    ASSERT_NE(pBoolean, nullptr);

    const Solid* pSolid = Solid::cast(pElem);
    ASSERT_NE(pSolid, nullptr);
}

// --- Color on Boolean ---

TEST(Boolean, SetColor)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    Union* pUnion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box *pBox1(nullptr), *pBox2(nullptr);
        createTwoBoxes(pDb.get(), pTrans, pBox1, pBox2);
        Union::create(pTrans, pBox1, pBox2, pUnion);
        pMgr->endTransaction();
    }
    ASSERT_NE(pUnion, nullptr);

    wy3d::Color c(10, 20, 30);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Union* pWrite = Union::cast(pTrans->getElementForWrite(pUnion->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setColor(c), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pUnion->getColor(), c);
}

// --- IO ---

TEST(Boolean, IO)
{
    std::string filePath("./test_boolean_unit.wy3dt");
    wydb::ElementId unionId;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();

        Union* pUnion(nullptr);
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            Box *pBox1(nullptr), *pBox2(nullptr);
            Box::create(pTrans, 100.0, 100.0, 100.0, pBox1);
            Box::create(pTrans, 50.0, 50.0, 50.0, pBox2);
            Union::create(pTrans, pBox1, pBox2, pUnion);
            pMgr->endTransaction();
        }
        ASSERT_NE(pUnion, nullptr);
        unionId = pUnion->getId();

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const Union* pUnion = Union::cast(pDb->getElement(unionId));
        ASSERT_NE(pUnion, nullptr);
        EXPECT_EQ(pUnion->getBooleanType(), BooleanType::Union);
        EXPECT_FALSE(pUnion->getShape().IsNull());
    }
}

TEST(Boolean, LoadEndCoverSample)
{
    // Regression: opening this model used to trip the face-count assert in
    // TopoShapeComparer::finallyCheckFace(). A fuse kept one body's face while splitting the
    // other body's face onto it, so the same new face was counted as both kept and split.
    const std::string filePath = std::string(YI3D_SOURCE_DIR) + "/samples/end-cover.wy3dt";
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    ASSERT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

    const wy3d::Union* pUnion = nullptr;
    wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
    for (; !iter.isDone(); iter.moveNext())
    {
        const wy3d::Union* pCandidate = wy3d::Union::cast(pDb->getElement(iter.current()));
        if (pCandidate)
        {
            pUnion = pCandidate;
            break;
        }
    }
    ASSERT_NE(pUnion, nullptr);

    const TopoDS_Shape& shape = pUnion->getShape();
    ASSERT_FALSE(shape.IsNull());

    const wy3d::TopoNaming* pTopoNaming = pUnion->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);
    std::vector<std::string> info;
    EXPECT_TRUE(pTopoNaming->check(shape, info)) << (info.empty() ? "" : info[0]);
}
