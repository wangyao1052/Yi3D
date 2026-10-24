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

#include "SketchCircleDimensionGizmo.h"

#include <QCoreApplication>
#include <QString>

#include <wy3dImpl.h>

#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchCircleDimensionGizmo::SketchCircleDimensionGizmo(
    const wy3d::SketchCircle* pSketchCircle)
    : SketchDimensionGizmo({ pSketchCircle->getId() }, pSketchCircle->getParent())
{
}

SketchCircleDimensionGizmo::~SketchCircleDimensionGizmo()
{
}

QString SketchCircleDimensionGizmo::getDimensionName() const
{
    return QCoreApplication::translate("SketchCircleDimensionGizmo", "Diameter");
}

bool SketchCircleDimensionGizmo::readDimension(
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

    const wy3d::SketchCircle* pCircle =
        wy3d::SketchCircle::cast(pDb->getElement(this->getFirstElementId()));
    if (!pCircle || pCircle->getRadius() < wy3d::TOL)
    {
        return false;
    }

    if (!this->readSingleParameter(wy3d::SketchCircle::PARAM_DIAMETER, value))
    {
        return false;
    }

    anchorUv = pCircle->getCenter() + wy::Vector2(pCircle->getRadius(), 0.0);
    placement = SketchDimensionPlacement::Radial;
    radialDirectionUv = wy::Vector2(1.0, 0.0);
    return true;
}

bool SketchCircleDimensionGizmo::applyValue(double value)
{
    return this->writeSingleParameter(wy3d::SketchCircle::PARAM_DIAMETER, value);
}

bool SketchCircleDimensionGizmo::isValidValue(double value) const
{
    return value >= 2.0 * wy3d::kMinValue && value <= 2.0 * wy3d::kMaxValue;
}

} // namespace wy3dApp
