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

#include "ModelingSelectGuiCmd.h"

#include <wy3dSolid.h>
#include <wy3dDatum.h>
#include <wy3dSketch.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/GizmoFactory.h>
#include "utils/TransactionUtil.h"
#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/CommonSelFilters.h>
#include "utils/IconUtil.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/featureTree/FeatureTreeWidget.h"
#include <wy3dApp/CommandAction.h>
#include <QMenu>
#include "commands/CommandNames.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

void ModelingSelectGuiCmd::createSelectionGizmos(
    const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos)
{
    // 事务进行中不创建内置 Gizmo（草图环境有常驻事务组，不能套用此检查）
    if (TransactionUtil::hasActiveTransaction())
    {
        return;
    }

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    GizmoFactory* pGizmoFactory = ApplicationImpl::instance().getGizmoFactory();
    if (!pGizmoFactory) return;

    // 获取实体特征
    if (sels.getCount() != 1) return;
    const wyap::Selection& sel = sels.createIterator().current();
    // 只有选择了单个整体元素才触发Gizmo
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element) return;
    wydb::ElementId id = sel.getElementId();
    const wydb::Element* pElem = pDb->getElement(id);
    if (!pElem) return;
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (!pSolid) return;

    gizmos = pGizmoFactory->createGizmos(pSolid);
}

class SelectGuiCmdSelFilter_Modeling : public SelectPostFilterFunctor
{
public:
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
            // 参照SolidWorks的行为:框选时不选中基准对象(基准面/基准轴/基准点等)
            if (wy3d::Datum::cast(pDb->getElement(sel.getElementId())))
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


ModelingSelectGuiCmd::ModelingSelectGuiCmd() : SelectGuiCmd()
{
}

ModelingSelectGuiCmd::~ModelingSelectGuiCmd()
{
}

void ModelingSelectGuiCmd::initializeSelectOptions(SelectOptions& selectOptions)
{
    SelectGuiCmd::initializeSelectOptions(selectOptions);
    // 建模环境下场景里可拾取的元素都应能选中。内部对象不可拾取:
    // - SketchEntity/Sketch3DEntity 只在草图环境加入场景(addElementNode 的 addSketchEntity 为 false 时直接返回);
    // - BodyModification 不渲染面、边节点 NodeMask 为 0 或 ~All、包围盒为空不进 RTree.
    // 因此用 All,让后续新增的顶层元素类型自动可选.
    selectOptions.allowedElemTypes = ElementNodeType::All;
    selectOptions.postFilter = std::make_shared<SelectGuiCmdSelFilter_Modeling>();
}

void ModelingSelectGuiCmd::onStart_EnvSpecific()
{
    ApplicationImpl::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setSelectable(true);
}

void ModelingSelectGuiCmd::selectAll_Impl(wyap::SelectionSet& ss)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;

    std::set<wydb::ElementId> ids;
    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        ids.insert(iter.current());
    }
    for (const wydb::ElementId& id : ids)
    {
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem) continue;
        // Ctrl+A选中基准对象(基准面/基准轴/基准点等)是没有意义的
        if (wy3d::Datum::cast(pElem))
        {
            continue;
        }
        if (pElem->getParent() == wydb::ElementId::kNull)
        {
            ss.add(wyap::Selection(id));
        }
    }
}

wy::Vector3 ModelingSelectGuiCmd::computePastePosition(double x, double y,
    const std::set<wydb::ElementId>& excludeIds)
{
    auto ret = this->computePoint(x, y, getSketchPlane(), true, excludeIds);
    return ret.second ? ret.second->getPosition() : ret.first;
}

void ModelingSelectGuiCmd::configureContextMenu(ContextMenu& menu)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }

    const wyap::SelectionSet& ss = ApplicationImpl::instance().getSelManager()->getSelections();
    bool addShowAction(false);
    bool addHideAction(false);
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element) continue;
        const wydb::Element* pElem = pDb->getElement(sel.getElementId());
        if (!pElem) continue;
        if (pElem->isHidden())
        {
            addShowAction = true;
        }
        else
        {
            addHideAction = true;
        }
    }

    if (addShowAction)
    {
        CommandAction* pActionShow = new CommandAction(CommandNames::Show, &menu);
        pActionShow->setText(QCoreApplication::translate("MainWindow", "Show"));
        pActionShow->setIcon(IconUtil::get(":/images/Edit_Show.svg"));
        menu.addAction(pActionShow);
    }

    if (addHideAction)
    {
        CommandAction* pActionHide = new CommandAction(CommandNames::Hide, &menu);
        pActionHide->setText(QCoreApplication::translate("MainWindow", "Hide"));
        pActionHide->setIcon(IconUtil::get(":/images/Edit_Hide.svg"));
        menu.addAction(pActionHide);
    }

    SelectGuiCmd::configureContextMenu(menu);
}

} // namespace wy3dApp
