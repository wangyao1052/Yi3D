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

#include "TransformLineEdit.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

#include <wyVector3.h>
#include <wy3dMath.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFeature.h>
#include <wy3dPrimitive.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <QHBoxLayout>
#include <QGridLayout>

#include <wy3dApp/Application.h>

TransformLineEdit::TransformLineEdit(
    TransformUnit unit,
    wydb::ParameterValueUPtr&& pParamValue,
    bool isAllTheSameValue,
    PropertyEditorWidget* parent) : ParamLineEdit("", "", std::move(pParamValue), isAllTheSameValue, false, parent), _unit(unit)
{
}

wy::ErrorStatus TransformLineEdit::modifyElement(wydb::Element* pElem, const wydb::ParameterValue& paramValue)
{
    if (!pElem)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }
    wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
    if (!pPrimitive)
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }

    wy::Vector3 pos = pPrimitive->getPosition();
    wy::Vector3 rot = pPrimitive->getRotation();
    switch (_unit)
    {
    case TransformUnit::PosX:
    {
        return pPrimitive->setPosition(wy::Vector3(paramValue.asDouble(), pos.y(), pos.z()));
    }
    break;

    case TransformUnit::PosY:
    {
        return pPrimitive->setPosition(wy::Vector3(pos.x(), paramValue.asDouble(), pos.z()));
    }
    break;

    case TransformUnit::PosZ:
    {
        return pPrimitive->setPosition(wy::Vector3(pos.x(), pos.y(), paramValue.asDouble()));
    }
    break;

    case TransformUnit::RotX:
    {
        double radian = wy3d::degreesToRadians(paramValue.asDouble());
        return pPrimitive->setRotation(wy::Vector3(radian, rot.y(), rot.z()));
    }
    break;

    case TransformUnit::RotY:
    {
        double radian = wy3d::degreesToRadians(paramValue.asDouble());
        return pPrimitive->setRotation(wy::Vector3(rot.x(), radian, rot.z()));
    }
    break;

    case TransformUnit::RotZ:
    {
        double radian = wy3d::degreesToRadians(paramValue.asDouble());
        return pPrimitive->setRotation(wy::Vector3(rot.x(), rot.y(), radian));
    }
    break;

    default:
    {
        assert(false);
        return wy::ErrorStatus::Error;
    }
    break;
    }

    return wy::ErrorStatus::Error;
}

void TransformLineEdit::getCurrParamValueFromDb(bool& isAllTheSameValue, wydb::ParameterValueUPtr& pOutParamValue)
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
        const wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
        if (!pPrimitive)
        {
            assert(false);
            continue;
        }

        wydb::ParameterValueUPtr pParamValue(nullptr);
        switch (_unit)
        {
        case TransformUnit::PosX:
            pParamValue = wydb::ParameterValue::createDouble(pPrimitive->getPosition().x());
            break;
        case TransformUnit::PosY:
            pParamValue = wydb::ParameterValue::createDouble(pPrimitive->getPosition().y());
            break;
        case TransformUnit::PosZ:
            pParamValue = wydb::ParameterValue::createDouble(pPrimitive->getPosition().z());
            break;
        case TransformUnit::RotX:
            pParamValue = wydb::ParameterValue::createDouble(pPrimitive->getRotation().x());
            break;
        case TransformUnit::RotY:
            pParamValue = wydb::ParameterValue::createDouble(pPrimitive->getRotation().y());
            break;
        case TransformUnit::RotZ:
            pParamValue = wydb::ParameterValue::createDouble(pPrimitive->getRotation().z());
            break;
        default:
            assert(false);
            break;
        }

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