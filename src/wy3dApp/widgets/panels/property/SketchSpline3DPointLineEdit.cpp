///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchSpline3DPointLineEdit.h"
#include <cassert>

#include <wyVector3.h>
#include <wy3dSketchSpline3D.h>

#include "SketchSpline3DPointsEditor.h"

SketchSpline3DPointLineEdit::SketchSpline3DPointLineEdit(Field field, wydb::ParameterValueUPtr&& pParamValue,
    SketchSpline3DPointsEditor* pPointsEditor, PropertyEditorWidget* parent)
    : ParamLineEdit("", "", std::move(pParamValue), true, false, parent)
    , _field(field)
    , _pPointsEditor(pPointsEditor)
{
}

wy::ErrorStatus SketchSpline3DPointLineEdit::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem || !_pPointsEditor)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }
    wy3d::SketchSpline3D* pSketchSpline3D = wy3d::SketchSpline3D::cast(pElem);
    if (!pSketchSpline3D)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    const std::size_t index = _pPointsEditor->getCurrPointIndex();
    const std::vector<wy::Vector3>& points = pSketchSpline3D->getPoints();
    if (index >= points.size())
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    std::vector<wy::Vector3> newPoints = points;
    // A closed spline has two coincident ends, each with its own index: editing one of
    // them deliberately opens the spline
    if (Field::X == _field) newPoints[index].setX(paramValue.asDouble());
    else if (Field::Y == _field) newPoints[index].setY(paramValue.asDouble());
    else newPoints[index].setZ(paramValue.asDouble());
    return pSketchSpline3D->setPoints(newPoints);
}

void SketchSpline3DPointLineEdit::getCurrParamValueFromDb(
    bool& isAllTheSameValue, wydb::ParameterValueUPtr& pOutParamValue)
{
    isAllTheSameValue = true;
    pOutParamValue = nullptr;

    if (!_pPointsEditor)
    {
        assert(false);
        return;
    }
    const wy3d::SketchSpline3D* pSketchSpline3D = _pPointsEditor->getSplineFromDb();
    if (!pSketchSpline3D)
    {
        assert(false);
        return;
    }

    const std::vector<wy::Vector3>& points = pSketchSpline3D->getPoints();
    const std::size_t index = _pPointsEditor->getCurrPointIndex();
    if (index >= points.size())
    {
        assert(false);
        return;
    }

    const wy::Vector3& point = points[index];
    if (Field::X == _field) pOutParamValue = wydb::ParameterValue::createDouble(point.x());
    else if (Field::Y == _field) pOutParamValue = wydb::ParameterValue::createDouble(point.y());
    else pOutParamValue = wydb::ParameterValue::createDouble(point.z());
}
