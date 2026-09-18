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

#include <wy3dNonParametricSheet.h>
#include <wy3dNonParametricSolid.h>
#include <wy3dMove.h>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pln.hxx>

TEST(Wy3d, Box)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    double length(100.0), width(200.0), height(300.0);
    Box* pBox(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy::ErrorStatus error = Box::create(pTrans, length, width, height, pBox);
    pTransMgr->endTransaction();

    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    ASSERT_NE(pBox, nullptr);
    EXPECT_EQ(pBox->getLength(), length);
    EXPECT_EQ(pBox->getWidth(), width);
    EXPECT_EQ(pBox->getHeight(), height);
}

TEST(Wy3d, Cylinder)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    double radius(50.0), height(200.0);
    Cylinder* pCylinder(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy::ErrorStatus error = Cylinder::create(pTrans, radius, height, pCylinder);
    pTransMgr->endTransaction();

    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    ASSERT_NE(pCylinder, nullptr);
    EXPECT_EQ(pCylinder->getRadius(), radius);
    EXPECT_EQ(pCylinder->getHeight(), height);
}

TEST(Wy3d, Sphere)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    double radius(30.0);
    Sphere* pSphere(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy::ErrorStatus error = Sphere::create(pTrans, radius, pSphere);
    pTransMgr->endTransaction();

    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    ASSERT_NE(pSphere, nullptr);
    EXPECT_EQ(pSphere->getRadius(), radius);
}

TEST(Wy3d, Cone)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    double radius(25.0), height(100.0);
    Cone* pCone(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy::ErrorStatus error = Cone::create(pTrans, radius, height, pCone);
    pTransMgr->endTransaction();

    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    ASSERT_NE(pCone, nullptr);
    EXPECT_EQ(pCone->getRadius(), radius);
    EXPECT_EQ(pCone->getHeight(), height);
}

TEST(Wy3d, Torus)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    double majorRadius(80.0), minorRadius(20.0);
    Torus* pTorus(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy::ErrorStatus error = Torus::create(pTrans, majorRadius, minorRadius, pTorus);
    pTransMgr->endTransaction();

    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    ASSERT_NE(pTorus, nullptr);
    EXPECT_EQ(pTorus->getMajorRadius(), majorRadius);
    EXPECT_EQ(pTorus->getMinorRadius(), minorRadius);
}

TEST(Wy3d, Tube)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    double outerRadius(50.0), innerRadius(30.0), height(200.0);
    Tube* pTube(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy::ErrorStatus error = Tube::create(pTrans, outerRadius, innerRadius, height, pTube);
    pTransMgr->endTransaction();

    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    ASSERT_NE(pTube, nullptr);
    EXPECT_EQ(pTube->getOuterRadius(), outerRadius);
    EXPECT_EQ(pTube->getInnerRadius(), innerRadius);
    EXPECT_EQ(pTube->getHeight(), height);
}

TEST(Wy3d, ConeIO)
{
    double radius(25.0), height(100.0);
    std::string filePath("./test_cone.wy3dt");
    wydb::ElementId coneId = wydb::ElementId::kNull;

    // 写入
    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

        Cone* pCone(nullptr);
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Cone::create(pTrans, radius, height, pCone);
        pTransMgr->endTransaction();
        coneId = pCone->getId();

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    // 读取
    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const wydb::Element* pElem = pDb->getElement(coneId);
        ASSERT_NE(pElem, nullptr);
        const Cone* pCone = Cone::cast(pElem);
        ASSERT_NE(pCone, nullptr);
        EXPECT_EQ(pCone->getRadius(), radius);
        EXPECT_EQ(pCone->getHeight(), height);
    }
}

TEST(Wy3d, PrimitiveModify)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    // 创建一个 Box 然后修改尺寸
    double length(100.0), width(200.0), height(300.0);
    Box* pBox(nullptr);
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box::create(pTrans, length, width, height, pBox);
        pTransMgr->endTransaction();
        ASSERT_NE(pBox, nullptr);
    }

    // 修改参数
    {
        double newLength(400.0);
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBoxWrite = Box::cast(pTrans->getElementForWrite(pBox->getId()));
        ASSERT_NE(pBoxWrite, nullptr);
        EXPECT_EQ(pBoxWrite->setLength(newLength), wy::ErrorStatus::Ok);
        pTransMgr->endTransaction();
        EXPECT_EQ(pBox->getLength(), newLength);
        EXPECT_EQ(pBox->getWidth(), width);  // 宽、高不变
        EXPECT_EQ(pBox->getHeight(), height);
    }
}

