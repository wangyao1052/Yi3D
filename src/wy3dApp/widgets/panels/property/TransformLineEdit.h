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

#ifndef WY3DAPP_TRANSFORM_LINE_EDIT_H
#define WY3DAPP_TRANSFORM_LINE_EDIT_H

#include "ParamLineEdit.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class PropertyEditorWidget;

enum class TransformUnit
{
    PosX = 1,
    PosY = 2,
    PosZ = 3,
    RotX = 4,
    RotY = 5,
    RotZ = 6,
};

class TransformLineEdit : public ParamLineEdit
{
    Q_OBJECT
public:
    TransformLineEdit(
        TransformUnit unit,
        wydb::ParameterValueUPtr&& pParamValue,
        bool isAllTheSameValue,
        PropertyEditorWidget* parent);

    virtual QSize sizeHint() const override
    {
        return QSize(50, 15);
    }

protected:
    virtual wy::ErrorStatus modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue) override;

private:
    // 获取当前参数值
    virtual void getCurrParamValueFromDb(bool& isAllTheSameValue, wydb::ParameterValueUPtr& pParamValue);

private:
    TransformUnit _unit;
};

} // namespace wy3dApp

#endif // WY3DAPP_TRANSFORM_LINE_EDIT_H
