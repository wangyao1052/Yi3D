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

#include "commands/modeling/solid/MergeGuiCmd.h"
#include <wydbTransaction.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dSolid.h>
#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/MessageBoxUtil.h"


// 主体选择过滤器
// 只能选择wy3d::Solid并且owner为空;
class MergeHostSelFilter : public SelectFilterFunctor
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

        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return SelectFilterStatus::Continue;
        if (!pSolid->getParent().isNull()) return SelectFilterStatus::Continue;
        return SelectFilterStatus::Ok;
    }
};

// 成员选择过滤器
// <1>不可以是主体<2>只能选择wy3d::Solid并且owner为空;
class MergeMembersSelFilter : public SelectFilterFunctor
{
public:
    MergeMembersSelFilter(const wydb::ElementId& hostId) : _hostId(hostId) {}

    inline virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        assert(pDb);
        wydb::ElementId id = sel.getElementId();
        if (id.isNull() || id == _hostId) return SelectFilterStatus::Continue;

        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return SelectFilterStatus::Continue;
        if (!pSolid->getParent().isNull()) return SelectFilterStatus::Continue;
        return SelectFilterStatus::Ok;
    }

private:
    wydb::ElementId _hostId;
};

MergeGuiCmd::MergeGuiCmd() : OsgGuiCommand(), _step(Step::Undefined), _hostId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

MergeGuiCmd::~MergeGuiCmd()
{
}

wyap::CmdExecution::StartResult MergeGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化:点选选项
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());

    // 初始化:步骤
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    auto isValidSelectionSet = [](const wyap::SelectionSet& ss, std::vector<wydb::ElementId>& ids) -> bool
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return false;

        ids.clear();
        ids.reserve(ss.getCount());
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wyap::Selection& sel = iter.current();
            if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
            {
                return false;
            }
            wydb::ElementId id = sel.getElementId();
            const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
            if (!pSolid) return false;
            if (!pSolid->getParent().isNull()) return false;
            ids.emplace_back(id);
        }
        return !ids.empty();
    };
    std::vector<wydb::ElementId> ids;
    if (isValidSelectionSet(ss, ids) && !ids.empty())
    {
        this->clearSelections();
        _hostId = ids.front();
        for (const wydb::ElementId& id : ids)
        {
            _pSelSetHighlightor->addSelection(wyap::Selection(id));
        }
        if (ids.size() == 1)
        {
            this->finishStep(Step::SelectHost);
        }
        else
        {
            assert(ids.size() > 1);
            this->finishStep(Step::SelectMembers);
        }
    }
    else
    {
        this->clearSelections();
        this->gotoStep(Step::SelectHost);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void MergeGuiCmd::cleanup()
{
    _step = Step::Undefined;
    _hostId = wydb::ElementId::kNull;
    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;
}

bool MergeGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectHost:
    {
        if (_hostId.isNull())
        {
            assert(false);
            return false;
        }

        // next step
        this->gotoStep(Step::SelectMembers);
        return true;
    }
    break;

    case Step::SelectMembers:
    {
        if (_pSelSetHighlightor->getSelectionSet().getCount() <= 1)
        {
            assert(false);
            return false;
        }

        unsigned int errorCode(0);
        if (this->merge(errorCode))
        {
            // exit
            this->requestEnd();
            return true;
        }
        else
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }

            // exit
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
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

void MergeGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectHost:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MergeGuiCmd",
            "Select the host solid."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;

        // 点选:选择过滤器
        assert(_hostId.isNull());
        _pointPickOption.pSelFilter = std::make_shared<MergeHostSelFilter>();
    }
    break;

    case Step::SelectMembers:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MergeGuiCmd",
            "Select one or more solids to merge; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;

        // 点选:选择过滤器
        assert(!_hostId.isNull());
        _pointPickOption.pSelFilter = std::make_shared<MergeMembersSelFilter>(_hostId);
    }
    break;

    default:
    {
        // 清空提示
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void MergeGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectHost:
    case Step::SelectMembers:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
    }
    break;
    }

    return;
}

void MergeGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectHost:
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            if (!sel.getElementId().isNull())
            {
                assert(_hostId.isNull());
                assert(_pSelSetHighlightor->getSelectionSet().isEmpty());
                _hostId = sel.getElementId();
                _pSelSetHighlightor->addSelection(sel);
                this->finishStep(_step);
            }
        }
    }
    break;

    case Step::SelectMembers:
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            assert(sel.getElementId() != _hostId);
            if (_pSelSetHighlightor->containsSelection(sel))
            {
                _pSelSetHighlightor->removeSelection(sel);
            }
            else
            {
                _pSelSetHighlightor->addSelection(sel);
            }
            _pPreview = nullptr;
        }
    }
    break;
    }

    return;
}

void MergeGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    switch (_step)
    {
    case Step::SelectHost:
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return;
        if (!pSolid->getParent().isNull()) return;

        _hostId = id;
        if (_pSelSetHighlightor) _pSelSetHighlightor->addSelection(wyap::Selection(id));
        this->finishStep(_step);
    }
    break;

    case Step::SelectMembers:
    {
        if (id == _hostId) return;

        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return;
        if (!pSolid->getParent().isNull()) return;
        if (_pSelSetHighlightor) _pSelSetHighlightor->addSelection(wyap::Selection(id));
    }
    break;

    default:
    {

    }
    break;
    }
}

void MergeGuiCmd::onEnterKey()
{
    if (Step::SelectMembers == _step)
    {
        const wyap::SelectionSet& ss = _pSelSetHighlightor->getSelectionSet();
        if (ss.getCount() > 1)
        {
            this->finishStep(_step);
        }
    }
}

void MergeGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool MergeGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectMembers == _step;
}

void MergeGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool MergeGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectMembers == _step;
}

void MergeGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectMembers == _step && _pSelSetHighlightor)
    {
        wyap::SelectionSet ss = _pSelSetHighlightor->getSelectionSet();
        if (!_hostId.isNull()) ss.remove(wyap::Selection(_hostId));
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            _pSelSetHighlightor->removeSelection(iter.current());
        }
    }
}

bool MergeGuiCmd::merge(unsigned int errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    // 提取数据
    if (_hostId.isNull()) return false;
    wyap::SelectionSet memberSS = _pSelSetHighlightor->getSelectionSet();
    memberSS.remove(wyap::Selection(_hostId));
    if (memberSS.isEmpty()) return false;
    std::set<wydb::ElementId> members;
    for (auto iter = memberSS.createIterator(); !iter.isDone(); iter.moveNext())
    {
        members.insert(iter.current().getElementId());
    }

    // 开启事务创建倒角圆角
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(_hostId));
    if (!pSolid)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    for (const wydb::ElementId& memberId : members)
    {
        wy3d::Solid* pSolidMember = wy3d::Solid::cast(pTrans->getElementForWrite(memberId));
        if (!pSolidMember)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        if (wy::ErrorStatus::Ok != pSolid->addModification(pSolidMember))
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
    }
    pDb->getTransactionManager()->endTransaction();

    // 查看有无错误码
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(pTransMgr->getChainUpdateFeedback(pSolid->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }

    return true;
}

