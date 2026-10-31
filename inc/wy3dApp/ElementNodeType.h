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

#ifndef WY3DAPP_ELEMENT_NODE_TYPE_H
#define WY3DAPP_ELEMENT_NODE_TYPE_H

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Underlying type matches osg::NodeMask (unsigned int).
// 低 24 位为类型/可拾取区, 高 8 位为"仅渲染、不可拾取"区(叠加几何/ghost 等使用 ~All).
enum ElementNodeType : unsigned int
{
    Undefined         = 0,
    Solid             = 0x00000001,
    Sheet             = 0x00000002,
    BodyModification  = 0x00000004,
    Curve             = 0x00000008,
    Datum             = 0x00000010,
    Sketch            = 0x00000020,
    SketchEntity      = 0x00000040,
    Sketch3D          = 0x00000080,
    Sketch3DEntity    = 0x00000100,
    All               = 0x00FFFFFF,
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_TYPE_H
