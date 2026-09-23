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

#include "ParamLineEdit.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QStyle>

#include <muParser.h>

#include <wy3dMath.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFeature.h>
#include <wy3dSketchParamNames.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <wy3dSketchSpline.h>
#include <QHBoxLayout>
#include <QGridLayout>

#include "application/Application.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "PropertyEditorWidget.h"
#include "scene/Scene.h"
#include "utils/MessageBoxUtil.h"

void ParamLineEdit::setWidgetFontSize(QWidget* pWidget, int pointSize)
{
    if (pWidget)
    {
        QFont font = pWidget->font();
        font.setPointSize(pointSize);
        pWidget->setFont(font);
    }
};

ParamLineEdit::ParamLineEdit(const std::string& className, const std::string& paramName, wydb::ParameterValueUPtr&& pParamValue,
    bool isAllTheSameValue, bool readonly, PropertyEditorWidget* parent)
    : QLineEdit(parent)
    , _className(className)
    , _paramName(paramName)
    , _pInitParamValue(std::move(pParamValue))
    , _isAllTheSameValue(isAllTheSameValue)
{
    // added by wangyao 2025.06.30 {
    this->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid #C0C0C0;" // 去除3D显示效果
        "   background: white;"
        "}"
        "QLineEdit:read-only {" // 只读时文本灰色
        "   color: gray;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid #4A90E2;"  // 获得焦点时蓝色边框
        "}"
    );
    // }

    assert(parent);
    if (parent->isReadOnly())
    {
        this->setReadOnly(true);
    }
    if (readonly)
    {
        this->setReadOnly(true);
    }

    if (!_pInitParamValue)
    {
        assert(false);
        this->setText("");
        return;
    }

    setWidgetFontSize(this);

    this->initText(_isAllTheSameValue, _pInitParamValue.get());
    _lastText = this->text();
    _pLastParamValue = wydb::ParameterValueUPtr(_pInitParamValue->clone());
    this->connect(this, &QLineEdit::editingFinished, this, &ParamLineEdit::onEditingFinished);
}

void ParamLineEdit::setReadOnly(bool isReadOnly)
{
    QLineEdit::setReadOnly(isReadOnly);
    // The grey comes from the read only rule of the style sheet, which Qt folds into the palette
    // when the widget is polished, and setReadOnly raises no style change
    this->style()->unpolish(this);
    this->style()->polish(this);
}

void ParamLineEdit::refresh()
{
    bool isAllTheSameValue(true);
    wydb::ParameterValueUPtr pParamValue(nullptr);
    this->getCurrParamValueFromDb(isAllTheSameValue, pParamValue);

    this->initText(isAllTheSameValue, pParamValue.get());
    _lastText = this->text();
    if (pParamValue) _pLastParamValue = wydb::ParameterValueUPtr(pParamValue->clone());
    else _pLastParamValue = nullptr;
}

void ParamLineEdit::getCurrParamValueFromDb(bool& isAllTheSameValue, wydb::ParameterValueUPtr& pOutParamValue)
{
    isAllTheSameValue = true;
    pOutParamValue = nullptr;

    // 获取当前选择集
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty())
    {
        assert(false);
        return;
    }

    // 获取当前数据库
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }

    // 遍历当前选择集获取参数值
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current().getElementId());
        if (!pElem)
        {
            assert(false);
            continue;
        }

        wydb::ParameterValueUPtr pParamValue = pElem->getParameterValue(_className, _paramName);
        if (!pParamValue)
        {
            assert(false);
            continue;
        }

        if (!pOutParamValue)
        {
            pOutParamValue = std::move(pParamValue);
        }
        else
        {
            if (!pOutParamValue->equals(*pParamValue))
            {
                isAllTheSameValue = false;
                break;
            }
        }
    }
}

bool ParamLineEdit::initText(bool isAllTheSameValue, const wydb::ParameterValue* pParamValue)
{
    if (!isAllTheSameValue)
    {
        this->setText("-");
        return true;
    }
    if (!pParamValue)
    {
        assert(false);
        this->setText("*");
        return false;
    }

    if (pParamValue->isInteger())
    {
        this->setText(QString::number(pParamValue->asInteger()));
    }
    else if (pParamValue->isDouble())
    {
        this->setText(QString::number(pParamValue->asDouble()));
    }
    else if (pParamValue->isBoolean())
    {
        if (pParamValue->asBoolean())
            this->setText("1");
        else
            this->setText("0");
    }
    else if (pParamValue->isString())
    {
        this->setText(pParamValue->asString().c_str());
    }
    else if (pParamValue->isElementId())
    {
        wydb::ElementId id = pParamValue->asElementId();
        this->setText(QString::number(id.value()));
    }
    else
    {
        assert(false);
        this->setText("*");
        return false;
    }

    return true;
}

