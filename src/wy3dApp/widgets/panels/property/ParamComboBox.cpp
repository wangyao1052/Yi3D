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

#include "ParamComboBox.h"
#include "EnumLabelTranslator.h"

#include <QFont>

#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>

#include "application/Application.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "PropertyEditorWidget.h"
#include "utils/MessageBoxUtil.h"

static void setWidgetFontSize(QWidget* pWidget, int pointSize = 12)
{
    QFont font = pWidget->font();
    font.setPointSize(pointSize);
    pWidget->setFont(font);
};

ParamComboBox::ParamComboBox(
    const std::string& className,
    const std::string& paramName,
    wydb::ParameterValueUPtr&& pParamValue,
    bool isAllTheSameValue,
    PropertyEditorWidget* parent)
    : QComboBox(parent)
    , _className(className)
    , _paramName(paramName)
    , _pInitParamValue(std::move(pParamValue))
    , _isAllTheSameValue(isAllTheSameValue)
    , _updating(false)
{
    setWidgetFontSize(this);

    assert(parent);
    if (parent->isReadOnly())
    {
        this->setEnabled(false);
    }

    // 从 AnyParameterValue 中提取 ParamEnumDef
    if (_pInitParamValue && _pInitParamValue->isAny())
    {
        const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(_pInitParamValue.get());
        if (pAnyVal)
        {
            const wy3d::ParamEnumDef* pDef = pAnyVal->tryGet<wy3d::ParamEnumDef>();
            if (pDef)
            {
                _enumDef = *pDef;
            }
        }
    }

    if (_enumDef.options.empty())
    {
        assert(false);
        this->setEnabled(false);
        return;
    }

    this->initItems();
    this->connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(onCurrentIndexChanged(int)));
}

void ParamComboBox::initItems()
{
    _updating = true;

    this->blockSignals(true);
    this->clear();

    for (const auto& opt : _enumDef.options)
    {
        this->addItem(EnumLabelTranslator::translate(opt.label), opt.value);
    }

    if (!_isAllTheSameValue)
    {
        // 多选且值不同时显示空项
        this->insertItem(0, QString::fromStdString("-"), -1);
        this->setCurrentIndex(0);
    }
    else
    {
        // 选中当前值对应的项
        int matched = -1;
        for (int i = 0; i < this->count(); ++i)
        {
            if (this->itemData(i).toInt() == _enumDef.currentValue)
            {
                matched = i;
                break;
            }
        }
        if (matched >= 0)
            this->setCurrentIndex(matched);
        else if (this->count() > 0)
            this->setCurrentIndex(0); // 兜底: 数据损坏时选第一项
    }

    this->blockSignals(false);
    _updating = false;
}

void ParamComboBox::onCurrentIndexChanged(int index)
{
    if (_updating) return;

    int newValue = this->itemData(index).toInt();
    if (newValue < 0) return; // 多选占位项, 不处理

    if (newValue == _enumDef.currentValue && _isAllTheSameValue) return;

    // 通过事务修改参数值
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->startModifyElements();
    {
        // 构造新的 ParamEnumDef
        wy3d::ParamEnumDef newDef(_enumDef.options, newValue);
        wydb::ParameterValueUPtr pParamValue = wydb::ParameterValue::createAny(newDef);
        bool ret = this->modifyElementsByTransaction(*pParamValue);
        if (ret)
        {
            _isAllTheSameValue = true;
            _enumDef.currentValue = newValue;
            initItems(); // 移除多选"-"占位项, 刷新为正常列表
        }
        else
        {
            // 失败, 恢复原值
            _updating = true;
            for (int i = 0; i < this->count(); ++i)
            {
                if (this->itemData(i).toInt() == _enumDef.currentValue)
                {
                    this->setCurrentIndex(i);
                    break;
                }
            }
            _updating = false;
        }
    }
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->endModifyElements();
}

bool ParamComboBox::modifyElementsByTransaction(const wydb::ParameterValue& paramValue)
{
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty())
    {
        assert(false);
        return false;
    }

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

bool ParamComboBox::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem)
    {
        assert(false);
        return false;
    }
    wy::ErrorStatus error = pElem->setParameterValue(_className, _paramName, paramValue);
    return wy::ErrorStatus::Ok == error;
}
