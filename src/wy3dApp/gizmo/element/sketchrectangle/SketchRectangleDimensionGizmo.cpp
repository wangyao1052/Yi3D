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

#include "SketchRectangleDimensionGizmo.h"

#include <array>
#include <cassert>
#include <utility>

#include <QCoreApplication>
#include <QString>

#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>

#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchRectangleDimensionGizmo::SketchRectangleDimensionGizmo(
    std::shared_ptr<const SketchRectangleInfo> rect,
    SketchRectangleDimension dimension)
    : SketchDimensionGizmo(
          std::vector<wydb::ElementId>(rect->lineIds.begin(), rect->lineIds.end()),
          rect->sketchId)
    , _rect(std::move(rect))
    , _dimension(dimension)
{
}

SketchRectangleDimensionGizmo::~SketchRectangleDimensionGizmo()
{
}

QString SketchRectangleDimensionGizmo::getDimensionName() const
{
    return SketchRectangleDimension::Width == _dimension
        ? QCoreApplication::translate("SketchRectangleDimensionGizmo", "Width")
        : QCoreApplication::translate("SketchRectangleDimensionGizmo", "Height");
}

bool SketchRectangleDimensionGizmo::readDimension(
    double& value,
    wy::Vector2& anchorUv,
    SketchDimensionPlacement& placement,
    wy::Vector2& radialDirectionUv) const
{
    // Live geometry first; the snapshot is only a fallback for a transient failure
    std::array<wy::Vector2, 4> corners = _rect->corners;
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    std::array<wy::Vector2, 4> liveCorners{};
    if (pDb && readSketchRectangleCorners(pDb, _rect->lineIds, liveCorners, nullptr, _rect->distTol))
    {
        corners = liveCorners;
    }

    const RectangleDimensionLabel label = computeRectangleDimensionLabel(corners, _dimension);
    if (!label.isValid)
    {
        return false;
    }

    value = label.value;
    anchorUv = label.edgeMid;
    placement = SketchRectangleDimension::Width == _dimension
        ? SketchDimensionPlacement::Below
        : SketchDimensionPlacement::Right;
    radialDirectionUv = wy::Vector2();
    return true;
}

bool SketchRectangleDimensionGizmo::applyValue(double value)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    std::array<wy::Vector2, 4> corners{};
    std::array<bool, 4> lineStartAtCorner{};
    if (!readSketchRectangleCorners(pDb, _rect->lineIds, corners, &lineStartAtCorner, _rect->distTol))
    {
        return false;
    }

    const RectangleDimensionLabel label = computeRectangleDimensionLabel(corners, _dimension);
    if (!label.isValid)
    {
        return false;
    }

    const std::array<wy::Vector2, 4> newCorners =
        computeRectangleResizedCorners(corners, _dimension, value, wy3d::kMinValue);

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    if (!pTransMgr)
    {
        return false;
    }

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans)
    {
        return false;
    }

    wy3d::SketchLine* pLines[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        pLines[i] = wy3d::SketchLine::cast(pTrans->getElementForWrite(_rect->lineIds[i]));
        if (!pLines[i])
        {
            pTransMgr->abortTransaction();
            return false;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const wy::Vector2& corner = newCorners[i];
        const wy::Vector2& nextCorner = newCorners[(i + 1) % 4];
        const bool startAtCorner = lineStartAtCorner[i];

        const wy::ErrorStatus startError = startAtCorner
            ? pLines[i]->setStartPoint(corner) : pLines[i]->setStartPoint(nextCorner);
        const wy::ErrorStatus endError = startAtCorner
            ? pLines[i]->setEndPoint(nextCorner) : pLines[i]->setEndPoint(corner);
        if (wy::ErrorStatus::Ok != startError || wy::ErrorStatus::Ok != endError)
        {
            pTransMgr->abortTransaction();
            return false;
        }
    }

    return wy::ErrorStatus::Ok == pTransMgr->endTransaction();
}

} // namespace wy3dApp
