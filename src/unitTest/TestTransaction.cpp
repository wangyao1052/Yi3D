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

#include <wy3dSketch.h>
#include <wy3dSketchPlane.h>
#include <wyIterator.h>

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
}

TEST(Wy3dTransaction, AbortRollsBackNewlyCreatedBox)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    Box* pBox(nullptr);
    wy::ErrorStatus error = Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    EXPECT_NE(pBox, nullptr);
    error = pTransMgr->abortTransaction();
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    // The aborted transaction must not leave any element behind
    EXPECT_EQ(countElements(pDb.get()), 0u);
    EXPECT_TRUE(pBox->isErased());
}

TEST(Wy3dTransaction, AbortRollsBackNewlyCreatedSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wy3d::SketchPlane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    wy3d::Sketch* pSketch(nullptr);
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, plane, pSketch);
    EXPECT_EQ(error, wy::ErrorStatus::Ok);
    EXPECT_NE(pSketch, nullptr);
    error = pTransMgr->abortTransaction();
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    // The aborted transaction must not leave any element behind
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, AbortKeepsPreviouslyCommittedElements)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBox(nullptr);
        Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        pTransMgr->endTransaction();
    }

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBox(nullptr);
        Box::create(pTrans, 20.0, 20.0, 20.0, pBox);
        pTransMgr->abortTransaction();
    }

    // Only the committed box remains
    EXPECT_EQ(countElements(pDb.get()), 1u);
}

TEST(Wy3dTransaction, AbortGroupRollsBackCommittedChildLeaf)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pGroup, nullptr);

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        wy::ErrorStatus error = Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        error = pTransMgr->endTransaction();
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
    }

    // The child leaf committed while the group is still open
    EXPECT_EQ(countElements(pDb.get()), 1u);

    wy::ErrorStatus error = pTransMgr->abortTransaction();
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    // Aborting the group must roll back the committed child leaf
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, AbortGroupRollsBackAbortedChildLeaf)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pGroup, nullptr);

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        wy::ErrorStatus error = Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        error = pTransMgr->abortTransaction();
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
    }

    wy::ErrorStatus error = pTransMgr->abortTransaction();
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    // The group abort must also roll back aborted child leaves
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, AbortGroupWithActiveChildLeaf)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pGroup, nullptr);

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    ASSERT_NE(pTrans, nullptr);
    Box* pBox(nullptr);
    wy::ErrorStatus error = Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    // Abort the group while its child leaf is still active
    error = pTransMgr->abortTransaction();
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, LeafAbortRollsBackModificationToExistingElement)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    Box* pBox(nullptr);
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        pTransMgr->endTransaction();
    }
    const wy3d::Color originalColor = pBox->getColor();

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBoxWrite = Box::cast(pTrans->getElementForWrite(pBox->getId()));
        ASSERT_NE(pBoxWrite, nullptr);
        EXPECT_EQ(pBoxWrite->setColor(wy3d::Color(255, 0, 0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }

    // The aborted modification must be rolled back
    EXPECT_EQ(pBox->getColor(), originalColor);
    EXPECT_EQ(countElements(pDb.get()), 1u);
}

TEST(Wy3dTransaction, LeafAbortRevertsEraseOfExistingElement)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    Box* pBox(nullptr);
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        pTransMgr->endTransaction();
    }

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBoxWrite = Box::cast(pTrans->getElementForWrite(pBox->getId()));
        ASSERT_NE(pBoxWrite, nullptr);
        EXPECT_EQ(pBoxWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }

    // The aborted erase must be reverted
    EXPECT_FALSE(pBox->isErased());
    EXPECT_EQ(countElements(pDb.get()), 1u);
}

