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

#include "utils/SketchPolygonRecognizer.h"

#include <cmath>
#include <memory>
#include <utility>

#include <wyMath.h>

#include "utils/SketchClosedLoop.h"
#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static bool detectSketchPolygon(const std::vector<const wy3d::SketchLine*>& lines,
                                SketchPolygonInfo& outInfo,
                                double angleTol,
                                double distTol)
{
    const int sides = static_cast<int>(lines.size());
    if (sides < kMinPolygonSides || sides > kMaxPolygonSides) return false;
    // A square keeps the rectangle gizmo (width/height labels)
    if (4 == sides) return false;
    if (angleTol <= 0.0 || distTol <= 0.0) return false;

    // 1. Cluster the endpoints and walk the ring (same sketch, minimum length,
    // no branch, distinct vertices)
    SketchClosedLoop loop;
    if (!buildSketchClosedLoop(lines, distTol, loop)) return false;

    // 2. Equal edges, equal turns, convex
    const PolygonRegularity regularity = validateRegularPolygon(loop.corners, angleTol, distTol);
    if (!regularity.isValid) return false;

    // 3. Normalize: counter-clockwise, starting at the vertex closest to +X
    const double signedArea = computeClosedLoopSignedArea(loop.corners, regularity.center);
    if (std::fabs(signedArea) <= wy::EPS) return false;
    if (signedArea < 0.0) reverseClosedLoopRing(loop);

    const int start = selectCanonicalPolygonStart(loop.corners, regularity.center);
    if (start < 0) return false;
    rotateClosedLoopStart(loop, start);

    // 4. Fill the result
    outInfo = SketchPolygonInfo();
    outInfo.sketchId = loop.sketchId;
    outInfo.sides = sides;
    outInfo.lineIds = loop.lineIds;
    outInfo.corners = loop.corners;
    outInfo.center = regularity.center;
    outInfo.circumRadius = regularity.circumRadius;
    outInfo.inRadius = regularity.inRadius;
    outInfo.rotation = computePolygonRotation(loop.corners, regularity.center);
    outInfo.labelEdge = selectPolygonLabelEdge(loop.corners, regularity.center);
    outInfo.angleTol = angleTol;
    outInfo.distTol = distTol;
    return true;
}

std::shared_ptr<const SketchPolygonInfo> tryDetectSketchPolygon(
    const std::vector<const wy3d::SketchLine*>& lines,
    double angleTol,
    double distTol)
{
    SketchPolygonInfo info;
    if (!detectSketchPolygon(lines, info, angleTol, distTol)) return nullptr;
    return std::make_shared<const SketchPolygonInfo>(std::move(info));
}

} // namespace wy3dApp
