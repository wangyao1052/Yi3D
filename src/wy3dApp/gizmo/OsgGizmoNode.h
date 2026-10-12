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

#ifndef WY3DAPP_OSG_GIZMO_NODE_H
#define WY3DAPP_OSG_GIZMO_NODE_H

#include <osg/PositionAttitudeTransform>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Gizmo的OSG节点
class OsgGizmoNode : public osg::PositionAttitudeTransform
{
public:
    OsgGizmoNode();
    explicit OsgGizmoNode(const OsgGizmoNode& rhs, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    virtual ~OsgGizmoNode();
    META_Node(wy3dApp, OsgGizmoNode);

    // 颜色
    static const osg::Vec4 RED_COLOR;
    static const osg::Vec4 GREEN_COLOR;
    static const osg::Vec4 BLUE_COLOR;
    static const osg::Vec4 YELLOW_COLOR;
    static const osg::Vec4 SKETCH_ENTITY_COLOR;
};

} // namespace wy3dApp

#endif // WY3DAPP_OSG_GIZMO_NODE_H
