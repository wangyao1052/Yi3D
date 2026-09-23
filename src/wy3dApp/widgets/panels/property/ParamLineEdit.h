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

#ifndef WY3DAPP_PARAM_LINE_EDIT_H
#define WY3DAPP_PARAM_LINE_EDIT_H

#include <QWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>

#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbParameter.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <wyapDocManager.h>

class PropertyEditorWidget;

class ParamLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    ParamLineEdit(
        const std::string& className,
        const std::string& paramName,
        wydb::ParameterValueUPtr&& pParamValue,
        bool isAllTheSameValue,
        bool readonly,
        PropertyEditorWidget* parent);

    static void setWidgetFontSize(QWidget* pWidget, int pointSize = 12);

    // 刷新
    void refresh();

    // Qt folds a style sheet's color into the palette when the widget is polished, and this call
    // raises no style change, so the grey of the read only rule would neither come nor go with it:
    // wrapping it keeps the paint in step with the state. Call it on a ParamLineEdit, not a
    // QLineEdit, or the wrapper is passed by
    void setReadOnly(bool isReadOnly);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        {
            focusNextChild(); // 按Enter后跳转到下一个控件
            return;
        }

        // 其他按键继续正常处理
        QLineEdit::keyPressEvent(event);
    }

    // 修改元素值(整体)
    wy::ErrorStatus modifyElementsByTransaction(const wydb::ParameterValue& paramValue);
    // 修改元素值
    virtual wy::ErrorStatus modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue);

private:
    // 初始化文本值
    bool initText(bool isAllTheSameValue, const wydb::ParameterValue* pParamValue);
    // 获取当前参数值
    virtual void getCurrParamValueFromDb(bool& isAllTheSameValue, wydb::ParameterValueUPtr& pParamValue);

    // 由当前文本值创建参数值
    wydb::ParameterValueUPtr newParamValueFromText();

    // 解析表达式
    bool parserExpression(const QString& qstr, double& outValue);
    bool parserExpression(const QString& qstr, int& outValue);

private slots:
    void onEditingFinished();

protected:
    // 参数类名
    std::string _className;
    // 参数命
    std::string _paramName;
    // 初始参数值
    wydb::ParameterValueUPtr _pInitParamValue;
    // 是否都是相同值
    bool _isAllTheSameValue;
    //
    QString _lastText;
    wydb::ParameterValueUPtr _pLastParamValue;
};

#endif // WY3DAPP_PARAM_LINE_EDIT_H