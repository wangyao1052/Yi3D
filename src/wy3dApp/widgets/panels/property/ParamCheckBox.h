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

#ifndef WY3DAPP_PARAM_CHECK_BOX_H
#define WY3DAPP_PARAM_CHECK_BOX_H

#include <QCheckBox>
#include <wydbElement.h>
#include <wydbParameter.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class PropertyEditorWidget;

class ParamCheckBox : public QCheckBox
{
    Q_OBJECT
public:
    ParamCheckBox(
        const std::string& className,
        const std::string& paramName,
        wydb::ParameterValueUPtr&& pParamValue,
        bool isAllTheSameValue,
        PropertyEditorWidget* parent);

protected:
    // 修改元素值(整体)
    bool modifyElementsByTransaction(const wydb::ParameterValue& paramValue);
    // 修改元素值
    virtual bool modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue);

private:
    // 初始化
    bool initValue();

private slots:
    void onStateChanged(int state);

protected:
    // 参数类名
    std::string _className;
    // 参数名
    std::string _paramName;
    // 初始参数值
    wydb::ParameterValueUPtr _pInitParamValue;
    // 是否都是相同值
    bool _isAllTheSameValue;
};

}

#endif // WY3DAPP_PARAM_CHECK_BOX_H
