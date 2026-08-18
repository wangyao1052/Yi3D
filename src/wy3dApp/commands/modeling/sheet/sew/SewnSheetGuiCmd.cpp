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

#include "SewnSheetGuiCmd.h"

#include <cassert>
#include <QCoreApplication>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSelectionType.h>
#include <wy3dSewnSheet.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"

// 默认缝合容差（与 SewnSheet 构造默认一致）
constexpr double kDefaultSewTolerance = 1e-6;

// 源片体选择过滤器: 只能是 wy3d::Sheet 且无主（未被其它特征占用）
class SewnSheetSourceSelFilter : public SelectFilterFunctor
{
public:
    inline virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        assert(pDb);
        wydb::ElementId id = sel.getElementId();
        if (id.isNull()) return SelectFilterStatus::Continue;

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(id));
        if (!pSheet) return SelectFilterStatus::Continue;
        if (!pSheet->getParent().isNull()) return SelectFilterStatus::Continue;
        return SelectFilterStatus::Ok;
    }
};

SewnSheetGuiCmd::SewnSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined)
{
    _options.pointSelect = true;
    _options.boxSelect = false;
    _options.preview = true;
}

SewnSheetGuiCmd::~SewnSheetGuiCmd()
{
}

wyap::CmdExecution::StartResult SewnSheetGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 监听选择集
    Application::instance().getSelManager()->addReactor(this);

    // USEPICKFIRST: 预选全是 Sheet → 直达公差步
    bool allSheets = true;
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (ss.isEmpty())
        {
            allSheets = false;
        }
        else
        {
            wydb::Database* pDb = Application::instance().getActiveDatabase();
            if (!pDb)
            {
                allSheets = false;
            }
            else
            {
                for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
                {
                    const wyap::Selection& sel = iter.current();
                    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
                    {
                        allSheets = false;
                        break;
                    }
                    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sel.getElementId()));
                    if (!pSheet)
                    {
                        allSheets = false;
                        break;
                    }
                    if (!pSheet->getParent().isNull())
                    {
                        allSheets = false;
                        break;
                    }
                    _sourceIds.insert(sel.getElementId());
                }
            }
        }
    }

    if (allSheets && !_sourceIds.empty())
    {
        this->clearSelections();
        this->finishStep(Step::SelectSources);
    }
    else
    {
        this->clearSelections();
        _sourceIds.clear();
        this->gotoStep(Step::SelectSources);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SewnSheetGuiCmd::onEnd()
{
    // 取消监听选择集
    Application::instance().getSelManager()->removeReactor(this);

    // 基类
    __baseClass::onEnd();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
}

void SewnSheetGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 取消监听选择集
    Application::instance().getSelManager()->removeReactor(this);

    // 基类
    __baseClass::onAbort(cause);

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
}

void SewnSheetGuiCmd::onSelectionChanged(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& currSS)
{
    if (_step == Step::SelectSources)
    {
        _sourceIds.clear();
        for (auto iter = currSS.createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wyap::Selection& sel = iter.current();
            if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
            {
                continue;
            }
            wydb::ElementId id = sel.getElementId();
            if (id.isNull())
            {
                continue;
            }
            _sourceIds.insert(id);
        }
    }
}

bool SewnSheetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSources:
    {
        if (_sourceIds.empty())
        {
            assert(false);
            return false;
        }

        // 直接以默认容差创建
        if (!this->newSewnSheet())
        {
            return false; // 创建失败: 留在选源步, 可修改选择后重试
        }

        this->requestEnd();
        return true;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void SewnSheetGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectSources:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 支持点选+框选
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.pickMask = static_cast<unsigned int>(ElementNodeType::Sheet);
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.filter = std::make_shared<SewnSheetSourceSelFilter>();
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SewnSheetGuiCmd",
            "Select one or more sheets to sew; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void SewnSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSources != _step) return;
    if (id.isNull()) return;

    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    if (!pSelMgr) return;
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(id));
    if (!pSheet) return;
    if (!pSheet->getParent().isNull()) return;

    wyap::Selection sel(id);
    if (pSelMgr->getSelections().contains(sel)) return;
    pSelMgr->beginChange();
    pSelMgr->addSelection(wyap::Selection(id));
    pSelMgr->endChange();
}

void SewnSheetGuiCmd::onEnterKey()
{
    if (Step::SelectSources == _step)
    {
        if (!_sourceIds.empty())
        {
            this->finishStep(_step);
        }
    }
}

void SewnSheetGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool SewnSheetGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectSources == _step;
}

void SewnSheetGuiCmd::onContextMenuAction_CompleteSelection()
{
    if (Step::SelectSources == _step)
    {
        if (!_sourceIds.empty())
        {
            this->finishStep(_step);
        }
    }
}

bool SewnSheetGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectSources == _step;
}

void SewnSheetGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectSources == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

bool SewnSheetGuiCmd::newSewnSheet()
{
    if (_sourceIds.empty()) return false;

    // 开启事务
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    {
        // 源片体
        std::vector<wy3d::Sheet*> sources;
        sources.reserve(_sourceIds.size());
        for (const wydb::ElementId& sourceId : _sourceIds)
        {
            wy3d::Sheet* pSource = wy3d::Sheet::cast(pTrans->getElementForWrite(sourceId));
            if (!pSource)
            {
                assert(false);
                goto ABORT_TRANS;
            }
            sources.emplace_back(pSource);
        }

        // 缝合片体
        wy3d::SewnSheet* pSewnSheet = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SewnSheet::create(pTrans, sources, kDefaultSewTolerance, pSewnSheet))
        {
            assert(false);
            goto ABORT_TRANS;
        }
        assert(pSewnSheet);

        // 提交事务
        pTransMgr->endTransaction();

        // 缝合片体已创建成功但还需要查看有无错误码
        unsigned int errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pTransMgr->getChainUpdateFeedback(pSewnSheet->getId()).get());
        if (errorCode != 0)
        {
            MessageBoxUtil::showError(errorCode);
            return false;
        }
    }

    return true;

    // 终止事务
ABORT_TRANS:
    pTransMgr->abortTransaction();
    return false;
}
