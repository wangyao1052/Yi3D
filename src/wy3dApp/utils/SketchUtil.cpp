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

#include "SketchUtil.h"
#include <QCoreApplication>
#include <QStringList>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wy3dErrorCode.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchProfile.h>
#include <wy3dSketchProfile_Revolution.h>
#include <wy3dSketchPath.h>
#include "translation/ErrorCodeTranslation.h"

bool SketchUtil::hasUnusedSketch(const wydb::Database* pDb)
{
    if (!pDb)
    {
        assert(false);
        return false;
    }
    auto iter = pDb->createIterator();
    for (; !iter.isDone(); iter.moveNext())
    {
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(iter.current()));
        if (!pSketch)
        {
            continue;
        }
        if (pSketch->getParent().isNull())
        {
            return true;
        }
    }

    return false;
}

wy::Vector3 SketchUtil::getSketchOrigin(const wydb::Database* pDb, const wydb::ElementId& sketchId)
{
    if (!pDb)
    {
        assert(false);
        return wy::Vector3::kZero;
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch)
    {
        assert(false);
        return wy::Vector3::kZero;
    }

    return pSketch->getPlane().getOrigin();
}

bool SketchUtil::isValidExtrusionProfile(const wy3d::Sketch& sketch, QString& error)
{
    return isValidProfile(sketch, error);
}

bool SketchUtil::isValidRevolutionProfile(const wy3d::Sketch& sketch, QString& error)
{
    wy3d::SketchProfile_Revolution sketchProfile(&sketch);
    if (sketchProfile.check()) return true;

    wy3d::ErrorCode errorCode = wy3d::ErrorCode::PROFILE_InvalidProfile;
    std::shared_ptr<wy3d::SketchError> pError = sketchProfile.getError();
    if (pError) errorCode = pError->type;
    error = ErrorCodeTranslation::instance().getErrorCodeDescription(errorCode);
    if (pError && !pError->ids.empty())
    {
        QStringList idStrs;
        for (const wydb::ElementId& id : pError->ids)
        {
            idStrs << QString::number(id.value());
        }
        error += "\n" + QCoreApplication::translate("SketchUtil", "Element IDs: %1")
            .arg(idStrs.join(", "));
    }

    return false;
}

bool SketchUtil::isValidSweepPath(const wy3d::Sketch& sketch, QString& error)
{
    return isValidPath(sketch, error);
}

bool SketchUtil::isValidSweepProfile(const wy3d::Sketch& sketch, QString& error)
{
    return isValidProfile(sketch, error);
}

bool whetherSketchHasOnlyOneSketchPoint(const wy3d::Sketch& sketch)
{
    unsigned int numOfEntities(0);
    for (auto iter = sketch.createIterator(); !iter.isDone(); iter.moveNext())
    {
        ++numOfEntities;
        if (numOfEntities > 1) // 多于1个图元
        {
            return false;
        }
    }
    if (1 != numOfEntities) // 0个图元
    {
        return false;
    }

    // 1个图元
    const wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(
        sketch.getDatabase()->getElement(sketch.createIterator().current()));
    return pSketchPoint != nullptr;
}

bool SketchUtil::isValidLoftProfile(const wy3d::Sketch& sketch, QString& error)
{
    if (whetherSketchHasOnlyOneSketchPoint(sketch)) return true;
    else return isValidProfile(sketch, error);
}

bool SketchUtil::isValidHelixProfile(const wy3d::Sketch& sketch, QString& error)
{
    wydb::Database* pDb = sketch.getDatabase();
    if (!pDb)
    {
        assert(false);
        error = ErrorCodeTranslation::instance().getErrorCodeDescription(
            wy3d::ErrorCode::PROFILE_InvalidProfile);
        return false;
    }

    // 遍历草图找到圆
    const wy3d::SketchCircle* pCircle(nullptr);
    for (auto iter = sketch.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current());
        if (!pElem)
        {
            assert(false);
            continue;
        }

        const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pElem);
        if (!pCurve) // 比如点
        {
            continue;
        }
        if (pCurve->isConstruction()) // 构造线不影响
        {
            continue;
        }

        const wyrx::ClassInfo* classInfo = pCurve->getClassInfo();
        if (classInfo == wy3d::SketchCenterLine::classInfo()) // 中心线不影响
        {
            continue;
        }

        if (classInfo != wy3d::SketchCircle::classInfo()) // 有其它实曲线
        {
            error = ErrorCodeTranslation::instance().getErrorCodeDescription(
                wy3d::ErrorCode::HELIX_InvalidSketch);
            return false;
        }

        if (pCircle) // 有多个圆
        {
            error = ErrorCodeTranslation::instance().getErrorCodeDescription(
                wy3d::ErrorCode::HELIX_InvalidSketch);
            return false;
        }
        pCircle = wy3d::SketchCircle::cast(pCurve);
        assert(pCircle);
    }
    if (!pCircle) // 没有圆
    {
        error = ErrorCodeTranslation::instance().getErrorCodeDescription(
            wy3d::ErrorCode::HELIX_InvalidSketch);
        return false;
    }

    return true;
}

bool SketchUtil::isValidProfile(const wy3d::Sketch& sketch, QString& error)
{
    wy3d::SketchProfile sketchProfile(&sketch);
    if (sketchProfile.check()) return true;

    wy3d::ErrorCode errorCode = wy3d::ErrorCode::PROFILE_InvalidProfile;
    std::shared_ptr<wy3d::SketchError> pError = sketchProfile.getError();
    if (pError)
    {
        errorCode = pError->type;
    }
    error = ErrorCodeTranslation::instance().getErrorCodeDescription(errorCode);
    if (pError && !pError->ids.empty())
    {
        QStringList idStrs;
        for (const wydb::ElementId& id : pError->ids)
        {
            idStrs << QString::number(id.value());
        }
        error += "\n" + QCoreApplication::translate("SketchUtil", "Element IDs: %1")
            .arg(idStrs.join(", "));
    }

    return false;
}

bool SketchUtil::isValidPath(const wy3d::Sketch& sketch, QString& error)
{
    wy3d::SketchPath sketchPath(&sketch);
    if (sketchPath.check()) return true;

    wy3d::ErrorCode errorCode = wy3d::ErrorCode::PATH_InvalidPath;
    std::shared_ptr<wy3d::SketchError> pError = sketchPath.getError();
    if (pError)
    {
        errorCode = pError->type;
    }
    error = ErrorCodeTranslation::instance().getErrorCodeDescription(errorCode);
    if (pError && !pError->ids.empty())
    {
        QStringList idStrs;
        for (const wydb::ElementId& id : pError->ids)
        {
            idStrs << QString::number(id.value());
        }
        error += "\n" + QCoreApplication::translate("SketchUtil", "Element IDs: %1")
            .arg(idStrs.join(", "));
    }

    return false;
}