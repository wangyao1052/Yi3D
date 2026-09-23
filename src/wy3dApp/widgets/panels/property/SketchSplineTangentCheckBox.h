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

#ifndef WY3DAPP_SKETCH_SPLINE_TANGENT_CHECK_BOX_H
#define WY3DAPP_SKETCH_SPLINE_TANGENT_CHECK_BOX_H

#include "ParamCheckBox.h"

class PropertyEditorWidget;
class SketchSplinePointsEditor;

// The check box of the tangency rows: whether the current point's tangent drives the shape. It is
// not a parameter, so this only overrides "which value to read/write", the same way
// SketchSplinePointLineEdit does. Transaction and the regen suppression around the commit come
// from ParamCheckBox.
class SketchSplineTangentCheckBox : public ParamCheckBox
{
    Q_OBJECT
public:
    SketchSplineTangentCheckBox(wydb::ParameterValueUPtr&& pParamValue,
        SketchSplinePointsEditor* pPointsEditor, PropertyEditorWidget* parent);

    // Reads the current value back from the database. The caller blocks the signals.
    void refresh();

protected:
    virtual bool modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue) override;

private slots:
    void onTangentStateChanged();

private:
    SketchSplinePointsEditor* _pPointsEditor;
};

#endif // WY3DAPP_SKETCH_SPLINE_TANGENT_CHECK_BOX_H
