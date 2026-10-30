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

#ifndef WY3DAPP_ELEMENT_RENDER_DATA_H
#define WY3DAPP_ELEMENT_RENDER_DATA_H

#include <osg/Array>

#include <wy3dApp/ElementNode.h>

namespace wy3dApp {

// 只有顶点和线索引的渲染数据
class LineRenderData : public RenderData
{
public:
    osg::ref_ptr<osg::UIntArray> lineIndices = new osg::UIntArray();
};

// 网格渲染数据:顶点、法线、三角面片索引、线索引
class MeshRenderData : public LineRenderData
{
public:
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    osg::ref_ptr<osg::UIntArray> triangleIndices = new osg::UIntArray();
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_RENDER_DATA_H
