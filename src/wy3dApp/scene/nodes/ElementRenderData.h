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

#include <vector>

#include <osg/Array>

#include <wy3dApp/ElementNode.h>

#include "scene/nodes/ElementMeshInfo.h"

namespace wy3dApp {

// 只有顶点和线索引的渲染数据
class LineRenderData : public RenderData
{
public:
    osg::ref_ptr<osg::UIntArray> lineIndices = new osg::UIntArray();
};

// 网格渲染数据:顶点、法线、三角面片索引、线索引、面/边拓扑信息
// 与 LineRenderData 平级:网格不是线,只是同时包含三角面与边线数据.
class MeshRenderData : public RenderData
{
public:
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    osg::ref_ptr<osg::UIntArray> triangleIndices = new osg::UIntArray();
    osg::ref_ptr<osg::UIntArray> lineIndices = new osg::UIntArray();
    // 面数据
    std::vector<FaceInfo> faceInfos;
    // 边数据
    std::vector<EdgeInfo> edgeInfos;
};

// 曲线高亮标志
enum class CurveRenderInfoFlag
{
    Highlight = 0x00000001,
};

// 曲线渲染信息
struct CurveRenderInfo
{
    unsigned int id;
    unsigned int numLines;
    unsigned int flags;

    inline void addFlag(CurveRenderInfoFlag flag)
    {
        flags |= static_cast<unsigned int>(flag);
    }
    inline void removeFlag(CurveRenderInfoFlag flag)
    {
        flags &= ~static_cast<unsigned int>(flag);
    }
    inline bool hasFlag(CurveRenderInfoFlag flag) const
    {
        return flags & static_cast<unsigned int>(flag);
    }

    CurveRenderInfo() : id(0), numLines(0), flags(0) {}
};

// 曲线渲染数据:线 + 曲线信息
class CurveRenderData : public LineRenderData
{
public:
    std::vector<CurveRenderInfo> curveInfos;
};

// 草图渲染数据:普通曲线、中心线、点
class SketchRenderData : public CurveRenderData
{
public:
    osg::ref_ptr<osg::UIntArray> centerLineIndices = new osg::UIntArray();
    osg::ref_ptr<osg::UIntArray> pointIndices = new osg::UIntArray();
    std::vector<CurveRenderInfo> centerLineInfos;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_RENDER_DATA_H
