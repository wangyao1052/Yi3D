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

#ifndef WY3DAPP_MOVE_ELEMENTS_H
#define WY3DAPP_MOVE_ELEMENTS_H

#include <osg/PositionAttitudeTransform>

#include <wyapSelection.h>

#include <wy3dApp/GuiCommand.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MoveElemens : public GuiCmdMakeElement
{
public:
    MoveElemens(GuiCommand* pGuiCmd);
    ~MoveElemens();

    bool init(const wyap::SelectionSet& ss);
    bool update(const wy3d::Plane& plane, const wy::Vector2& moveVec2d);
    bool update(const wy::Vector3& moveVec);
    bool perform(const wyap::SelectionSet& ss, const wy::Vector3& moveVec);

private:
    osg::ref_ptr<osg::PositionAttitudeTransform> _pat;
};

} // namespace wy3dApp

#endif // WY3DAPP_MOVE_ELEMENTS_H
