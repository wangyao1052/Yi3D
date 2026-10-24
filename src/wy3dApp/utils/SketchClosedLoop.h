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

#ifndef WY3DAPP_SKETCH_CLOSED_LOOP_H
#define WY3DAPP_SKETCH_CLOSED_LOOP_H

#include <vector>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wydbTransaction.h>
#include <wy3dSketchLine.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// N lines joined end to end into one closed loop
struct SketchClosedLoop
{
    wydb::ElementId sketchId;

    // lineIds[i] connects corners[i] -> corners[(i+1)%n]
    std::vector<wydb::ElementId> lineIds;
    std::vector<wy::Vector2> corners;
    // True when the stored start of lineIds[i] is the end at corners[i]
    std::vector<bool> lineStartAtCorner;
};

// Orders the lines into one simple closed loop. The input may be unordered and
// every line may store either direction. Returns false for open ends, branches,
// duplicate lines or lines that do not share one sketch.
bool buildSketchClosedLoop(const std::vector<const wy3d::SketchLine*>& lines,
                           double distTol,
                           SketchClosedLoop& outLoop);

// Reads the live corners of a loop; lineIds must be in ring order.
bool readSketchClosedLoopCorners(const wydb::Database* pDb,
                                 const std::vector<wydb::ElementId>& lineIds,
                                 std::vector<wy::Vector2>& outCorners,
                                 std::vector<bool>* outLineStartAtCorner = nullptr,
                                 double joinTol = 1e-6);

// Writes all corners in one go; the caller owns the transaction and aborts it on failure.
bool writeSketchClosedLoopCorners(wydb::Transaction* pTrans,
                                  const std::vector<wydb::ElementId>& lineIds,
                                  const std::vector<bool>& lineStartAtCorner,
                                  const std::vector<wy::Vector2>& corners);

// Signed area around the center; positive for a counter-clockwise ring
double computeClosedLoopSignedArea(const std::vector<wy::Vector2>& corners,
                                   const wy::Vector2& center);

// Reverses the traversal direction while keeping corners[0] in place
void reverseClosedLoopRing(SketchClosedLoop& loop);

// Rotates the ring so that corners[startIndex] becomes corners[0]
void rotateClosedLoopStart(SketchClosedLoop& loop, int startIndex);

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_CLOSED_LOOP_H
