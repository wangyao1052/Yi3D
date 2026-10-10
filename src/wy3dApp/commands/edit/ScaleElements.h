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

#ifndef WY3DAPP_SCALE_ELEMENTS_H
#define WY3DAPP_SCALE_ELEMENTS_H

#include <osg/MatrixTransform>
#include <osg/ref_ptr>

#include <wyVector2.h>
#include <wyapSelection.h>
#include <wydbElementId.h>

#include "commands/GuiCommand.h"

class ScaleElements : public GuiCmdMakeElement
{
public:
    explicit ScaleElements(GuiCommand* pGuiCmd);
    ~ScaleElements();

    bool init(const wyap::SelectionSet& ss);
    bool update(const wy3d::Plane& sketchPlane, const wy::Vector2& basePnt, const double scale);
    bool perform(const wyap::SelectionSet& ss,
        const wydb::ElementId& sketchId,
        const wy::Vector2& basePnt, const double scale);

private:
    osg::ref_ptr<osg::MatrixTransform> _pMatrixTransform;
};

#endif // WY3DAPP_SCALE_ELEMENTS_H
