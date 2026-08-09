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
#include <wy3dDatumPlane.h>
#include <wy3dSketch.h>

#include "application/Application.h"
#include "gizmo/GizmoFactory.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/TransactionUtil.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/featureTree/FeatureTreeWidget.h"


class SelectGuiCmdSelFilter_Modeling : public SelectFilterFunctor
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


ModelingSelectGuiCmd::ModelingSelectGuiCmd() : SelectGuiCmd()
{
}

ModelingSelectGuiCmd::~ModelingSelectGuiCmd()
{
}

void ModelingSelectGuiCmd::configureSelectOptions(GuiCmdSelectOptions& options)
{
    options.pickMask = static_cast<unsigned int>(
        ElementNodeType::Solid |
        ElementNodeType::Sheet |
        ElementNodeType::Sketch |
        ElementNodeType::DatumPlane |
        ElementNodeType::Curve);
    options.filter = std::make_shared<SelectGuiCmdSelFilter_Modeling>();
}

void ModelingSelectGuiCmd::onStart_EnvSpecific()
{
    Application::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setSelectable(true);
}

void ModelingSelectGuiCmd::selectAll_Impl(wyap::SelectionSet& ss)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
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
        // Ctrl+A选中基准面是没有意义的
        if (pElem->getClassInfo() == wy3d::DatumPlane::classInfo())
        {
            continue;
        }
        if (pElem->getParent() == wydb::ElementId::kNull)
        {
            ss.add(wyap::Selection(id));
        }
    }
}

bool ModelingSelectGuiCmd::tryAddPositionGizmo_Impl(const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos)
{
    if (TransactionUtil::hasActiveTransaction())
    {
        return false;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return true;
    GizmoFactory* pGizmoFactory = Application::instance().getGizmoFactory();
    if (!pGizmoFactory) return true;

    // 获取实体特征
    if (sels.getCount() != 1) return true;
    const wyap::Selection& sel = sels.createIterator().current();
    // 只有选择了单个整体元素才触发Gizmo
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element) return true;
    wydb::ElementId id = sel.getElementId();
    const wydb::Element* pElem = pDb->getElement(id);
    if (!pElem) return true;
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (!pSolid) return true;

    gizmos = pGizmoFactory->createGizmos(pSolid);
    return true;
}

GuiCmdEnvType ModelingSelectGuiCmd::getPasteEnvType() const
{
    return GuiCmdEnvType::Modeling;
}

wy::Vector3 ModelingSelectGuiCmd::computePastePosition(double x, double y,
    const std::set<wydb::ElementId>& excludeIds)
{
    auto ret = this->computePosition3d(x, y, getSketchPlane(), excludeIds);
    return ret.second ? ret.second->getPosition() : ret.first;
}