TEST(Wy3dTransaction, AbortGroupRollsBackMultipleCommittedChildLeaves)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pGroup, nullptr);

    for (int i = 0; i < 3; ++i)
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 10.0, 10.0, 10.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), 3u);

    EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);

    // All three committed child leaves must be rolled back
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, AbortGroupRollsBackMixedChildLeaves)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pGroup, nullptr);

    // Child 1: created and committed
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 10.0, 10.0, 10.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    // Child 2: created then aborted
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 20.0, 20.0, 20.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }
    // Child 3: created and modified, still active
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 30.0, 30.0, 30.0, pBox), wy::ErrorStatus::Ok);
        Box* pBoxWrite = Box::cast(pTrans->getElementForWrite(pBox->getId()));
        ASSERT_NE(pBoxWrite, nullptr);
        EXPECT_EQ(pBoxWrite->setColor(wy3d::Color(255, 0, 0)), wy::ErrorStatus::Ok);
    }

    // The first abort closes the active child leaf, the second aborts the group
    EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);
    EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);

    // All children must be rolled back regardless of their state
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, UndoRedoAfterGroupCommit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    {
        wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
        ASSERT_NE(pGroup, nullptr);
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 10.0, 10.0, 10.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(countElements(pDb.get()), 1u);

    EXPECT_EQ(pTransMgr->undo(), wy::ErrorStatus::Ok);
    EXPECT_EQ(countElements(pDb.get()), 0u);

    EXPECT_EQ(pTransMgr->redo(), wy::ErrorStatus::Ok);
    EXPECT_EQ(countElements(pDb.get()), 1u);
}

TEST(Wy3dTransaction, StartTransactionWhileLeafActiveReturnsNull)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    ASSERT_NE(pTrans, nullptr);

    // A second leaf cannot be started while the first is still active
    EXPECT_EQ(pTransMgr->startTransaction(), nullptr);

    EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
}

TEST(Wy3dTransaction, UndoRedoAfterLeafCommit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 10.0, 10.0, 10.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(countElements(pDb.get()), 1u);

    EXPECT_EQ(pTransMgr->undo(), wy::ErrorStatus::Ok);
    EXPECT_EQ(countElements(pDb.get()), 0u);

    EXPECT_EQ(pTransMgr->redo(), wy::ErrorStatus::Ok);
    EXPECT_EQ(countElements(pDb.get()), 1u);
}

TEST(Wy3dTransaction, NestedGroupInnerAbortRollsBackAndOuterSurvives)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pOuterGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pOuterGroup, nullptr);

    // Inner group: leaf with box A committed, then the inner group is aborted
    {
        wydb::Transaction* pInnerGroup = pTransMgr->startTransactionGroup();
        ASSERT_NE(pInnerGroup, nullptr);

        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 10.0, 10.0, 10.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);

        EXPECT_EQ(countElements(pDb.get()), 1u);

        EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }

    // Box A is rolled back with the inner group
    EXPECT_EQ(countElements(pDb.get()), 0u);

    // The outer group is still usable: box B survives the outer commit
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 20.0, 20.0, 20.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);

    EXPECT_EQ(countElements(pDb.get()), 1u);
}

TEST(Wy3dTransaction, NestedGroupOuterAbortRollsBackCommittedInnerGroup)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pOuterGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pOuterGroup, nullptr);

    {
        wydb::Transaction* pInnerGroup = pTransMgr->startTransactionGroup();
        ASSERT_NE(pInnerGroup, nullptr);

        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 10.0, 10.0, 10.0, pBox), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);

        // Commit the inner group into the outer group
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), 1u);

    // Aborting the outer group rolls back the committed inner group
    EXPECT_EQ(pTransMgr->abortTransaction(), wy::ErrorStatus::Ok);

    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, ErasePersistsAfterCommit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    Box* pBox(nullptr);
    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        Box* pBoxWrite = Box::cast(pTrans->getElementForWrite(pBox->getId()));
        ASSERT_NE(pBoxWrite, nullptr);
        EXPECT_EQ(pBoxWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTransMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    // The committed erase keeps the element hidden
    EXPECT_TRUE(pBox->isErased());
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

TEST(Wy3dTransaction, CommitGroupKeepsChildLeaf)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    wydb::Transaction* pGroup = pTransMgr->startTransactionGroup();
    ASSERT_NE(pGroup, nullptr);

    {
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        Box* pBox(nullptr);
        wy::ErrorStatus error = Box::create(pTrans, 10.0, 10.0, 10.0, pBox);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        error = pTransMgr->endTransaction();
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
    }

    wy::ErrorStatus error = pTransMgr->endTransaction();
    EXPECT_EQ(error, wy::ErrorStatus::Ok);

    EXPECT_EQ(countElements(pDb.get()), 1u);
}
