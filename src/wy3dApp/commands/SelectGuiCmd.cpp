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

#include "SelectGuiCmd.h"
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wyapSelManager.h>
#include <wyapGizmoManager.h>
#include "gizmo/GizmoFactory.h"
#include <wyapClipboard.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include <wy3dFeature.h>
#include <wy3dSolid.h>
#include <wy3dPrimitive.h>
#include <wy3dDatumPlane.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include "scene/nodes/ElementNode.h"
#include "utils/TransactionUtil.h"
#include "utils/CopyPasteUtil.h"
#include "environments/sketch/SketchEnvironment.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/featureTree/FeatureTreeWidget.h"
#include "widgets/panels/property/PropertyEditorWidget.h"
#include "application/Application.h"
#include "widgets/frame/MainWindow.h"
#include "select/filters/CommonSelFilters.h"
#include "commands/CommandNames.h"

#include <wy3dSketchEllipseArc.h>


class SelectGuiCmdSelFilter_Modeling : public SelectFilterFunctor
{
public:
    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        assert(pDb);
        switch (selectAction)
        {
        case SelectAction::Point:
        {
            return SelectFilterStatus::Ok;
        }
        break;

        case SelectAction::Window:
        case SelectAction::Crossing:
        {
            // 参照SolidWorks的行为:框选时不选中参照平面
            const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(
                pDb->getElement(sel.getElementId()));
            if (pDatumPlane)
            {
                return SelectFilterStatus::Continue;
            }
        }
        break;

        default:
        {
            assert(false);
        }
        break;
        }

        return SelectFilterStatus::Ok;
    }
};

SelectGuiCmd::SelectGuiCmd() : OsgGuiCommand()
{
    _options.pointSelect = true;
    _options.boxSelect = true;
    _options.selectionType = wy3d::SelectionType::Element;
    _options.preview = true;
}

SelectGuiCmd::~SelectGuiCmd()
{
}

void SelectGuiCmd::enableSelect()
{
    // 选择配置项
    GuiCmdSelectOptions selOptions;
    selOptions.pointSelect = true;
    selOptions.boxSelect = true;
    selOptions.selectionType = wy3d::SelectionType::Element;
    configureSelectOptions(selOptions);
    selOptions.preview = true;
    selOptions.selectMode = SelectMode::Full;
    this->configSelect(selOptions);
}

void SelectGuiCmd::disableSelect()
{
    GuiCmdSelectOptions selOptions;
    selOptions.pointSelect = false;
    selOptions.boxSelect = false;
    this->configSelect(selOptions);
}

wyap::CmdExecution::StartResult SelectGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 建模环境下的选择命令才支持特征树上的节点可选择
    onStart_EnvSpecific();
    // 选择命令支持属性框可编辑
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->setReadOnly(false);

    // 监听选择集
    Application::instance().getSelManager()->addReactor(this);

    // 添加Gizmo
    bool addGizmoRet = this->tryAddElementPositionGizmo(Application::instance().getSelManager()->getSelections());

    // 拷贝操作
    _pPasteOp = nullptr;

    // 更新提示
    this->updateSelectTipAndLabel();

    // 允许选择(点选+框选)
    this->enableSelect();

    // 局部刷新捕捉系统
    // 在新增了重定位草图坐标系命令后,需要在选择命令的开始局部刷新捕捉系统;
    // 因为选择命令中的复制粘贴会使用捕捉功能.
    if (getSketchSnapSys())
    {
        getSketchSnapSys()->partiallyUpdate(Application::instance().getActiveDatabase());
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SelectGuiCmd::onEnd()
{
    // 取消监听选择集
    Application::instance().getSelManager()->removeReactor(this);

    // 特征树节点不可选
    Application::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setSelectable(false);
    // 属性编辑框只读
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->setReadOnly(true);

    // 放弃当前粘贴的对象
    _pPasteOp = nullptr;

    __baseClass::onEnd();

}
void SelectGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 取消监听选择集
    Application::instance().getSelManager()->removeReactor(this);

    // 特征树节点不可选
    Application::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setSelectable(false);
    // 属性编辑框只读
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->setReadOnly(true);

    // 放弃当前粘贴的对象
    _pPasteOp = nullptr;

    __baseClass::onAbort(cause);

}

