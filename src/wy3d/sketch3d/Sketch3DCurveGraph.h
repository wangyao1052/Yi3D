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

#ifndef WY3D_SKETCH3D_CURVE_GRAPH_H
#define WY3D_SKETCH3D_CURVE_GRAPH_H

#include <vector>

#include <wy3dDefs.h>
#include <wydbElementId.h>

NS_WY3D_BEG

class Sketch3D;
class SketchCurve3D;

// The 3D counterpart of SketchCurveGraph: collects the curves of a 3D sketch,
// fuses their endpoints into shared vertices by quantized coordinates and
// records the adjacency. Purely curve-level, it builds no topology. Shared by
// Sketch3DProfile and Sketch3DPath, which impose their own rules on the result
class Sketch3DCurveGraph
{
public:
    struct VertexInfo
    {
        // Incident curves; a self-closed curve is counted twice (degree 2)
        std::vector<int> adjacentCurves;
        // Curves starting / ending here; a self-closed curve is in both
        std::vector<int> outgoingCurves;
        std::vector<int> incomingCurves;
    };

    struct CurveEnds
    {
        // Vertex indices of the start / end point (equal for a self-closed curve)
        int v1;
        int v2;
    };

public:
    explicit Sketch3DCurveGraph(const Sketch3D* pSketch3D, double tol = 1e-5);

    // Whether every child was a usable curve; see getRejectedCurveId
    bool isCollected() const { return _isCollected; }

    // The curve that broke the collection (kNull if it failed for other reasons)
    const wydb::ElementId& getRejectedCurveId() const { return _rejectedCurveId; }

    const std::vector<const SketchCurve3D*>& getCurves() const { return _curves; }
    const std::vector<wydb::ElementId>& getCurveIds() const { return _curveIds; }
    const std::vector<CurveEnds>& getCurveEnds() const { return _curveEnds; }
    const std::vector<VertexInfo>& getVertexInfos() const { return _vertexInfos; }

    // Sorted, de-duplicated ids of the given curve indices
    std::vector<wydb::ElementId> idsOfCurves(const std::vector<int>& indices) const;

private:
    bool collect();
    bool fuseVertices();

private:
    const Sketch3D* _pSketch3D;
    double _tol;
    bool _isCollected;
    wydb::ElementId _rejectedCurveId;

    std::vector<const SketchCurve3D*> _curves;
    std::vector<wydb::ElementId> _curveIds;
    std::vector<CurveEnds> _curveEnds;
    std::vector<VertexInfo> _vertexInfos;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_CURVE_GRAPH_H
