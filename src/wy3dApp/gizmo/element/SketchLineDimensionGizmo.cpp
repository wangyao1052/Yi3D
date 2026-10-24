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

#include "SketchLineDimensionGizmo.h"

#include <QCoreApplication>
#include <QString>

#include <wy3dImpl.h>

#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

const char* lineParameterName(SketchLineDimensionGizmo::Type type)
{
    return SketchLineDimensionGizmo::Type::Length == type
        ? wy3d::SketchLine::PARAM_LENGTH
        : wy3d::SketchLine::PARAM_ANGLE;
}

} // namespace

SketchLineDimensionGizmo::SketchLineDimensionGizmo(const wy3d::SketchLine* pSketchLine,
                                                   Type type)
    : SketchDimensionGizmo({ pSketchLine->getId() }, pSketchLine->getParent())
    , _type(type)
{
}

SketchLineDimensionGizmo::~SketchLineDimensionGizmo()
{
}

QString SketchLineDimensionGizmo::getDimensionName() const
{
    return Type::Length == _type
        ? QCoreApplication::translate("SketchLineDimensionGizmo", "Length")
        : QCoreApplication::translate("SketchLineDimensionGizmo", "Angle");
}

bool SketchLineDimensionGizmo::readDimension(
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

    const wy3d::SketchLine* pLine =
        wy3d::SketchLine::cast(pDb->getElement(this->getFirstElementId()));
    if (!pLine || pLine->getLength() < wy3d::TOL)
    {
        return false;
    }

    if (!this->readSingleParameter(lineParameterName(_type), value))
    {
        return false;
    }

    const wy::Vector2 start = pLine->getStartPoint();
    const wy::Vector2 end = pLine->getEndPoint();
    const wy::Vector2 direction = (end - start).normalized();

    if (Type::Length == _type)
    {
        // Beyond the end, along the line
        anchorUv = end;
        radialDirectionUv = direction;
    }
    else
    {
        // Beside the middle point; the upper side puts a vertical line's label
        // on the right, independent of the stored direction
        const wy::Vector2 normal(-direction.y(), direction.x());
        const bool isUpper = 0.0 < normal.y() || (0.0 == normal.y() && 0.0 < normal.x());
        anchorUv = (start + end) * 0.5;
        radialDirectionUv = isUpper ? normal : wy::Vector2(-normal.x(), -normal.y());
    }
    placement = SketchDimensionPlacement::Radial;
    return true;
}

bool SketchLineDimensionGizmo::applyValue(double value)
{
    return this->writeSingleParameter(lineParameterName(_type), value);
}

bool SketchLineDimensionGizmo::isValidValue(double value) const
{
    return Type::Angle == _type || value >= wy3d::kMinValue;
}

} // namespace wy3dApp
