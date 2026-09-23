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

#include "SketchSplinePointLineEdit.h"
#include <cassert>

#include <wyVector2.h>
#include <wy3dMath.h>
#include <wy3dSketchSpline.h>

#include "SketchSplinePointsEditor.h"

SketchSplinePointLineEdit::SketchSplinePointLineEdit(Field field, wydb::ParameterValueUPtr&& pParamValue,
    SketchSplinePointsEditor* pPointsEditor, PropertyEditorWidget* parent)
    : ParamLineEdit("", "", std::move(pParamValue), true, false, parent)
    , _field(field)
    , _pPointsEditor(pPointsEditor)
{
}

wy::ErrorStatus SketchSplinePointLineEdit::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem || !_pPointsEditor)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }
    wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pElem);
    if (!pSketchSpline)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    const std::size_t index = _pPointsEditor->getCurrPointIndex();
    const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
    if (index >= points.size())
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    if (Field::X == _field || Field::Y == _field)
    {
        std::vector<wy::Vector2> newPoints = points;
        // A closed spline has two coincident ends, each with its own index: editing one of
        // them deliberately opens the spline
        if (Field::X == _field) newPoints[index].setX(paramValue.asDouble());
        else newPoints[index].setY(paramValue.asDouble());
        return pSketchSpline->setPoints(newPoints);
    }

    // Each point holds the tangent the curve runs with there, so the edit starts from the table
    std::vector<wy3d::SketchSpline::Tangent> tangents = pSketchSpline->getTangents();
    if (index >= tangents.size())
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    wy3d::SketchSpline::Tangent& tangent = tangents[index];
    if (Field::TangentAngle == _field) tangent.angle = wy3d::degreesToRadians(paramValue.asDouble());
    // The magnitude of a tangent is a length, so the spline refuses one that is not positive
    else tangent.magnitude = paramValue.asDouble();
    return pSketchSpline->setTangents(tangents);
}

void SketchSplinePointLineEdit::getCurrParamValueFromDb(
    bool& isAllTheSameValue, wydb::ParameterValueUPtr& pOutParamValue)
{
    isAllTheSameValue = true;
    pOutParamValue = nullptr;

    if (!_pPointsEditor)
    {
        assert(false);
        return;
    }
    const wy3d::SketchSpline* pSketchSpline = _pPointsEditor->getSplineFromDb();
    if (!pSketchSpline)
    {
        assert(false);
        return;
    }

    const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
    const std::size_t index = _pPointsEditor->getCurrPointIndex();
    if (index >= points.size())
    {
        assert(false);
        return;
    }

    if (Field::X == _field || Field::Y == _field)
    {
        const wy::Vector2& point = points[index];
        pOutParamValue = wydb::ParameterValue::createDouble(Field::X == _field ? point.x() : point.y());
        return;
    }

    // A point that does not drive the shape holds the tangent the curve runs with there, so the
    // angle and the magnitude are the ones the shape is doing
    const wy3d::SketchSpline::Tangent tangent = pSketchSpline->getTangentAt(index);
    pOutParamValue = wydb::ParameterValue::createDouble(Field::TangentAngle == _field
        ? wy3d::radiansToDegrees(tangent.angle) : tangent.magnitude);
}
