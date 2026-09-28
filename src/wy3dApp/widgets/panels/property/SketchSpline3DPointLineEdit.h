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

#ifndef WY3DAPP_SKETCH_SPLINE3D_POINT_LINE_EDIT_H
#define WY3DAPP_SKETCH_SPLINE3D_POINT_LINE_EDIT_H

#include "ParamLineEdit.h"

class PropertyEditorWidget;
class SketchSpline3DPointsEditor;

// One value of one 3D sketch spline point; an instance handles a single field of it.
// Such a value is not a parameter, so this only overrides "which value to read/write",
// the same way SketchSplinePointLineEdit does. Transaction, error reporting and the regen
// suppression around the commit all come from ParamLineEdit.
// A 3D spline point is a model space point with no tangency of its own, so where the 2D
// version has two coordinates plus the two tangent values, this one has the three
// coordinates.
class SketchSpline3DPointLineEdit : public ParamLineEdit
{
    Q_OBJECT
public:
    enum class Field { X = 1, Y = 2, Z = 3 };

    SketchSpline3DPointLineEdit(Field field, wydb::ParameterValueUPtr&& pParamValue,
        SketchSpline3DPointsEditor* pPointsEditor, PropertyEditorWidget* parent);

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
    SketchSpline3DPointsEditor* _pPointsEditor;
};

#endif // WY3DAPP_SKETCH_SPLINE3D_POINT_LINE_EDIT_H