void SelectGuiCmd::reset()
{
    // 粘贴模式
    if (_pPasteOp)
    {
        this->cancelPaste();
    }
    // 选择模式
    else
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

void SelectGuiCmd::onEnterKey()
{
}

void SelectGuiCmd::onSpaceKey()
{
    if (_pPasteOp) return; // 在执行粘贴动作

    //Application::instance().postLastCommandEvent();
}

// 选择集变更
void SelectGuiCmd::onSelectionChanged(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& currSS)
{
    // 清除Gizmo
    Application::instance().getGizmoManager()->beginChange();
    Application::instance().getGizmoManager()->clearGizmos();
    Application::instance().getGizmoManager()->endChange();

    // 获取选择的特征
    bool addGizmoRet = this->tryAddElementPositionGizmo(currSS);

    // 更新提示
    this->updateSelectTipAndLabel();
}

void SelectGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (_pPasteOp)
    {
        wy::Vector3 pos = computePastePosition(event.x, event.y, _pPasteOp->excludeIds);
        _pPasteOp->pPasteElements->update(pos);
    }
    return;
}

void SelectGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (_pPasteOp) // 正在粘贴元素
    {
        wy::Vector3 pos = computePastePosition(event.x, event.y, _pPasteOp->excludeIds);
        this->endPaste(pos);
    }
    return;
}

void SelectGuiCmd::onKeyDown(const KeyEvent& event)
{
    // Ctrl+A 全选
    if (event.key == KeyCode::CtrlA)
    {
        if (!_pPasteOp)
        {
            selectAll();
        }
    }
    // Ctrl+C 复制
    else if (event.key == KeyCode::CtrlC)
    {
        if (!_pPasteOp)
        {
            copy();
        }
    }
    // Ctrl+V 粘贴
    else if (event.key == KeyCode::CtrlV)
    {
        if (!_pPasteOp)
        {
            QPoint screenPos = QCursor::pos();
            double wx, wy;
            this->screenToWindowPos(screenPos.x(), screenPos.y(), wx, wy);
            this->beginPaste(wx, wy);
        }
    }
    // Delete 删除
    else if (event.key == KeyCode::Delete && !_pPasteOp)
    {
        this->erase();
    }
    else
    {
        __baseClass::onKeyDown(event);
    }

    return;
}

void SelectGuiCmd::onEscapeKey()
{
    // In paste mode, Esc cancels paste; otherwise clears selections.
    if (_pPasteOp)
    {
        this->cancelPaste();
    }
    else
    {
        wyap::SelManager* pSelMgr = Application::instance().getSelManager();
        assert(pSelMgr);
        pSelMgr->beginChange();
        pSelMgr->clearSelections();
        pSelMgr->endChange();
    }
}

void SelectGuiCmd::selectAll()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    wyap::SelectionSet ss;
    selectAll_Impl(ss);

    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->setSelections(ss);
    Application::instance().getSelManager()->endChange();
}

void SelectGuiCmd::copy()
{
    CopyPasteUtil::CopyReturn ret = CopyPasteUtil::copy();
    if (ret != CopyPasteUtil::CopyReturn::Ok)
    {
        CopyPasteUtil::showCopyErrorMsgBox(ret);
    }
}

void SelectGuiCmd::erase()
{
    std::set<wydb::ElementId> ids;
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        // added by wangyao 2025.04.18 {
        // 当选中非Element类型时,直接跳过.(比如选中了立方体的面)
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element)
        {
            continue;
        }
        // }
        ids.insert(sel.getElementId());
    }
    if (ids.empty()) return;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    // added by wangyao 2025.04.19 {
    // 删除前清空选择集应该是一个不错的选择,避免未考虑到的一些逻辑问题.
    // 清空选择集
    Application::instance().getSelManager()->clearSelections();
    // }

    wydb::TransactionOption option;
    option.chainUpdateScope = getSketchSnapSys() ? wydb::ChainUpdateScope::Local : wydb::ChainUpdateScope::Cascade;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return;
    }
    for (const wydb::ElementId& id : ids)
    {
        wydb::Element* pElem = pTrans->getElementForWrite(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }
        pElem->erase(true);
    }
    pDb->getTransactionManager()->endTransaction();
}

void SelectGuiCmd::updateSelectTipAndLabel()
{
    if (!_pPasteOp)
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SelectGuiCmd",
            "Select elements. Pick individual items or use window selection."));
    }
}