void ParamLineEdit::onEditingFinished()
{
    if (!_pInitParamValue || !_pLastParamValue)
    {
        assert(false);
        return;
    }

    // 文本内容没有修改直接返回
    if (!this->isModified())
    {
        return;
    }

    // 更新参数值
    // 经测试:<1>在文本框中修改值再按Enter键,会触发2次editingFinished信号;
    //       <2>在文本框中修改值再将焦点切换到其它控件,只会触发1次editingFinished信号;
    // 为了避免情况<1>下重复修改元素,需要判断当前参数值是否和上一次相等
    wydb::ParameterValueUPtr pCurParamValue = this->newParamValueFromText();
    if (!pCurParamValue)
    {
        this->setText(_lastText);
        this->setFocus();
        return;
    }
    assert(_pLastParamValue->getType() == pCurParamValue->getType());
    if (_isAllTheSameValue)
    {
        if (_pLastParamValue->equals(*pCurParamValue))
        {
            this->setText(_lastText);
            return;
        }
    }

    // 通过事务修改参数值
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->startModifyElements();
    wy::ErrorStatus error = this->modifyElementsByTransaction(*pCurParamValue);
    if (wy::ErrorStatus::Ok == error)
    {
        _isAllTheSameValue = true; // 成功提交事务后值都相等了
        _lastText = this->text();
        _pLastParamValue = std::move(pCurParamValue);
    }
    else
    {
        this->setText(_lastText);
        switch (error)
        {
        case wy::ErrorStatus::NotCurrentlyAllowed:
            MessageBoxUtil::showError(tr("The parameter does not support modification."));
            break;
        case wy::ErrorStatus::InvalidInput:
            MessageBoxUtil::showError(tr("Invalid parameter value."));
            break;
        default:
            MessageBoxUtil::showError(tr("Failed to modify the parameter value!"));
            break;
        }
        this->setFocus();
    }
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->endModifyElements();

    // added by wangyao 2025.06.30 {
    // 成功修改参数后需要刷新所有参数编辑框的值,因为参数之间可以是相互关联的,比如圆的直径和半径.
    if (wy::ErrorStatus::Ok == error)
    {
        Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
            DockPanelIds::Property)->refresh();
    }
    // }
}

wy::ErrorStatus ParamLineEdit::modifyElementsByTransaction(const wydb::ParameterValue& paramValue)
{
    // 获取当前选择集
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty())
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    // 开启事务修改元素参数值
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::Element* pElem = pTrans->getElementForWrite(iter.current().getElementId());
        if (!pElem)
        {
            assert(false);
            continue;
        }
        error = this->modifyElement(pElem, paramValue);
        if (wy::ErrorStatus::Ok != error)
        {
            break;
        }
    }

    if (wy::ErrorStatus::Ok == error)
    {
        wy::ErrorStatus error = pDb->getTransactionManager()->endTransaction();
        assert(wy::ErrorStatus::Ok == error);
    }
    else
    {
        pDb->getTransactionManager()->abortTransaction();
    }
    return error;
}

wy::ErrorStatus ParamLineEdit::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    return pElem->setParameterValue(_className, _paramName, paramValue);
}

wydb::ParameterValueUPtr ParamLineEdit::newParamValueFromText()
{
    assert(_pInitParamValue);
    bool ok(false);
    if (_pInitParamValue->isInteger())
    {
        int newValue(0);
        QString text = this->text().trimmed();
        if (!text.isEmpty() && text.at(0) == '=') // 表达式
        {
            text = text.mid(1);
            if (!this->parserExpression(text, newValue))
            {
                return nullptr;
            }
        }
        else
        {
            newValue = text.toInt(&ok);
            if (!ok) return nullptr;
        }
        return wydb::ParameterValue::createInteger(newValue);
    }
    else if (_pInitParamValue->isDouble())
    {
        double newValue(0.0);
        QString text = this->text().trimmed();
        if (!text.isEmpty() && text.at(0) == '=') // 表达式
        {
            text = text.mid(1);
            if (!this->parserExpression(text, newValue))
            {
                return nullptr;
            }
        }
        else
        {
            newValue = text.toDouble(&ok);
            if (!ok) return nullptr;
        }
        return wydb::ParameterValue::createDouble(newValue);
    }
    else if (_pInitParamValue->isBoolean())
    {
        short value = this->text().toShort();
        if (!ok) return nullptr;
        bool newValue = value > 0 ? true : false;
        return wydb::ParameterValue::createBoolean(newValue);
    }
    else if (_pInitParamValue->isString())
    {
        // modified by wangyao 2025.08.20 {
        // 目前Y3DT格式是ANSI编码的,所有包含超出范围的字符都使用UTF-8编码
        std::string text = this->text().toUtf8().toStdString();
        // }
        return wydb::ParameterValue::createString(text);
    }
    else if (_pInitParamValue->isElementId())
    {
        int nValue = this->text().toInt(&ok);
        if (!ok) return nullptr;
        wydb::ElementId newValue(nValue);
        return wydb::ParameterValue::createElementId(newValue);
    }
    else
    {
        return nullptr;
    }
}

static void _initParser(mu::Parser& parser)
{
    // 定义常量PI
    parser.DefineConst("PI", wy3d::PI);
    parser.DefineConst("pi", wy3d::PI);
    parser.DefineConst("Pi", wy3d::PI);
    parser.DefineConst("pI", wy3d::PI);

    // 添加自然常数e
    parser.DefineConst("e", wy3d::E);
    parser.DefineConst("E", wy3d::E);
}

bool ParamLineEdit::parserExpression(const QString& qstr, double& outValue)
{
    std::string strText = qstr.toStdString();
    outValue = 0.0;

    try
    {
        mu::Parser parser;
        _initParser(parser);
        parser.SetExpr(strText);
        outValue = parser.Eval();
        return true;
    }
    catch (const mu::Parser::exception_type& e)
    {
        std::string strError = e.GetMsg();
        MessageBoxUtil::showError(strError.c_str());
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

bool ParamLineEdit::parserExpression(const QString& qstr, int& outValue)
{
    double value(0.0);
    if (!this->parserExpression(qstr, value))
    {
        return false;
    }
    outValue = static_cast<int>(value);
    return true;
}
