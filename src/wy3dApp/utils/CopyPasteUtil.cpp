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

#include "CopyPasteUtil.h"
#include <list>
#include <set>
#include <vector>
#include <cassert>
#include <QCoreApplication>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wyapClipboard.h>
#include <wy3dDatumPlane.h>
#include <wy3dImportedSolid.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>
#include <wy3dSelectionType.h>
#include "application/Application.h"
#include "environments/sketch/SketchEnvironment.h"
#include "utils/MessageBoxUtil.h"

static inline bool _canCopy(const wydb::Element* pElem)
{
    assert(pElem);

    // 导入实体特征暂不支持复制。
    if (pElem->getClassInfo() == wy3d::ImportedSolid::classInfo())
    {
        return false;
    }

    // 元素宿主为空(顶层元素)则一定可以复制
    if (pElem->getParent().isNull())
    {
        return true;
    }

    // 1.实体修改对象不能单独复制
    const wy3d::SolidModification* pSolidMod = wy3d::SolidModification::cast(pElem);
    if (pSolidMod) return false;

    // 2.切除材料实体不能单独复制
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (pSolid)
    {
        if (pSolid->isCut())
        {
            return false;
        }
    }

    return true;
}

CopyPasteUtil::CopyReturn CopyPasteUtil::copy()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return CopyReturn::Error;

    std::set<wydb::ElementId> ids;
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        // 当选中非Element类型时,直接跳过.(比如选中了立方体的面)
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element)
        {
            continue;
        }
        ids.insert(sel.getElementId());
    }
    if (ids.empty()) return CopyReturn::Ok; // 没有选择元素

    std::vector<wydb::ElementId> elemIds;
    for (const wydb::ElementId& id : ids)
    {
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }
        if (!_canCopy(pElem))
        {
            continue;
        }
        elemIds.push_back(id);
    }
    if (elemIds.empty()) return CopyReturn::ElemsNotSupported;

    std::shared_ptr<wyap::ElementsClipData> pClipData = wyap::Clipboard::newElementsClipData(pDb, elemIds);
    if (!pClipData)
    {
        return CopyReturn::Error;
    }
    pClipData->setDescription(CopyPasteUtil::generateCopyPasteLabel());
    Application::instance().getClipboard()->setClipData(pClipData);
    return CopyReturn::Ok;
}

// 只要有一个元素可以复制就返回true
bool CopyPasteUtil::canCopy(const std::list<const wydb::Element*>& elemsToCopy, std::list<const wydb::Element*>& elemsCanCopy)
{
    elemsCanCopy.clear();
    if (elemsToCopy.empty())
    {
        return false;
    }

    for (const wydb::Element* pElem : elemsToCopy)
    {
        if (!pElem)
        {
            assert(false);
            continue;
        }

        if (_canCopy(pElem))
        {
            elemsCanCopy.emplace_back(pElem);
        }
        else
        {
            continue;
        }
    }

    return !elemsCanCopy.empty();
}

bool CopyPasteUtil::canCopy(const std::list<const wydb::Element*>& elemsToCopy)
{
    if (elemsToCopy.empty())
    {
        return false;
    }

    for (const wydb::Element* pElem : elemsToCopy)
    {
        if (!pElem)
        {
            assert(false);
            continue;
        }

        if (_canCopy(pElem)) // 只要有一个元素可以复制就返回true
        {
            return true;
        }
    }

    return false;
}

void CopyPasteUtil::showCopyErrorMsgBox(CopyReturn copyRet)
{
    switch (copyRet)
    {
    case CopyReturn::Ok:
        return;

    case CopyReturn::Error:
        MessageBoxUtil::showError(QCoreApplication::translate("CopyPasteUtil",
            "Copy error."));
        return;

    case CopyReturn::ElemsNotSupported:
        MessageBoxUtil::showError(QCoreApplication::translate("CopyPasteUtil",
            "The elements does not support copying."));
        return;

    default:
        assert(false);
        return;
    }
}

CopyPasteUtil::PasteReturn CopyPasteUtil::paste()
{
    // 剪贴板中没有复制元素则直接返回
    if (!CopyPasteUtil::canPaste())
    {
        return PasteReturn::Ok;
    }

    // 生成拷贝元素
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return PasteReturn::Error;

    std::shared_ptr<const wyap::ClipData> pClipData = Application::instance().getClipboard()->getClipData();
    const wyap::ElementsClipData* pElementsClipData = dynamic_cast<const wyap::ElementsClipData*>(pClipData.get());
    if (!pElementsClipData)
    {
        assert(false);
        return PasteReturn::Error;
    }

    // 将拷贝的对象添加到事务
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        return PasteReturn::Error;
    }
    std::vector<wydb::Element*> copyElements;
    if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTrans, *pElementsClipData, copyElements))
    {
        assert(false);
        return PasteReturn::Error;
    }
    if (copyElements.empty())
    {
        assert(false);
        return PasteReturn::Error;
    }
    for (wydb::Element* pElem : copyElements)
    {
        if (!pElem)
        {
            assert(false);
            continue;
        }
        pTrans->addNewlyCreatedElement(pElem);
    }
    pDb->getTransactionManager()->endTransaction();

    return PasteReturn::Ok;
}

bool CopyPasteUtil::canPaste()
{
    std::shared_ptr<const wyap::ClipData> pClipData = Application::instance().getClipboard()->getClipData();
    if (!pClipData) return false;

    const wyap::ElementsClipData* pElementsClipData = dynamic_cast<const wyap::ElementsClipData*>(pClipData.get());
    if (!pElementsClipData) return false;

    return pElementsClipData->getDescription() == CopyPasteUtil::generateCopyPasteLabel();
}

void CopyPasteUtil::showPasteErrorMsgBox(PasteReturn pasteRet)
{
    switch (pasteRet)
    {
    case PasteReturn::Ok:
        return;

    case PasteReturn::Error:
        MessageBoxUtil::showError(QCoreApplication::translate("CopyPasteUtil",
            "Paste error."));
        return;

    default:
        assert(false);
        return;
    }
}

std::string CopyPasteUtil::generateCopyPasteLabel()
{
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    if (SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnv))
    {
        return "sketch";
    }
    else
    {
        return "";
    }
}
