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

#include "SketchArcDimensionGizmo.h"

#include <QCoreApplication>
#include <QString>

#include <wy3dImpl.h>

#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchArcDimensionGizmo::SketchArcDimensionGizmo(const wy3d::SketchArc* pSketchArc)
    : SketchDimensionGizmo({ pSketchArc->getId() }, pSketchArc->getParent())
{
}

SketchArcDimensionGizmo::~SketchArcDimensionGizmo()
{
}

QString SketchArcDimensionGizmo::getDimensionName() const
{
    return QCoreApplication::translate("SketchArcDimensionGizmo", "Radius");
}

bool SketchArcDimensionGizmo::readDimension(
    double& value,
    wy::Vector2& anchorUv,
    SketchDimensionPlacement& placement,
    wy::Vector2& radialDirectionUv) const
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    const wy3d::SketchArc* pArc =
        wy3d::SketchArc::cast(pDb->getElement(this->getFirstElementId()));
    if (!pArc || pArc->getRadius() < wy3d::TOL)
    {
        return false;
    }

    if (!this->readSingleParameter(wy3d::SketchArc::PARAM_RADIUS, value))
    {
        return false;
    }

    const wy::Vector2 middle = pArc->getMiddlePoint();
    const wy::Vector2 radial = middle - pArc->getCenter();
    if (radial.length() < wy3d::TOL)
    {
        return false;
    }

    anchorUv = middle;
    placement = SketchDimensionPlacement::Radial;
    radialDirectionUv = radial.normalized();
    return true;
}

bool SketchArcDimensionGizmo::applyValue(double value)
{
    return this->writeSingleParameter(wy3d::SketchArc::PARAM_RADIUS, value);
}

bool SketchArcDimensionGizmo::isValidValue(double value) const
{
    return value >= wy3d::kMinValue && value <= wy3d::kMaxValue;
}

} // namespace wy3dApp
