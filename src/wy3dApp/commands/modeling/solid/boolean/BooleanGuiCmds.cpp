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

#include "commands/modeling/solid/boolean/BooleanGuiCmds.h"
#include <wy3dApp/Application.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dUnion.h>
#include <wy3dDifference.h>
#include <wy3dIntersection.h>
#include <wy3dApp/Scene.h>
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"


// 目标体选择过滤器
// 只能选择wy3d::Solid并且owner为空;
class BooleanTargetSelFilter : public SelectFilterFunctor
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

// 参与体选择过滤器
// <1>不可以是目标体<2>只能选择wy3d::Solid并且owner为空;
class BooleanToolsSelFilter : public SelectFilterFunctor
{
public:
    BooleanToolsSelFilter(const wydb::ElementId& targetId) : _targetId(targetId) {}

    inline virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        assert(pDb);
        wydb::ElementId id = sel.getElementId();
        if (id.isNull() || id == _targetId) return SelectFilterStatus::Continue;
        
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return SelectFilterStatus::Continue;
        if (!pSolid->getParent().isNull()) return SelectFilterStatus::Continue;
        return SelectFilterStatus::Ok;
    }

private:
    wydb::ElementId _targetId;
};

BooleanGuiCmd::BooleanGuiCmd() : OsgGuiCommand(), _boolType(wy3d::BooleanType::Undefined), _targetId(), _step(Step::Undefined)
{
    _options.pointSelect = true;
    _options.boxSelect = false;
    _options.preview = true;
}

BooleanGuiCmd::BooleanGuiCmd(wy3d::BooleanType boolType) : OsgGuiCommand(), _boolType(boolType), _targetId(), _step(Step::Undefined)
{
    _options.pointSelect = true;
    _options.boxSelect = false;
    _options.preview = true;
}

BooleanGuiCmd::~BooleanGuiCmd()
{
}

