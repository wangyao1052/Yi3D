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

#ifndef WY3DAPP_ELEMENT_RENDER_DATA_TYPES_H
#define WY3DAPP_ELEMENT_RENDER_DATA_TYPES_H

#include <vector>

#include <osg/Array>

#include <wy3dApp/ElementRenderData.h>

#include "scene/nodes/ElementMeshInfo.h"

namespace wy3dApp {

class LineRenderData : public ElementRenderData
{
public:
    osg::ref_ptr<osg::UIntArray> lineIndices = new osg::UIntArray();
};

class MeshRenderData : public ElementRenderData
{
public:
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    osg::ref_ptr<osg::UIntArray> triangleIndices = new osg::UIntArray();
    osg::ref_ptr<osg::UIntArray> lineIndices = new osg::UIntArray();
    std::vector<FaceInfo> faceInfos;
    std::vector<EdgeInfo> edgeInfos;
};

enum class CurveRenderInfoFlag
{
    Highlight = 0x00000001,
};

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

class CurveRenderData : public LineRenderData
{
public:
    std::vector<CurveRenderInfo> curveInfos;
};

class SketchRenderData : public CurveRenderData
{
public:
    osg::ref_ptr<osg::UIntArray> centerLineIndices = new osg::UIntArray();
    osg::ref_ptr<osg::UIntArray> pointIndices = new osg::UIntArray();
    std::vector<CurveRenderInfo> centerLineInfos;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_RENDER_DATA_TYPES_H
