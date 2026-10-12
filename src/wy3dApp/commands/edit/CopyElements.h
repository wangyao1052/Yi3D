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

#ifndef WY3DAPP_COPY_ELEMENTS_H
#define WY3DAPP_COPY_ELEMENTS_H

#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelection.h>

#include <wy3dApp/GuiCommand.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class CopyElemens : public GuiCmdMakeElement
{
public:
    CopyElemens(GuiCommand* pGuiCmd);
    ~CopyElemens();

    bool init(const wyap::SelectionSet& ss);

    bool update(const wy3d::Plane& plane, const wy::Vector2& moveVec2d);
    bool update(const wy::Vector3& moveVec);

    bool perform(const wyap::SelectionSet& ss,
        const wy::Vector3& moveVec,
        wydb::ElementId sketchId);

private:
    osg::ref_ptr<osg::PositionAttitudeTransform> _pat;
};

}

#endif // WY3DAPP_COPY_ELEMENTS_H
