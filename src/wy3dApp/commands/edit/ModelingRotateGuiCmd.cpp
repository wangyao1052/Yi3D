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

#include "ModelingRotateGuiCmd.h"

#include <wy3dSolid.h>
#include <wy3dSheet.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "commands/edit/MoveRotateGuiCmdUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "select/SketchPlaneSelFilter.h"
#include "snap/SnapObject.h"
#include "utils/GuiCommandUtil.h"


class RotateGuiCmdPreFilter_Modeling : public SelectPreFilterFunctor
{
public:
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        assert(pDb);
        if (id.isNull())
        {
            return SelectFilterStatus::Break;
        }

        if (!GuiCommandUtil::isTopLevelBody(pDb->getElement(id)))
        {
            return SelectFilterStatus::Continue;
        }

        return SelectFilterStatus::Ok;
    }
};


ModelingRotateGuiCmd::ModelingRotateGuiCmd() : RotateGuiCmd()
{
}

ModelingRotateGuiCmd::~ModelingRotateGuiCmd()
{
}

void ModelingRotateGuiCmd::onStart_EnvSpecific()
{
    // 过滤出有效选择集
    wyap::SelectionSet filterSS = getValidSSFromCurrentSelSet_MoveRotateGuiCmd();
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->setSelections(filterSS);
    Application::instance().getSelManager()->endChange();

    // 选择工作平面点选选项:基准面
    _pickWorkPlnOption.pickMask = static_cast<unsigned int>(ElementNodeType::DatumPlane);
    _pickWorkPlnOption.selType = wy3d::SelectionType::Element;
    _pickWorkPlnOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();
}

void ModelingRotateGuiCmd::cleanupEnvSpecific()
{
    // 隐藏工作平面坐标系
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }

    // 移除工作平面原点捕捉对象
    if (_pWorkPlnOriginSnapObject)
    {
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        pSnapSys->beginChange();
        {
            pSnapSys->removeResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();
    }
}

const wy3d::SketchPlane& ModelingRotateGuiCmd::getActivePlane() const
{
    return _workPln;
}

SketchSnapSystem* ModelingRotateGuiCmd::getActiveSnapSystem() const
{
    return _pWorkPlnSnapSystem.get();
}

GuiCmdEnvType ModelingRotateGuiCmd::getEnvType() const
{
    return GuiCmdEnvType::Modeling;
}

void ModelingRotateGuiCmd::gotoNextStepAfterSelectElements()
{
    this->gotoStep(Step::SelectWorkingPlane);
}

void ModelingRotateGuiCmd::configureSelectElementOptions(GuiCmdSelectOptions& options)
{
    options.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::Sheet);
    options.preFilter = std::make_shared<RotateGuiCmdPreFilter_Modeling>();
    options.filter = std::make_shared<MultiClassSelFilter>(
        std::vector<wyrx::ClassInfo*>{wy3d::Solid::classInfo(), wy3d::Sheet::classInfo()});
}
