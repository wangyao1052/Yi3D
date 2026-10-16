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

#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include "scene/nodes/ElementNodeType.h"
#include "commands/edit/MoveRotateGuiCmdUtil.h"
#include <wy3dApp/CommonSelFilters.h>
#include "select/SketchPlaneSelFilter.h"
#include "snap/SnapObject.h"
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

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
    _pickWorkPlnOption.allowedElemTypes = ElementNodeType::DatumPlane;
    _pickWorkPlnOption.selType = wy3d::SelectionType::Element;
    _pickWorkPlnOption.pPostFilter = std::make_shared<SketchPlaneSelFilterFunctor>();
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

const wy3d::Plane& ModelingRotateGuiCmd::getActivePlane() const
{
    return _workPln;
}

Snap2dSystem* ModelingRotateGuiCmd::getActiveSnapSystem() const
{
    return _pWorkPlnSnapSystem.get();
}

void ModelingRotateGuiCmd::gotoNextStepAfterSelectElements()
{
    this->gotoStep(Step::SelectWorkingPlane);
}

void ModelingRotateGuiCmd::configureSelectElementOptions(SelectOptions& options)
{
    options.allowedElemTypes = ElementNodeType::Solid | ElementNodeType::Sheet;
    options.preFilter = std::make_shared<RotateGuiCmdPreFilter_Modeling>();
    options.postFilter = MultiClassSelPostFilter::create<wy3d::Solid, wy3d::Sheet>();
}

} // namespace wy3dApp
