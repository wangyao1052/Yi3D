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

#ifndef WY3DAPP_PARAM_COMBO_BOX_H
#define WY3DAPP_PARAM_COMBO_BOX_H

#include <QComboBox>
#include <string>

#include <wydbParameter.h>
#include <wydbElement.h>
#include <wy3dParamEnumDef.h>

class PropertyEditorWidget;

class ParamComboBox : public QComboBox
{
    Q_OBJECT
public:
    ParamComboBox(
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
    // 初始化选项
    void initItems();

private slots:
    void onCurrentIndexChanged(int index);

protected:
    // 参数类名
    std::string _className;
    // 参数名
    std::string _paramName;
    // 初始参数值
    wydb::ParameterValueUPtr _pInitParamValue;
    // 是否都是相同值
    bool _isAllTheSameValue;
    // 枚举定义
    wy3d::ParamEnumDef _enumDef;
    // 是否正在程序中更新 (避免信号循环)
    bool _updating;
};

#endif // WY3DAPP_PARAM_COMBO_BOX_H
