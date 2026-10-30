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

#ifndef WY3DAPP_ELEMENT_MESH_INFO_H
#define WY3DAPP_ELEMENT_MESH_INFO_H

#include <vector>

namespace wy3dApp {

// 面/边信息: 实体节点与片体节点共用一套, 索引均为形体上的拓扑面/边序号
// Internal to the built-in Solid/Sheet nodes; not part of the plugin SDK.
enum class FaceInfoFlag
{
    Highlight = 0x00000001,
};
struct FaceInfo
{
    unsigned int numTriangles;
    std::vector<int> edgeIndices; // 序号从0开始
    unsigned int flags;

    inline void addFlag(FaceInfoFlag flag)
    {
        flags |= static_cast<unsigned int>(flag);
    }
    inline void removeFlag(FaceInfoFlag flag)
    {
        flags &= ~static_cast<unsigned int>(flag);
    }
    inline bool hasFlag(FaceInfoFlag flag) const
    {
        return flags & static_cast<unsigned int>(flag);
    }

    FaceInfo() : numTriangles(0), flags(0) {}
};

enum class EdgeInfoFlag
{
    Highlight = 0x00000001,
};
struct EdgeInfo
{
    unsigned int numLines;
    unsigned int flags;

    inline void addFlag(EdgeInfoFlag flag)
    {
        flags |= static_cast<unsigned int>(flag);
    }
    inline void removeFlag(EdgeInfoFlag flag)
    {
        flags &= ~static_cast<unsigned int>(flag);
    }
    inline bool hasFlag(EdgeInfoFlag flag) const
    {
        return flags & static_cast<unsigned int>(flag);
    }

    EdgeInfo() : numLines(0), flags(0) {}
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_MESH_INFO_H
