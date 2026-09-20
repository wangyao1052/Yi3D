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

#ifndef WY3D_SKETCH3D_EXTEND_GRAPH_H
#define WY3D_SKETCH3D_EXTEND_GRAPH_H

#include <map>
#include <memory>

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>

#include <utils/wy3dSketch3DExtendNode.h>

NS_WY3D_BEG

class Sketch3D;
class SketchCurve3D;

// Records, for every curve that could grow, the crossings lying on its own extension. Where the
// trim graph asks "which piece is under the cursor", this one asks "how far would this curve have
// to grow to touch something".
//
// The 2D version needed a separate ray-intersection routine per curve type and a loop per pairing;
// here every curve grows through Sketch3DCurveIntersectionUtil's extended operand, so one loop
// covers all of them.
class WY3D_EXPORT Sketch3DExtendGraph
{
public:
    Sketch3DExtendGraph(const Sketch3D* pSketch3D, double tol);

    bool isValid() const { return _isValid; }

    wydb::Database* getDatabase() const;

    Sketch3DExtendNodeSPtr getNode(const wydb::ElementId& id) const;
    bool addNode(Sketch3DExtendNodeSPtr pNode);

    Sketch3DExtendSegment pick(const wydb::ElementId& id, const wy::Vector3& pos);

private:
    bool init();

private:
    const Sketch3D* _pSketch3D;
    double _tol;
    bool _isValid;

    std::map<wydb::ElementId, Sketch3DExtendNodeSPtr> _id2Node;
};

typedef std::shared_ptr<Sketch3DExtendGraph> Sketch3DExtendGraphSPtr;

NS_WY3D_END

#endif // WY3D_SKETCH3D_EXTEND_GRAPH_H