// A modification chain writes its result back into the body's own shape, so a body that cannot
// rebuild its shape from parameters - a non-parametric (imported) one - has to keep the shape it
// was built with and re-run from that. Otherwise every parameter change stacks onto the last
// result, and the change after the first one goes twice as far as asked.
TEST(Wy3d, NonParametricBodyRerunsFromItsOriginalShape)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    const auto minX = [](const TopoDS_Shape& shape)
    {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        return box.CornerMin().X();
    };

    // A 10 x 20 x 30 box with a corner at the origin
    NonParametricSolid* pSolid(nullptr);
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        EXPECT_EQ(NonParametricSolid::create(pTrans, BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape(), pSolid),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
        ASSERT_NE(pSolid, nullptr);
    }
    EXPECT_NEAR(minX(pSolid->getShape()), 0.0, 1e-6);

    Move* pMove(nullptr);
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        wy3d::Solid* pSolidWrite = wy3d::Solid::cast(pTrans->getElementForWrite(pSolid->getId()));
        ASSERT_NE(pSolidWrite, nullptr);
        EXPECT_EQ(Move::create(pTrans, pSolidWrite, wy::Vector3(100.0, 0.0, 0.0), pMove), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
        ASSERT_NE(pMove, nullptr);
    }
    EXPECT_NEAR(minX(pSolid->getShape()), 100.0, 1e-6);

    // Each new vector moves the body from where it was built, not from where the last one left it
    for (double x : { 200.0, 50.0 })
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Move* pMoveWrite = Move::cast(pTrans->getElementForWrite(pMove->getId()));
        ASSERT_NE(pMoveWrite, nullptr);
        EXPECT_EQ(pMoveWrite->setVector(wy::Vector3(x, 0.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);

        const wy3d::Solid* pBody = wy3d::Solid::cast(pDb->getElement(pSolid->getId()));
        ASSERT_NE(pBody, nullptr);
        EXPECT_NEAR(minX(pBody->getShape()), x, 1e-6) << "move x = " << x;
    }

    // The shape it was built with is what a reload has to start from, so it is persisted
    {
        const std::string filePath("./test_nonparam_solid.wy3dt");
        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        std::unique_ptr<wy3d::Database> pReloadedDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pReloadedDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Solid* pBody = wy3d::Solid::cast(pReloadedDb->getElement(pSolid->getId()));
        ASSERT_NE(pBody, nullptr);
        EXPECT_NEAR(minX(pBody->getShape()), 50.0, 1e-6);

        wydb::TransactionManager* pTransMgr = pReloadedDb->getTransactionManager();
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Move* pMoveWrite = Move::cast(pTrans->getElementForWrite(pMove->getId()));
        ASSERT_NE(pMoveWrite, nullptr);
        EXPECT_EQ(pMoveWrite->setVector(wy::Vector3(300.0, 0.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);

        pBody = wy3d::Solid::cast(pReloadedDb->getElement(pSolid->getId()));
        ASSERT_NE(pBody, nullptr);
        EXPECT_NEAR(minX(pBody->getShape()), 300.0, 1e-6);
    }
}

// The source shape of a non-parametric sheet can be swapped for another one: the sheet keeps its
// identity and recomputes, and what goes into the undo record is the replacement itself - not only
// the shape it produced
TEST(Wy3d, NonParametricSheetReplacesItsSourceShape)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    const auto minX = [](const TopoDS_Shape& shape)
    {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        return box.CornerMin().X();
    };
    const auto faceAt = [](double x)
    {
        BRepBuilderAPI_MakeFace face(
            gp_Pln(gp_Pnt(x, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 0.0, 10.0, 0.0, 20.0);
        return face.Shape();
    };

    NonParametricSheet* pSheet(nullptr);
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        EXPECT_EQ(NonParametricSheet::create(pTrans, faceAt(0.0), pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
        ASSERT_NE(pSheet, nullptr);
        sheetId = pSheet->getId();
    }
    EXPECT_NEAR(minX(pSheet->getShape()), 0.0, 1e-6);

    // Another face goes in, the same element comes out
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        NonParametricSheet* pSheetWrite =
            NonParametricSheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        EXPECT_EQ(pSheetWrite->setSourceShape(faceAt(100.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    const NonParametricSheet* pSheetNow = NonParametricSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheetNow, nullptr);
    EXPECT_NEAR(minX(pSheetNow->getShape()), 100.0, 1e-6);

    // Undo puts the first face back, redo the second one
    ASSERT_EQ(pTransMgr->undo(), wy::ErrorStatus::Ok);
    pSheetNow = NonParametricSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheetNow, nullptr);
    EXPECT_NEAR(minX(pSheetNow->getShape()), 0.0, 1e-6);

    ASSERT_EQ(pTransMgr->redo(), wy::ErrorStatus::Ok);
    pSheetNow = NonParametricSheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheetNow, nullptr);
    EXPECT_NEAR(minX(pSheetNow->getShape()), 100.0, 1e-6);

    // Undoing once more leaves the first face as the source too, and a reload rebuilds from that
    // source rather than from the shape that was undone
    ASSERT_EQ(pTransMgr->undo(), wy::ErrorStatus::Ok);
    {
        const std::string filePath("./test_nonparam_sheet.wy3dt");
        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        std::unique_ptr<wy3d::Database> pReloadedDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pReloadedDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Sheet* pReloaded = wy3d::Sheet::cast(pReloadedDb->getElement(sheetId));
        ASSERT_NE(pReloaded, nullptr);
        EXPECT_NEAR(minX(pReloaded->getShape()), 0.0, 1e-6);
    }
}

TEST(Wy3d, MultiPrimitive)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();

    Box* pBox(nullptr);
    Cylinder* pCylinder(nullptr);
    Cone* pCone(nullptr);
    Sphere* pSphere(nullptr);

    Box::create(pTrans, 100.0, 200.0, 300.0, pBox);
    Cylinder::create(pTrans, 25.0, 50.0, pCylinder);
    Cone::create(pTrans, 30.0, 80.0, pCone);
    Sphere::create(pTrans, 20.0, pSphere);

    pTransMgr->endTransaction();

    ASSERT_NE(pBox, nullptr);
    ASSERT_NE(pCylinder, nullptr);
    ASSERT_NE(pCone, nullptr);
    ASSERT_NE(pSphere, nullptr);

    // 验证各元素独立存在、id 不同
    EXPECT_FALSE(pBox->getId().isNull());
    EXPECT_FALSE(pCylinder->getId().isNull());
    EXPECT_FALSE(pCone->getId().isNull());
    EXPECT_FALSE(pSphere->getId().isNull());
    EXPECT_NE(pBox->getId(), pCylinder->getId());
    EXPECT_NE(pBox->getId(), pCone->getId());
    EXPECT_NE(pBox->getId(), pSphere->getId());
}

TEST(Wy3d, IO)
{
    double boxLength(100.0), boxWidth(200.0), boxHeight(300.0);
    std::string filePath("./test_box.wy3dt");
    wydb::ElementId boxId = wydb::ElementId::kNull;

    // 写入文件
    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

        Box* pBox(nullptr);
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box::create(pTrans, boxLength, boxWidth, boxHeight, pBox);
        pTransMgr->endTransaction();
        boxId = pBox->getId();

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    // 读取文件
    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const wydb::Element* pElem = pDb->getElement(boxId);
        ASSERT_NE(pElem, nullptr);
        const Box* pBox = Box::cast(pElem);
        ASSERT_NE(pBox, nullptr);
        EXPECT_EQ(pBox->getLength(), boxLength);
        EXPECT_EQ(pBox->getWidth(), boxWidth);
        EXPECT_EQ(pBox->getHeight(), boxHeight);
    }
}
