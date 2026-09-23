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

#ifndef WY3DAPP_SKETCH_SPLINE_POINT_LINE_EDIT_H
#define WY3DAPP_SKETCH_SPLINE_POINT_LINE_EDIT_H

#include "ParamLineEdit.h"

class PropertyEditorWidget;
class SketchSplinePointsEditor;

// One value of one spline point; an instance handles a single field of it.
// Such a value is not a parameter, so this only overrides "which value to read/write",
// the same way TransformLineEdit does. Transaction, error reporting and the regen
// suppression around the commit all come from ParamLineEdit.
// The tangent angle is a degrees value here: the core stores radians.
class SketchSplinePointLineEdit : public ParamLineEdit
{
    Q_OBJECT
public:
    enum class Field { X = 1, Y = 2, TangentAngle = 3, TangentWeight = 4 };

    SketchSplinePointLineEdit(Field field, wydb::ParameterValueUPtr&& pParamValue,
        SketchSplinePointsEditor* pPointsEditor, PropertyEditorWidget* parent);

    virtual QSize sizeHint() const override
    {
        return QSize(50, 15);
    }

protected:
    virtual wy::ErrorStatus modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue) override;

private:
    virtual void getCurrParamValueFromDb(bool& isAllTheSameValue, wydb::ParameterValueUPtr& pOutParamValue) override;

private:
    Field _field;
    SketchSplinePointsEditor* _pPointsEditor;
};

#endif // WY3DAPP_SKETCH_SPLINE_POINT_LINE_EDIT_H
