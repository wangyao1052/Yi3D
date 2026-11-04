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

#ifndef WY3DAPP_UTILS_SKETCH_SLOT_GEOMETRY_H
#define WY3DAPP_UTILS_SKETCH_SLOT_GEOMETRY_H

namespace wy3dApp {

// A point in sketch UV space.
struct SlotPoint
{
    double x = 0.0;
    double y = 0.0;
};

// A counter-clockwise circular arc in sketch UV space.
struct SlotArc
{
    SlotPoint center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
};

// Geometry of a straight slot ("obround"):
//
//        upperStart            upperEnd
//      B + n*r  ------------>  A + n*r
//        /                            \
//   B - n*r  <--- lower line ----  A - n*r
//
// The four segments are ordered so that every segment's end coincides with the
// next segment's start:
//   rightArc (B - n*r -> B + n*r), upper line, leftArc (A + n*r -> A - n*r),
//   lower line.
// Both arcs are counter-clockwise, as required by wy3d::SketchArc.
struct SlotGeometry
{
    SlotArc rightArc;
    SlotPoint upperStart;
    SlotPoint upperEnd;
    SlotArc leftArc;
    SlotPoint lowerStart;
    SlotPoint lowerEnd;

    // Computes the slot between two center points. width is the full width of
    // the slot (twice the end radius). Returns false for degenerate input.
    static bool compute(
        const SlotPoint& firstCenter,
        const SlotPoint& secondCenter,
        double width,
        SlotGeometry& out,
        double tolerance = 1.0e-9);
};

} // namespace wy3dApp

#endif // WY3DAPP_UTILS_SKETCH_SLOT_GEOMETRY_H
