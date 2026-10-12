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

#include "ParamCheckBox.h"

#include <wy3dMath.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFeature.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>

#include <QWidget>
#include <QHBoxLayout>
#include <QGridLayout>

#include <wy3dApp/Application.h>
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "PropertyEditorWidget.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static void setWidgetFontSize(QWidget* pWidget, int pointSize = 12)
{
    QFont font = pWidget->font();
    font.setPointSize(pointSize);
    pWidget->setFont(font);
};

ParamCheckBox::ParamCheckBox(const std::string& className, const std::string& paramName, wydb::ParameterValueUPtr&& pParamValue,
    bool isAllTheSameValue, PropertyEditorWidget* parent)
    : QCheckBox(parent)
    , _className(className)
    , _paramName(paramName)
    , _pInitParamValue(std::move(pParamValue))
    , _isAllTheSameValue(isAllTheSameValue)
{
    setWidgetFontSize(this);

    assert(parent);
    if (parent->isReadOnly())
    {
        this->setEnabled(false);
    }

    if (!_pInitParamValue || !_pInitParamValue->isBoolean())
    {
        assert(false);
        this->setEnabled(false);
        this->setTristate(true); // 启用三态
        this->setCheckState(Qt::PartiallyChecked); // 初始为中间态
        return;
    }

    this->initValue();
    this->connect(this, SIGNAL(stateChanged(int)), this, SLOT(onStateChanged(int)));
}

bool ParamCheckBox::initValue()
{
    // 如果不是所有的值都相等则启用三态
    if (!_isAllTheSameValue)
    {
        this->setTristate(true); // 启用三态
        this->setCheckState(Qt::PartiallyChecked); // 初始为中间态
        return true;
    }

    // 所有值都相等
    assert(_pInitParamValue && _pInitParamValue->isBoolean());
    if (_pInitParamValue->asBoolean())
        this->setChecked(true);
    else
        this->setChecked(false);

    return true;
}

void ParamCheckBox::onStateChanged(int state)
{
    if (Qt::PartiallyChecked == state)
    {
        return;
    }

    // 通过事务修改参数值
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->startModifyElements();
    {
        wydb::ParameterValueUPtr pParamValue = wydb::ParameterValue::createBoolean(Qt::Checked == state ? true : false);
        bool ret = this->modifyElementsByTransaction(*pParamValue);
        if (ret)
        {
            _isAllTheSameValue = true; // 成功提交事务后值都相等了
            if (this->isTristate()) this->setTristate(false); // 取消三态
        }
        else
        {
            if (this->isTristate()) this->setCheckState(Qt::PartiallyChecked);
        }
    }
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->endModifyElements();
}

bool ParamCheckBox::modifyElementsByTransaction(const wydb::ParameterValue& paramValue)
{
    // 获取当前选择集
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty())
    {
        assert(false);
        return false;
    }

    // 开启事务修改元素参数值
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    bool ok(true);
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::Element* pElem = pTrans->getElementForWrite(iter.current().getElementId());
        if (!pElem)
        {
            assert(false);
            continue;
        }
        if (!this->modifyElement(pElem, paramValue))
        {
            ok = false;
            break;
        }
    }

    if (ok)
    {
        wy::ErrorStatus error = pDb->getTransactionManager()->endTransaction();
        assert(wy::ErrorStatus::Ok == error);

        return true;
    }
    else
    {
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
}

bool ParamCheckBox::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem)
    {
        assert(false);
        return false;
    }
    wy::ErrorStatus error = pElem->setParameterValue(_className, _paramName, paramValue);
    if (wy::ErrorStatus::Ok == error)
        return true;
    else
        return false;
}

} // namespace wy3dApp
