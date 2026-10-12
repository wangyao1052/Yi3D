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

#ifndef WY3DAPP_SKETCH_EXTEND_GRAPH_H
#define WY3DAPP_SKETCH_EXTEND_GRAPH_H

#include <map>
#include <RTree/RTree.h>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>

#include "SketchExtendNode.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SketchExtendGraph
{
public:
    SketchExtendGraph(const wy3d::Sketch* pSketch, double tol);

    // 是否有效
    bool isValid() const { return _isValid; }

    // 获取结点
    SketchExtendNodeSPtr getNode(const wydb::ElementId& id) const;
    // 添加结点
    bool addNode(SketchExtendNodeSPtr pNode);

    // 拾取段
    SketchExtendSegment pick(const wydb::ElementId& id, const wy::Vector2& position);

private:
    bool init();

private:
    const wy3d::Sketch* _pSketch;
    double _tol;
    bool _isValid;

    // 所有结点
    std::map<wydb::ElementId, SketchExtendNodeSPtr> _id2Node;
};

}

#endif // WY3DAPP_SKETCH_EXTEND_GRAPH_H
