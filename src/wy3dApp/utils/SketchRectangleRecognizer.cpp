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

#include "utils/SketchRectangleRecognizer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <wy3dImpl.h>
#include <wy3dSketch.h>

#include "utils/SketchClosedLoop.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

constexpr int kLineCount = 4;

} // namespace

static bool detectSketchRectangle(const std::vector<const wy3d::SketchLine*>& lines,
                                  SketchRectangleInfo& outInfo,
                                  double angleTol,
                                  double distTol)
{
    if (lines.size() != kLineCount) return false;
    if (angleTol <= 0.0 || distTol <= 0.0) return false;

    // 1. Cluster the endpoints and walk the ring (same sketch, minimum length,
    // no branch, distinct vertices)
    SketchClosedLoop loop;
    if (!buildSketchClosedLoop(lines, distTol, loop)) return false;

    // 2. Each corner is the midpoint of its two clustered endpoints
    std::array<wy::Vector2, kLineCount> corners{};
    for (int k = 0; k < kLineCount; ++k)
    {
        corners[k] = loop.corners[k];
    }

    // 3. Rectangle checks
    std::array<wy::Vector2, kLineCount> edges{};
    std::array<double, kLineCount> lengths{};
    std::array<wy::Vector2, kLineCount> units{};
    for (int k = 0; k < kLineCount; ++k)
    {
        edges[k] = corners[(k + 1) % kLineCount] - corners[k];
        lengths[k] = edges[k].length();
        if (lengths[k] < wy3d::kMinValue) return false;
        units[k] = edges[k] / lengths[k];
    }

    const double sinTol = std::sin(angleTol);
    for (int k = 0; k < kLineCount; ++k)
    {
        if (std::fabs(units[k].dot(units[(k + 1) % kLineCount])) > sinTol) return false;
    }

    // Consecutive turns must share one sign, rejecting bowties/folds
    const double firstCross = units[0].cross(units[1]);
    if (std::fabs(firstCross) <= wy::EPS) return false;
    for (int k = 0; k < kLineCount; ++k)
    {
        if (units[k].cross(units[(k + 1) % kLineCount]) * firstCross <= 0.0) return false;
    }

    // Opposite sides are equal (tolerance widens with the angular error)
    for (int k = 0; k < 2; ++k)
    {
        const double maxLength = std::max(lengths[k], lengths[k + 2]);
        const double lengthTol = std::max(distTol, 2.0 * sinTol * maxLength);
        if (std::fabs(lengths[k] - lengths[k + 2]) > lengthTol) return false;
    }

    // 4. Orthonormal frame (v points from c0 to c3)
    const wy::Vector2 u = units[0];
    wy::Vector2 v(-u.y(), u.x());
    const wy::Vector2 heightDir = corners[3] - corners[0];
    if (v.dot(heightDir) < 0.0) v = -v;

    const double halfWidth = lengths[0] * 0.5;
    const double halfHeight = lengths[3] * 0.5;
    if (halfWidth < 0.5 * wy3d::kMinValue || halfHeight < 0.5 * wy3d::kMinValue)
    {
        return false;
    }

    // 5. Fill the result
    outInfo = SketchRectangleInfo();
    outInfo.sketchId = loop.sketchId;
    for (int k = 0; k < kLineCount; ++k)
    {
        outInfo.lineIds[k] = loop.lineIds[k];
        outInfo.corners[k] = corners[k];
    }
    outInfo.center = corners[0] + halfWidth * u + halfHeight * v;
    outInfo.u = u;
    outInfo.v = v;
    outInfo.halfWidth = halfWidth;
    outInfo.halfHeight = halfHeight;
    outInfo.angleTol = angleTol;
    outInfo.distTol = distTol;
    return true;
}

std::shared_ptr<const SketchRectangleInfo> tryDetectSketchRectangle(
    const std::vector<const wy3d::SketchLine*>& lines,
    double angleTol,
    double distTol)
{
    SketchRectangleInfo info;
    if (!detectSketchRectangle(lines, info, angleTol, distTol)) return nullptr;
    return std::make_shared<const SketchRectangleInfo>(std::move(info));
}

bool readSketchRectangleCorners(const wydb::Database* pDb,
                                const std::array<wydb::ElementId, 4>& lineIds,
                                std::array<wy::Vector2, 4>& outCorners,
                                std::array<bool, 4>* outLineStartAtCorner,
                                double joinTol)
{
    const std::vector<wydb::ElementId> ids(lineIds.begin(), lineIds.end());
    std::vector<wy::Vector2> corners;
    std::vector<bool> lineStartAtCorner;
    if (!readSketchClosedLoopCorners(pDb, ids, corners,
                                     outLineStartAtCorner ? &lineStartAtCorner : nullptr, joinTol))
    {
        return false;
    }

    for (int i = 0; i < kLineCount; ++i)
    {
        outCorners[i] = corners[i];
        if (outLineStartAtCorner) (*outLineStartAtCorner)[i] = lineStartAtCorner[i];
    }
    return true;
}

} // namespace wy3dApp
