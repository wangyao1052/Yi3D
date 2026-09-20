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

#ifndef WY3D_SKETCH3D_TRIM_GRAPH_H
#define WY3D_SKETCH3D_TRIM_GRAPH_H

#include <map>
#include <memory>

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>

#include <utils/wy3dSketch3DTrimNode.h>

NS_WY3D_BEG

class Sketch3D;
class SketchCurve3D;

// Every curve in a sketch, with each crossing recorded on both curves that share it. A trim asks
// the graph which piece the cursor is over; the graph is rebuilt or patched after the edit so the
// remaining pieces keep answering correctly.
class WY3D_EXPORT Sketch3DTrimGraph
{
public:
    Sketch3DTrimGraph(const Sketch3D* pSketch3D, double tol);

    bool isValid() const { return _isValid; }

    // The sketch's database. The 2D graph reached for the application singleton here; a 3D sketch
    // is not tied to one, so the graph carries it instead.
    wydb::Database* getDatabase() const;

    Sketch3DTrimNodeSPtr getNode(const wydb::ElementId& id) const;
    bool addNode(Sketch3DTrimNodeSPtr pNode);

    Sketch3DTrimSegment pick(const wydb::ElementId& id, const wy::Vector3& pos);

private:
    bool init();

private:
    const Sketch3D* _pSketch3D;
    double _tol;
    bool _isValid;

    std::map<wydb::ElementId, Sketch3DTrimNodeSPtr> _id2Node;
};

typedef std::shared_ptr<Sketch3DTrimGraph> Sketch3DTrimGraphSPtr;

NS_WY3D_END

#endif // WY3D_SKETCH3D_TRIM_GRAPH_H
