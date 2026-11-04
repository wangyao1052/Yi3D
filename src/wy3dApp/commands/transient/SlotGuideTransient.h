///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_COMMANDS_TRANSIENT_SLOT_GUIDE_TRANSIENT_H
#define WY3DAPP_COMMANDS_TRANSIENT_SLOT_GUIDE_TRANSIENT_H

#include <osg/Array>
#include <osg/ref_ptr>

#include <wyVector2.h>
#include <wy3dApp/GuiCmdTransient.h>
#include <wy3dPlane.h>

namespace osg
{
class Geometry;
}

namespace wy3dApp {

// Dashed centerline used while picking the two slot centers. It is kept on
// its own transient node so it can stay visible while the slot outline preview
// is hidden (for example when the width is still zero).
class SlotGuideTransient : public GuiCmdTransient
{
public:
    explicit SlotGuideTransient(const wy3d::Plane& plane);

    void update(const wy::Vector2& start, const wy::Vector2& end);
    void hideLine();

private:
    wy3d::Plane _plane;
    osg::ref_ptr<osg::Vec3Array> _vertices;
    osg::ref_ptr<osg::Geometry> _geometry;
};

} // namespace wy3dApp

#endif // WY3DAPP_COMMANDS_TRANSIENT_SLOT_GUIDE_TRANSIENT_H
