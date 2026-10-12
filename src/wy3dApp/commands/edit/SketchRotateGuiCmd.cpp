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

#include "SketchRotateGuiCmd.h"

#include <wy3dSketchEntity.h>

#include "scene/nodes/ElementNodeType.h"
#include <wy3dApp/CommonSelFilters.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchRotateGuiCmd::SketchRotateGuiCmd() : RotateGuiCmd()
{
}

SketchRotateGuiCmd::~SketchRotateGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchRotateGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    return ret;
}

const wy3d::Plane& SketchRotateGuiCmd::getActivePlane() const
{
    return _sketchInfo.sketchPlane;
}

SketchSnapSystem* SketchRotateGuiCmd::getActiveSnapSystem() const
{
    return _sketchInfo.pSketchSnapSys;
}

void SketchRotateGuiCmd::gotoNextStepAfterSelectElements()
{
    this->gotoStep(Step::SpecifyBasePnt);
}

void SketchRotateGuiCmd::configureSelectElementOptions(SelectOptions& options)
{
    options.pickMask = ElementNodeType::SketchEntity;
    options.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity::classInfo());
}

} // namespace wy3dApp
