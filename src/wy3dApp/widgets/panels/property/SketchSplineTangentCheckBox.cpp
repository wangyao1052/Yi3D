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

#include "SketchSplineTangentCheckBox.h"
#include <cassert>

#include <QSignalBlocker>

#include <wy3dSketchSpline.h>

#include "SketchSplinePointsEditor.h"

SketchSplineTangentCheckBox::SketchSplineTangentCheckBox(wydb::ParameterValueUPtr&& pParamValue,
    SketchSplinePointsEditor* pPointsEditor, PropertyEditorWidget* parent)
    : ParamCheckBox("", "", std::move(pParamValue), true, parent)
    , _pPointsEditor(pPointsEditor)
{
    // ParamCheckBox commits in its own stateChanged handler but, unlike ParamLineEdit, does not
    // refresh the panel afterwards; the rows around it read the flag it has just changed, so
    // they are re-evaluated from here. Slots run in connection order and ParamCheckBox connects
    // in its constructor, so the commit has already happened by the time this one runs.
    QObject::connect(this, &QCheckBox::stateChanged, this, &SketchSplineTangentCheckBox::onTangentStateChanged);
}

void SketchSplineTangentCheckBox::onTangentStateChanged()
{
    if (_pPointsEditor) _pPointsEditor->refresh();
}

bool SketchSplineTangentCheckBox::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem || !_pPointsEditor)
    {
        assert(false);
        return false;
    }
    wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pElem);
    if (!pSketchSpline)
    {
        assert(false);
        return false;
    }

    const std::size_t index = _pPointsEditor->getCurrPointIndex();
    std::vector<wy3d::SketchSpline::Tangent> tangents = pSketchSpline->getTangents();
    if (index >= tangents.size())
    {
        assert(false);
        return false;
    }

    // The tangent the point carries is the one the curve runs with there, so nothing about the
    // shape moves when the constraint takes over from it
    tangents[index].isDriving = paramValue.asBoolean();
    return wy::ErrorStatus::Ok == pSketchSpline->setTangents(tangents);
}

void SketchSplineTangentCheckBox::refresh()
{
    bool value = false;

    const wy3d::SketchSpline* pSketchSpline = _pPointsEditor ? _pPointsEditor->getSplineFromDb() : nullptr;
    if (pSketchSpline) value = pSketchSpline->getTangentAt(_pPointsEditor->getCurrPointIndex()).isDriving;

    const QSignalBlocker blocker(this);
    this->setChecked(value);
}
