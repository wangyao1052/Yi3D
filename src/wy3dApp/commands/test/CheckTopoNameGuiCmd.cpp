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

#include "CheckTopoNameGuiCmd.h"

#include <string>
#include <sstream>

#include <QCoreApplication>
#include <QToolTip>
#include <QInputDialog>
#include <QMessageBox>

#include <TopExp.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSolid.h>
#include <wy3dSolid.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"
#include <wy3dSheet.h>
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/ChamferDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"


// 前置过滤器: 确保只能选择单一主体的面或边
class CheckTopoNameGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        assert(pDb);
        if (id.isNull()) return SelectFilterStatus::Continue;
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElem);
        if (!pSolid && !pSheet) return SelectFilterStatus::Break;

        return SelectFilterStatus::Ok;
    }
};

CheckTopoNameGuiCmd::CheckTopoNameGuiCmd() : OsgGuiCommand()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

CheckTopoNameGuiCmd::~CheckTopoNameGuiCmd()
{
}

wyap::CmdExecution::StartResult CheckTopoNameGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::Sheet);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelPreFilter = std::make_shared<CheckTopoNameGuiCmdPreSelFilter>();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 禁用输入
    // 提示信息
    Application::instance().getStatusBar()->setTips("");

    // 鼠标样式
    Application::instance().setCursor(CursorType::SelectElements);

    // 预览
    _pPreview = nullptr;

    return wyap::CmdExecution::StartResult::Succeeded;
}
void CheckTopoNameGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

}
void CheckTopoNameGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);

}

void CheckTopoNameGuiCmd::reset()
{
    _pPreview = nullptr;
}

void CheckTopoNameGuiCmd::checkTopoName(const wyap::Selection& sel)
{
    std::vector<std::string> checkInfo;
    wydb::ElementId id = sel.getElementId();
    if (_id2CheckInfo.find(id) != _id2CheckInfo.cend())
    {
        checkInfo = _id2CheckInfo[id];
    }
    else
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElem);
        if (!pSolid && !pSheet) return;
        const wy3d::TopoNaming* pTopoNaming = pSolid
            ? pSolid->getTopoNaming() : pSheet->getTopoNaming();
        if (!pTopoNaming) return;
        TopoDS_Shape shape = pSolid ? pSolid->getShape() : pSheet->getShape();
        bool isValid = pTopoNaming->check(shape, checkInfo);
        if (isValid) checkInfo.insert(checkInfo.cbegin(), "ok");
        else checkInfo.insert(checkInfo.cbegin(), "***error***");
        _id2CheckInfo[id] = checkInfo;
    }

    std::string strTotal;
    for (const std::string& infoItem : checkInfo)
    {
        strTotal += infoItem + "; ";
    }
    QToolTip::showText(QCursor::pos(), strTotal.c_str(), nullptr, QRect(), 30000); // 30s
    Application::instance().getStatusBar()->setTips(strTotal.c_str());
}

void CheckTopoNameGuiCmd::onMouseMove(const MouseEvent& event)
{
    // 点选预览
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
    Application::instance().getStatusBar()->setTips("");
    if (_pPreview)
    {
        wyap::Selection sel = _pPreview->getSelection();
        this->checkTopoName(sel);
    }

    return;
}

GuiCmdMenu* CheckTopoNameGuiCmd::initContextMenu()
{
    return new CheckTopoNameGuiCmdMenu(this);
}

bool CheckTopoNameGuiCmdMenu::initCustomHeaderActions(QMenu* menu)
{
    assert(menu);

    // Check All
    QAction* pActionCheckAll = new QAction("Check All", menu);
    menu->addAction(pActionCheckAll);
    this->connect(pActionCheckAll, &QAction::triggered, this, &CheckTopoNameGuiCmdMenu::onCheckAll);

    return true;
}

void CheckTopoNameGuiCmdMenu::onCheckAll()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    std::vector<wydb::ElementId> invalidIds;
    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElem);
        if (!pSolid && !pSheet) continue;
        const wy3d::TopoNaming* pTopoNaming = pSolid
            ? pSolid->getTopoNaming() : pSheet->getTopoNaming();
        if (!pTopoNaming) return;
        TopoDS_Shape shape = pSolid ? pSolid->getShape() : pSheet->getShape();

        std::vector<std::string> checkInfo;
        bool isValid = pTopoNaming->check(shape, checkInfo);
        if (!isValid)
        {
            invalidIds.emplace_back(id);
        }
    }

    // 所有设计体(顶层实体)的拓扑命名都是有效的
    if (invalidIds.empty())
    {
        QMessageBox::information(nullptr, "Yi3D", "All is valid.");
    }
    else
    {
        std::stringstream ss;
        ss << "Invalid solids: ";
        for (const wydb::ElementId& id : invalidIds)
        {
            ss << id.value() << ";";
        }
        QMessageBox::warning(nullptr, "Yi3D", ss.str().c_str());
    }
}