bool SelectGuiCmd::beginPaste(double x, double y)
{
    // 剪贴板中没有复制元素则直接返回
    if (!CopyPasteUtil::canPaste())
    {
        return false;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    _pPasteOp = std::make_shared<PasteOp>();
    _pPasteOp->pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    _pPasteOp->pPasteElements = std::make_shared<PasteElements>(pDb, this, getPasteEnvType(), getSketchId(), getSketchPlane());
    wy::Vector3 pos = computePastePosition(x, y, {});
    PasteElements::InitRet initRet = _pPasteOp->pPasteElements->init(pos);
    if (PasteElements::InitRet::Failed == initRet)
    {
        assert(false);
        _pPasteOp = nullptr;
        return false;
    }
    else if (PasteElements::InitRet::Success_End == initRet)
    {
        _pPasteOp->pPasteElements->commit();
        _pPasteOp = nullptr;
        return true;
    }
    else if (PasteElements::InitRet::Success_Continue == initRet)
    {
        // 继续之后的逻辑
        if (!getSketchId().isNull()) _pPasteOp->excludeIds.insert(getSketchId());
        const std::set<wydb::ElementId>& newlyCreatedIds = _pPasteOp->pPasteElements->getNewlyCreatedElements();
        _pPasteOp->excludeIds.insert(newlyCreatedIds.cbegin(), newlyCreatedIds.cend());
    }
    else
    {
        assert(false);
        _pPasteOp = nullptr;
        return false;
    }

    Application::instance().getStatusBar()->reset();
    Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SelectGuiCmd",
        "Specify the insertion point; you can directly input the coordinate values."));

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 禁用选择
    this->disableSelect();

    return true;
}

void SelectGuiCmd::endPaste(const wy::Vector3& pos)
{
    if (_pPasteOp)
    {
        _pPasteOp->pPasteElements->perform(pos);
        _pPasteOp = nullptr;
    }

    Application::instance().getStatusBar()->reset();
    this->updateSelectTipAndLabel();

    // 启用选择
    this->enableSelect();
}

void SelectGuiCmd::cancelPaste()
{
    if (_pPasteOp)
    {
        _pPasteOp = nullptr;
    }

    Application::instance().getStatusBar()->reset();
    this->updateSelectTipAndLabel();

    // 启用选择
    this->enableSelect();
}

bool SelectGuiCmd::tryAddElementPositionGizmo(const wyap::SelectionSet& sels)
{
    std::list<wyap::GizmoSPtr> gizmos;
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    GizmoFactory* pGizmoFactory = Application::instance().getGizmoFactory();
    if (!pGizmoFactory) return false;

    if (!tryAddPositionGizmo_Impl(sels, gizmos))
    {
        return false;
    }

    Application::instance().getGizmoManager()->beginChange();
    Application::instance().getGizmoManager()->clearGizmos();
    for (const wyap::GizmoSPtr& pGizmo : gizmos)
    {
        Application::instance().getGizmoManager()->addGizmo(pGizmo);
    }
    Application::instance().getGizmoManager()->endChange();

    return true;
}

// ============================================================================
// SelectGuiCmdMenu
// ============================================================================

SelectGuiCmdMenu::SelectGuiCmdMenu(GuiCommand* pCmd)
    : GuiCmdMenu(pCmd)
{
}

bool SelectGuiCmdMenu::initCustomMiddleActions(QMenu* menu)
{
    assert(menu);

    bool added = false;

    // Copy: only visible when elements are selected.
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (!ss.isEmpty())
    {
        QAction* pActionCopy = new QAction(
            QIcon(":/images/Edit_Copy.svg"),
            QCoreApplication::translate("MainWindow", "Copy"), menu);
        menu->addAction(pActionCopy);
        connect(pActionCopy, &QAction::triggered, this, &SelectGuiCmdMenu::onCopy);
        added = true;
    }

    // Paste: only visible when clipboard has content.
    if (CopyPasteUtil::canPaste())
    {
        QAction* pActionPaste = new QAction(
            QIcon(":/images/Edit_PasteClip.svg"),
            QCoreApplication::translate("MainWindow", "Paste"), menu);
        menu->addAction(pActionPaste);
        connect(pActionPaste, &QAction::triggered, this, &SelectGuiCmdMenu::onPaste);
        added = true;
    }

    return added;
}

void SelectGuiCmdMenu::onCopy()
{
    SelectGuiCmd* pCmd = dynamic_cast<SelectGuiCmd*>(_pCmd);
    if (pCmd) pCmd->copy();
}

void SelectGuiCmdMenu::onPaste()
{
    SelectGuiCmd* pCmd = dynamic_cast<SelectGuiCmd*>(_pCmd);
    if (!pCmd) return;
    QPoint screenPos = QCursor::pos();
    double wx, wy;
    pCmd->screenToWindowPos(screenPos.x(), screenPos.y(), wx, wy);
    pCmd->beginPaste(wx, wy);
}

GuiCmdMenu* SelectGuiCmd::initContextMenu()
{
    return new SelectGuiCmdMenu(this);
}

