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

#ifndef WY3DAPP_MAKE_TEXT_H
#define WY3DAPP_MAKE_TEXT_H

#include <set>

#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dPlane.h>
#include <wy3dMakeSketchText.h>

#include <wy3dApp/GuiCommand.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class MakeText : public GuiCmdMakeElement
{
public:
    explicit MakeText(GuiCommand* pGuiCmd);
    ~MakeText();

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool create(wydb::ElementId sketchId, const wy3d::MakeSketchText::Data& data);
    bool update(const wy::Vector2& pos);
    bool performTranslate(const wy::Vector2& pos);

private:
    void initCopyNodes();

private:
    wy3d::Plane _sketchPlane;
    std::set<wydb::ElementId> _ids;
    osg::ref_ptr<osg::PositionAttitudeTransform> _pat;
};

}

#endif // WY3DAPP_MAKE_TEXT_H