wyap::CmdExecution::StartResult BooleanGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);


    // 初始化
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    auto extractTargetTools = [](const wyap::SelectionSet& ss,
        wydb::ElementId& targetId, std::set<wydb::ElementId>& toolIds) -> bool
    {
        if (ss.isEmpty()) return false;

        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return false;

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
            wydb::ElementId ownerId = pSolid->getParent();
            if (!ownerId.isNull())
            {
                return false;
            }
            if (targetId.isNull())
            {
                targetId = id;
            }
            else
            {
                toolIds.insert(id);
            }
        }
    };
    wydb::ElementId targetId(wydb::ElementId::kNull);
    std::set<wydb::ElementId> toolIds;
    if (extractTargetTools(ss, targetId, toolIds) && !targetId.isNull())
    {
        if (toolIds.empty())
        {
            Application::instance().getSelManager()->beginChange();
            Application::instance().getSelManager()->clearSelections();
            Application::instance().getSelManager()->endChange();
            _targetId = targetId;
            this->finishStep(Step::SpecifyTarget);

            // 监听选择集
            Application::instance().getSelManager()->addReactor(this);
        }
        else
        {
            Application::instance().getSelManager()->beginChange();
            Application::instance().getSelManager()->clearSelections();
            Application::instance().getSelManager()->endChange();
            _targetId = targetId;
            _toolIds = toolIds;
            this->finishStep(Step::SpecifyTools);
        }
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->gotoStep(Step::SpecifyTarget);

        // 监听选择集
        Application::instance().getSelManager()->addReactor(this);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void BooleanGuiCmd::onEnd()
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
void BooleanGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
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

void BooleanGuiCmd::onSelectionChanged(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& currSS)
{
    if (_step == Step::SpecifyTarget)
    {
        if (addedSS.getCount() == 1)
        {
            wydb::ElementId id = addedSS.createIterator().current().getElementId();
            _targetId = id;
            this->finishStep(_step);
        }
    }
    else if (_step == Step::SpecifyTools)
    {
        _toolIds.clear();
        for (auto iter = currSS.createIterator(); !iter.isDone(); iter.moveNext())
        {
            wydb::ElementId id = iter.current().getElementId();
            if (id == _targetId) continue;
            _toolIds.insert(id);
        }
    }
    else
    {
        assert(false);
    }
}

void BooleanGuiCmd::reset()
{
    Step _step = Step::Undefined;
    _targetId = wydb::ElementId::kNull;
    _toolIds.clear();

    this->gotoStep(Step::SpecifyTarget);
}

bool BooleanGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyTarget:
    {
        // next step
        this->gotoStep(Step::SpecifyTools);
        return true;
    }
    break;

    case Step::SpecifyTools:
    {
        if (!this->perform())
        {
            this->reset();
            return false;
        }

        // next step
        this->requestEnd();
        //this->gotoStep(Step::SpecifyTarget);
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

void BooleanGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SpecifyTarget:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 支持点选;不支持框选
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = false;
        selOptions.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.filter = std::make_shared<BooleanTargetSelFilter>();
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Full;
        this->configSelect(selOptions);

        // 输入控件
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("BooleanGuiCmd",
            "Select the target solid."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SpecifyTools:
    {
        // 支持点选+框选
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.filter = std::make_shared<BooleanToolsSelFilter>(_targetId);
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        // 输入控件
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("BooleanGuiCmd",
            "Select one or more tool solids; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 高亮目标体
        _targetHighlightor.addSelection(wyap::Selection(_targetId));
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

void BooleanGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    auto tryAddSelection = [](const wydb::ElementId& id)
    {
        wyap::SelManager* pSelMgr = Application::instance().getSelManager();
        if (!pSelMgr) return;

        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return;
        wydb::ElementId ownerId = pSolid->getParent();
        if (!ownerId.isNull()) return;

        wyap::Selection sel(id);
        if (pSelMgr->getSelections().contains(sel))
        {
            return;
        }
        pSelMgr->beginChange();
        pSelMgr->addSelection(wyap::Selection(id));
        pSelMgr->endChange();
    };

    switch (_step)
    {
    case Step::SpecifyTarget:
    {
        tryAddSelection(id);
    }
    break;

    case Step::SpecifyTools:
    {
        if (id == _targetId) return;
        tryAddSelection(id);
    }
    break;

    default:
    {
    }
    break;
    }
}

void BooleanGuiCmd::onEnterKey()
{
    if (Step::SpecifyTools == _step)
    {
        if (!_toolIds.empty())
        {
            this->finishStep(_step);
        }
    }
}

void BooleanGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool BooleanGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SpecifyTools == _step;
}

void BooleanGuiCmd::onContextMenuAction_CompleteSelection()
{
    if (BooleanGuiCmd::Step::SpecifyTools == _step)
    {
        if (!_toolIds.empty())
        {
            this->finishStep(_step);
        }
    }
}

bool BooleanGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SpecifyTools == _step;
}

void BooleanGuiCmd::onContextMenuAction_ClearSelection()
{
    if (BooleanGuiCmd::Step::SpecifyTools == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

bool BooleanGuiCmd::perform()
{
    // 执行布尔操作
    bool ret = this->newBooleanFeature();

    return ret;
}

bool BooleanGuiCmd::newBooleanFeature()
{
    if (_targetId.isNull()) return false;
    if (_toolIds.empty()) return false;

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

    // 执行操作
    {
        // 目标体
        wy3d::Solid* pTargetSolid = wy3d::Solid::cast(pTrans->getElementForWrite(_targetId));
        if (!pTargetSolid)
        {
            assert(false);
            goto ABORT_TRANS;
        }

        // 参与体
        std::vector<wy3d::Solid*> toolSolids;
        toolSolids.reserve(_toolIds.size());
        for (const wydb::ElementId& toolId : _toolIds)
        {
            wy3d::Solid* pToolSolid = wy3d::Solid::cast(pTrans->getElementForWrite(toolId));
            if (!pToolSolid)
            {
                assert(false);
                goto ABORT_TRANS;
            }
            toolSolids.emplace_back(pToolSolid);
        }

        // 布尔体
        if (wy3d::BooleanType::Union == _boolType)
        {
            wy3d::Union* pUnion = wy3d::Union::cast(pTargetSolid);
            if (pUnion)
            {
                for (wy3d::Solid* pTool : toolSolids)
                {
                    if (wy::ErrorStatus::Ok != pUnion->addTool(pTool))
                    {
                        assert(false);
                        goto ABORT_TRANS;
                    }
                }
            }
            else
            {
                wy::ErrorStatus error = wy3d::Union::create(pTrans, pTargetSolid, toolSolids, pUnion);
                if (wy::ErrorStatus::Ok == error)
                {
                }
                else
                {
                    assert(false);
                    goto ABORT_TRANS;
                }
            }
        }
        else if (wy3d::BooleanType::Difference == _boolType)
        {
            wy3d::Difference* pDifference = wy3d::Difference::cast(pTargetSolid);
            if (pDifference)
            {
                for (wy3d::Solid* pTool : toolSolids)
                {
                    if (wy::ErrorStatus::Ok != pDifference->addTool(pTool))
                    {
                        assert(false);
                        goto ABORT_TRANS;
                    }
                }
            }
            else
            {
                wy::ErrorStatus error = wy3d::Difference::create(pTrans, pTargetSolid, toolSolids, pDifference);
                if (wy::ErrorStatus::Ok == error)
                {
                }
                else
                {
                    assert(false);
                    goto ABORT_TRANS;
                }
            }
        }
        else if (wy3d::BooleanType::Intersection == _boolType)
        {
            wy3d::Intersection* pIntersection = wy3d::Intersection::cast(pTargetSolid);
            if (pIntersection)
            {
                for (wy3d::Solid* pTool : toolSolids)
                {
                    if (wy::ErrorStatus::Ok != pIntersection->addTool(pTool))
                    {
                        assert(false);
                        goto ABORT_TRANS;
                    }
                }
            }
            else
            {
                wy::ErrorStatus error = wy3d::Intersection::create(pTrans, pTargetSolid, toolSolids, pIntersection);
                if (wy::ErrorStatus::Ok == error)
                {
                }
                else
                {
                    assert(false);
                    goto ABORT_TRANS;
                }
            }   
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }

    // 提交事务
    pTransMgr->endTransaction();
    return true;

    // 终止事务
ABORT_TRANS:
    pTransMgr->abortTransaction();
    return false;
}
