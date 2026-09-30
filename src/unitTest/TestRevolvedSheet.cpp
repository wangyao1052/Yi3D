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

#include <string>
#include <memory>

#include <wy3dRevolvedSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dParamNames.h>
#include <wydbParameter.h>

// A revolved sheet stores its angles in radians - setters, filer and OCC all work in radians - so
// the parameter interface, which is what the property panel goes through, converts at the border:
// degrees in, degrees out, the same convention the solid Revolve feature follows.

namespace
{
    // A 100 x 50 rectangle with a centre line as its axis, 10 away from the rectangle
    static wydb::ElementId createRectSketchWithAxis(wy3d::Database* pDb, wydb::ElementId& axisId)
    {
        axisId = wydb::ElementId::kNull;
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
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(10.0, 0.0), wy::Vector2(60.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(60.0, 0.0), wy::Vector2(60.0, 50.0), pLines[1]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(60.0, 50.0), wy::Vector2(10.0, 50.0), pLines[2]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(10.0, 50.0), wy::Vector2(10.0, 0.0), pLines[3]), wy::ErrorStatus::Ok);
            for (wy3d::SketchLine* pLine : pLines)
            {
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            wy3d::SketchCenterLine* pAxis(nullptr);
            EXPECT_EQ(wy3d::SketchCenterLine::create(pTrans, wy::Vector2(0.0, -10.0), wy::Vector2(0.0, 60.0), pAxis), wy::ErrorStatus::Ok);
            if (!pAxis)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch->addEntity(pAxis), wy::ErrorStatus::Ok);
            axisId = pAxis->getId();

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }
}

TEST(RevolvedSheet, AnglesAreDegreesAtTheParameterBoundary)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();

    wydb::ElementId axisId = wydb::ElementId::kNull;
    const wydb::ElementId sketchId = createRectSketchWithAxis(pDb.get(), axisId);
    ASSERT_FALSE(sketchId.isNull());
    ASSERT_FALSE(axisId.isNull());

    wy3d::RevolvedSheet* pSheet(nullptr);
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        // The GUI command hands the angles over in radians
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        const wy3d::SketchCurve* pAxis = wy3d::SketchCurve::cast(pDb->getElement(axisId));
        ASSERT_NE(pSketch, nullptr);
        ASSERT_NE(pAxis, nullptr);
        EXPECT_EQ(wy3d::RevolvedSheet::create(pTrans, pSketch, pAxis, 0.0, wy3d::TWO_PI, pSheet), wy::ErrorStatus::Ok);
        ASSERT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSheet, nullptr);

    const std::string className = wy3d::RevolvedSheet::classInfo()->className();

    // Read back in degrees
    {
        wydb::ParameterValueUPtr pVal = pSheet->getParameterValue(className, wy3d::ParamNames::REVOLUTION_PARAM_START_ANGLE);
        ASSERT_NE(pVal, nullptr);
        EXPECT_TRUE(pVal->isDouble());
        EXPECT_NEAR(pVal->asDouble(), 0.0, 1e-9);
    }
    {
        wydb::ParameterValueUPtr pVal = pSheet->getParameterValue(className, wy3d::ParamNames::REVOLUTION_PARAM_END_ANGLE);
        ASSERT_NE(pVal, nullptr);
        EXPECT_TRUE(pVal->isDouble());
        EXPECT_NEAR(pVal->asDouble(), 360.0, 1e-9);
    }

    // Written in degrees, stored as radians
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::RevolvedSheet* pWrite = wy3d::RevolvedSheet::cast(pTrans->getElementForWrite(pSheet->getId()));
        ASSERT_NE(pWrite, nullptr);

        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::REVOLUTION_PARAM_START_ANGLE,
            *wydb::ParameterValue::createInteger(1)), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::REVOLUTION_PARAM_END_ANGLE,
            *wydb::ParameterValue::createDouble(90.0)), wy::ErrorStatus::Ok);

        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_NEAR(pSheet->getStartAngle(), 0.0, 1e-9);
    EXPECT_NEAR(pSheet->getEndAngle(), wy3d::PI_2, 1e-9);

    // And the round trip comes back as written
    wydb::ParameterValueUPtr pVal = pSheet->getParameterValue(className, wy3d::ParamNames::REVOLUTION_PARAM_END_ANGLE);
    ASSERT_NE(pVal, nullptr);
    EXPECT_NEAR(pVal->asDouble(), 90.0, 1e-9);
}
