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

#ifndef WY3DAPP_PICK_COMMON_H
#define WY3DAPP_PICK_COMMON_H

#include <cassert>
#include <osg/Geometry>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 绘制类型
enum class DrawMode
{
    Undefined = 0,
    Face = 0x00000001,
    Edge = 0x00000002,
    Vertex = 0x00000004,
    All = Face | Edge | Vertex,
};

class PickCommon
{
public:
    static DrawMode getDrawableModeOfGeometry(osg::Geometry* geometry);
    static DrawMode getDrawableMode(osg::Drawable* drawable);
};

// 获取绘制类型
inline DrawMode PickCommon::getDrawableModeOfGeometry(osg::Geometry * geometry)
{
    if (!geometry)
    {
        assert(false);
        return DrawMode::Undefined;
    }
    const osg::Geometry::PrimitiveSetList& primSets = geometry->getPrimitiveSetList();
    if (primSets.empty())
    {
        assert(false);
        return DrawMode::Undefined;
    }
    assert(primSets.front());

    switch (primSets.front()->getMode())
    {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_QUAD_STRIP:
        return DrawMode::Face;

    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
        return DrawMode::Edge;

    case GL_POINTS:
        return DrawMode::Vertex;

    case GL_POLYGON: // 可以设置为填充也可以设置为不填充
    default:
        assert(false);
        return DrawMode::Undefined;
    }

    assert(false);
    return DrawMode::Undefined;
}

inline DrawMode PickCommon::getDrawableMode(osg::Drawable* drawable)
{
    assert(drawable);
    osg::Geometry* geometry = dynamic_cast<osg::Geometry*>(drawable);
    return getDrawableModeOfGeometry(geometry);
}

}

#endif // WY3DAPP_PICK_COMMON_H
