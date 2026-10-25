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

#include "SketchPolygonDimensionGizmo.h"

#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QString>

#include <wy3dImpl.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "SketchPolygonCircleGhostGizmo.h"
#include "utils/SketchClosedLoop.h"
#include "utils/SketchPolygonDimensionMath.h"
#include "utils/SketchPolygonDragMath.h"
#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchPolygonDimensionGizmo::SketchPolygonDimensionGizmo(
    std::shared_ptr<const SketchPolygonInfo> polygon,
    std::weak_ptr<SketchPolygonCircleGhostGizmo> circleGhost)
    : SketchDimensionGizmo(
          std::vector<wydb::ElementId>(polygon->lineIds.begin(), polygon->lineIds.end()),
          polygon->sketchId)
    , _polygon(std::move(polygon))
    , _pCircleGhost(std::move(circleGhost))
{
    assert(_polygon);
    assert(!_pCircleGhost.expired());
}

SketchPolygonDimensionGizmo::~SketchPolygonDimensionGizmo()
{
}

QString SketchPolygonDimensionGizmo::getDimensionName() const
{
    return QCoreApplication::translate("SketchPolygonDimensionGizmo", "Radius");
}

QStringList SketchPolygonDimensionGizmo::getEditorChoices() const
{
    return { QCoreApplication::translate("SketchPolygonDimensionGizmo", "Inscribed"),
             QCoreApplication::translate("SketchPolygonDimensionGizmo", "Circumscribed") };
}

int SketchPolygonDimensionGizmo::getEditorChoice() const
{
    return static_cast<int>(_dimension);
}

void SketchPolygonDimensionGizmo::setEditorChoice(int index)
{
    if (index < 0 || index > 1) return;
    const SketchPolygonDimension dimension = static_cast<SketchPolygonDimension>(index);
    if (dimension == _dimension) return;
    _dimension = dimension;

    // Only the reading changes: no transaction, the circle merely redraws
    if (const std::shared_ptr<SketchPolygonCircleGhostGizmo> pCircleGhost =
            _pCircleGhost.lock())
    {
        pCircleGhost->setDimension(dimension);
    }
}

bool SketchPolygonDimensionGizmo::readDimension(
    double& value,
    wy::Vector2& anchorUv,
    SketchDimensionPlacement& placement,
    wy::Vector2& radialDirectionUv) const
{
    // Live geometry first; the snapshot is only a fallback for a transient failure
    std::vector<wy::Vector2> corners = _polygon->corners;
    wy::Vector2 center = _polygon->center;
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (pDb)
    {
        std::vector<wy::Vector2> liveCorners;
        if (readSketchClosedLoopCorners(pDb, _polygon->lineIds, liveCorners,
                                        nullptr, _polygon->distTol))
        {
            const PolygonRegularity regularity = validateRegularPolygon(
                liveCorners, _polygon->angleTol, _polygon->distTol);
            if (!regularity.isValid) return false;
            corners = std::move(liveCorners);
            center = regularity.center;
        }
    }

    const PolygonDimensionLabel label =
        computePolygonDimensionLabel(corners, center, _dimension, _polygon->labelEdge);
    if (!label.isValid) return false;

    value = label.value;
    anchorUv = label.anchor;
    placement = SketchDimensionPlacement::Radial;
    radialDirectionUv = label.radialDirection;
    return true;
}

bool SketchPolygonDimensionGizmo::applyValue(double value)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return false;

    std::vector<wy::Vector2> corners;
    std::vector<bool> lineStartAtCorner;
    if (!readSketchClosedLoopCorners(pDb, _polygon->lineIds, corners, &lineStartAtCorner,
                                     _polygon->distTol))
    {
        return false;
    }

    const PolygonRegularity regularity =
        validateRegularPolygon(corners, _polygon->angleTol, _polygon->distTol);
    if (!regularity.isValid) return false;

    const int sides = _polygon->sides;
    const double minInradius = computePolygonMinInradius(sides, wy3d::kMinValue);
    const std::vector<wy::Vector2> newCorners = computePolygonDimensionResizedVertices(
        regularity.center, computePolygonRotation(corners, regularity.center),
        sides, _dimension, value, minInradius);
    if (static_cast<int>(newCorners.size()) != sides) return false;

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    if (!pTransMgr) return false;

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;

    if (!writeSketchClosedLoopCorners(pTrans, _polygon->lineIds, lineStartAtCorner, newCorners))
    {
        pTransMgr->abortTransaction();
        return false;
    }

    return wy::ErrorStatus::Ok == pTransMgr->endTransaction();
}

bool SketchPolygonDimensionGizmo::isValidValue(double value) const
{
    const double minValue =
        computePolygonMinDimensionValue(_dimension, _polygon->sides, wy3d::kMinValue);
    return std::isfinite(value) && value >= minValue;
}

} // namespace wy3dApp
