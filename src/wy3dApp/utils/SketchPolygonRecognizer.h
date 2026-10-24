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

#ifndef WY3DAPP_SKETCH_POLYGON_RECOGNIZER_H
#define WY3DAPP_SKETCH_POLYGON_RECOGNIZER_H

#include <memory>
#include <vector>

#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchLine.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// N lines joined end to end into a regular polygon. There is no polygon element
// in the database: the format only persists the individual lines.
struct SketchPolygonInfo
{
    wydb::ElementId sketchId;

    int sides = 0;

    // Ring order, counter-clockwise, starting at the vertex closest to +X:
    // lineIds[i] connects corners[i] -> corners[(i+1)%sides]
    std::vector<wydb::ElementId> lineIds;
    // Snapshot from recognition, only a fallback when the live read fails
    std::vector<wy::Vector2> corners;

    wy::Vector2 center;
    double circumRadius = 0.0;
    double inRadius = 0.0;
    // Angle of corners[0] relative to the center, radians
    double rotation = 0.0;
    // Edge whose midpoint carries the radius label, fixed at recognition
    int labelEdge = 0;

    double angleTol = 1e-4; // radians
    double distTol = 1e-6;  // model units
};

// Recognizes 3..100 lines (4 excluded: a square stays with the rectangle) as a
// regular polygon. The input may be unordered and every line may store either
// direction. Returns nullptr on failure (no exception/assert).
std::shared_ptr<const SketchPolygonInfo> tryDetectSketchPolygon(
    const std::vector<const wy3d::SketchLine*>& lines,
    double angleTol = 1e-4,
    double distTol = 1e-6);

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_RECOGNIZER_H
