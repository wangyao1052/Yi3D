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

#include "OsgGizmoNode.h"
#include "scene/RenderConst.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

const osg::Vec4 OsgGizmoNode::RED_COLOR = osg::Vec4(0.7f, 0.0f, 0.0f, 1.0f);
// modified by wangyao 2025.04.27 {
// Y轴的颜色改为青色
//const osg::Vec4 OsgGizmoNode::GREEN_COLOR = osg::Vec4(0.2, 1.0, 1.0, 1.0);
const osg::Vec4 OsgGizmoNode::GREEN_COLOR = osg::Vec4(0.1, 0.75, 0.75, 1.0);
// }
const osg::Vec4 OsgGizmoNode::BLUE_COLOR = osg::Vec4(0.0f, 0.0f, 0.7f, 1.0f);
const osg::Vec4 OsgGizmoNode::YELLOW_COLOR = osg::Vec4(0.8f, 0.8f, 0.0f, 1.0f);
const osg::Vec4 OsgGizmoNode::SKETCH_ENTITY_COLOR = osg::Vec4(0.0f, 0.5f, 0.7f, 1.0f);

OsgGizmoNode::OsgGizmoNode() : osg::PositionAttitudeTransform()
{
    // 关闭灯光
    this->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 关闭深度测试
    this->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    // 后绘制
    this->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Gizmo, "RenderBin");
}

OsgGizmoNode::OsgGizmoNode(const OsgGizmoNode& rhs, const osg::CopyOp& copyop)
    : osg::PositionAttitudeTransform(rhs, copyop)
{
}

OsgGizmoNode::~OsgGizmoNode()
{
}

}
