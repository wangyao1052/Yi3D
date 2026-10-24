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

#include "utils/SketchClosedLoop.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <wy3dImpl.h>
#include <wy3dSketch.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

int findRoot(std::vector<int>& parent, int index)
{
    while (parent[index] != index)
    {
        parent[index] = parent[parent[index]];
        index = parent[index];
    }
    return index;
}

void unite(std::vector<int>& parent, int a, int b)
{
    a = findRoot(parent, a);
    b = findRoot(parent, b);
    if (a != b) parent[b] = a;
}

} // namespace

bool buildSketchClosedLoop(const std::vector<const wy3d::SketchLine*>& lines,
                           double distTol,
                           SketchClosedLoop& outLoop)
{
    const int lineCount = static_cast<int>(lines.size());
    if (lineCount < 3 || distTol <= 0.0) return false;

    // Every line must live in the same database, sketch and plane
    const wydb::Database* pDb = nullptr;
    wydb::ElementId parentId;
    const double minLength = std::max(distTol, wy3d::kMinValue);
    for (const wy3d::SketchLine* pLine : lines)
    {
        if (!pLine) return false;
        if (pLine->getLength() < minLength) return false;

        const wydb::Database* pLineDb = pLine->getDatabase();
        if (!pLineDb) return false;
        if (!pDb)
        {
            pDb = pLineDb;
            parentId = pLine->getParent();
        }
        else if (pDb != pLineDb || pLine->getParent() != parentId)
        {
            return false;
        }
    }
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(parentId));
    if (!pSketch || !pSketch->getPlane().isValid()) return false;

    // Cluster the 2N endpoints; a valid loop has exactly N clusters of two
    // endpoints belonging to two different lines
    const int slotCount = 2 * lineCount;
    std::vector<wy::Vector2> slotPoints(slotCount);
    std::vector<int> slotLine(slotCount);
    for (int i = 0; i < lineCount; ++i)
    {
        slotPoints[2 * i] = lines[i]->getStartPoint();
        slotPoints[2 * i + 1] = lines[i]->getEndPoint();
        slotLine[2 * i] = i;
        slotLine[2 * i + 1] = i;
    }

    std::vector<int> parent(slotCount);
    for (int i = 0; i < slotCount; ++i) parent[i] = i;
    for (int a = 0; a < slotCount; ++a)
    {
        for (int b = a + 1; b < slotCount; ++b)
        {
            if ((slotPoints[a] - slotPoints[b]).length() <= distTol)
            {
                unite(parent, a, b);
            }
        }
    }

    std::vector<int> rootToCluster(slotCount, -1);
    std::vector<int> clusterSize;
    std::vector<int> clusterFirstLine;
    std::vector<wy::Vector2> clusterSum;
    std::vector<int> slotCluster(slotCount, -1);
    int clusterCount = 0;
    for (int i = 0; i < slotCount; ++i)
    {
        const int root = findRoot(parent, i);
        int cluster = rootToCluster[root];
        if (cluster < 0)
        {
            if (clusterCount >= lineCount) return false;
            cluster = clusterCount;
            rootToCluster[root] = cluster;
            ++clusterCount;
            clusterSize.push_back(0);
            clusterFirstLine.push_back(-1);
            clusterSum.push_back(wy::Vector2());
        }

        if (clusterSize[cluster] >= 2) return false;
        if (1 == clusterSize[cluster] && clusterFirstLine[cluster] == slotLine[i]) return false;
        if (0 == clusterSize[cluster]) clusterFirstLine[cluster] = slotLine[i];
        ++clusterSize[cluster];
        clusterSum[cluster] += slotPoints[i];
        slotCluster[i] = cluster;
    }

    if (clusterCount != lineCount) return false;
    for (int i = 0; i < clusterCount; ++i)
    {
        if (2 != clusterSize[i]) return false;
    }

    std::vector<int> startCluster(lineCount);
    std::vector<int> endCluster(lineCount);
    for (int i = 0; i < lineCount; ++i)
    {
        startCluster[i] = slotCluster[2 * i];
        endCluster[i] = slotCluster[2 * i + 1];
        if (startCluster[i] < 0 || endCluster[i] < 0) return false;
        if (startCluster[i] == endCluster[i]) return false;
    }

    // Walk the loop from line 0; every vertex must have exactly one unused
    // incident edge and all lines must be used before returning to the start
    std::vector<bool> used(lineCount, false);
    std::vector<int> cycleLines(lineCount);
    std::vector<int> cycleVerts(lineCount);
    int curLine = 0;
    int curVert = startCluster[0];
    cycleVerts[0] = curVert;
    for (int k = 0; k < lineCount; ++k)
    {
        if (used[curLine]) return false;
        used[curLine] = true;
        cycleLines[k] = curLine;

        const int nextVert = (startCluster[curLine] == curVert)
            ? endCluster[curLine] : startCluster[curLine];
        if (k == lineCount - 1)
        {
            if (nextVert != cycleVerts[0]) return false;
        }
        else
        {
            cycleVerts[k + 1] = nextVert;

            int nextLine = -1;
            for (int j = 0; j < lineCount; ++j)
            {
                if (used[j]) continue;
                if (startCluster[j] == nextVert || endCluster[j] == nextVert)
                {
                    if (nextLine >= 0) return false; // branch or self-intersection
                    nextLine = j;
                }
            }
            if (nextLine < 0) return false;
            curLine = nextLine;
            curVert = nextVert;
        }
    }

    for (int i = 0; i < lineCount; ++i)
    {
        for (int j = i + 1; j < lineCount; ++j)
        {
            if (cycleVerts[i] == cycleVerts[j]) return false;
        }
    }

    outLoop = SketchClosedLoop();
    outLoop.sketchId = pSketch->getId();
    outLoop.lineIds.resize(lineCount);
    outLoop.corners.resize(lineCount);
    outLoop.lineStartAtCorner.resize(lineCount);
    for (int k = 0; k < lineCount; ++k)
    {
        const wy3d::SketchLine* pLine = lines[cycleLines[k]];
        outLoop.lineIds[k] = pLine->getId();
        outLoop.corners[k] = clusterSum[cycleVerts[k]] * 0.5;

        const double startDistance2 = (pLine->getStartPoint() - outLoop.corners[k]).length2();
        const double endDistance2 = (pLine->getEndPoint() - outLoop.corners[k]).length2();
        outLoop.lineStartAtCorner[k] = startDistance2 <= endDistance2;
    }
    return true;
}

bool readSketchClosedLoopCorners(const wydb::Database* pDb,
                                 const std::vector<wydb::ElementId>& lineIds,
                                 std::vector<wy::Vector2>& outCorners,
                                 std::vector<bool>* outLineStartAtCorner,
                                 double joinTol)
{
    const int lineCount = static_cast<int>(lineIds.size());
    if (!pDb || lineCount < 3 || joinTol <= 0.0) return false;

    // ends[2*i] is the stored start of line i, ends[2*i+1] its end
    std::vector<wy::Vector2> ends(2 * lineCount);
    for (int i = 0; i < lineCount; ++i)
    {
        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pDb->getElement(lineIds[i]));
        if (!pLine) return false;
        ends[2 * i] = pLine->getStartPoint();
        ends[2 * i + 1] = pLine->getEndPoint();
    }

    // atCorner[i] is 0/1 when the stored start/end of line i sits at corners[i]
    std::vector<int> atCorner(lineCount, 0);
    const double tol2 = joinTol * joinTol;

    // Corner 1 pairs the endpoints of lines 0 and 1 (storage direction unknown)
    {
        double bestDistance2 = std::numeric_limits<double>::max();
        int bestA = 0;
        int bestB = 0;
        for (int a = 0; a < 2; ++a)
        {
            for (int b = 0; b < 2; ++b)
            {
                const double distance2 = (ends[a] - ends[2 + b]).length2();
                if (distance2 < bestDistance2)
                {
                    bestDistance2 = distance2;
                    bestA = a;
                    bestB = b;
                }
            }
        }
        if (bestDistance2 > tol2) return false;

        outCorners.assign(lineCount, wy::Vector2());
        outCorners[1] = (ends[bestA] + ends[2 + bestB]) * 0.5;
        atCorner[1] = bestB;
        atCorner[0] = 1 - bestA;
    }

    // Remaining corners: the free end of the previous line meets the next line
    for (int i = 1; i < lineCount - 1; ++i)
    {
        const wy::Vector2 joint = ends[2 * i + (1 - atCorner[i])];
        double bestDistance2 = std::numeric_limits<double>::max();
        int bestB = 0;
        for (int b = 0; b < 2; ++b)
        {
            const double distance2 = (joint - ends[2 * (i + 1) + b]).length2();
            if (distance2 < bestDistance2)
            {
                bestDistance2 = distance2;
                bestB = b;
            }
        }
        if (bestDistance2 > tol2) return false;

        outCorners[i + 1] = (joint + ends[2 * (i + 1) + bestB]) * 0.5;
        atCorner[i + 1] = bestB;
    }

    // The last line must close the loop at corner 0
    {
        const wy::Vector2 lastEnd = ends[2 * (lineCount - 1) + (1 - atCorner[lineCount - 1])];
        const wy::Vector2 firstEnd = ends[atCorner[0]];
        if ((lastEnd - firstEnd).length2() > tol2) return false;
        outCorners[0] = (lastEnd + firstEnd) * 0.5;
    }

    if (outLineStartAtCorner)
    {
        outLineStartAtCorner->resize(lineCount);
        for (int i = 0; i < lineCount; ++i)
        {
            (*outLineStartAtCorner)[i] = (0 == atCorner[i]);
        }
    }
    return true;
}

bool writeSketchClosedLoopCorners(wydb::Transaction* pTrans,
                                  const std::vector<wydb::ElementId>& lineIds,
                                  const std::vector<bool>& lineStartAtCorner,
                                  const std::vector<wy::Vector2>& corners)
{
    const int lineCount = static_cast<int>(lineIds.size());
    if (!pTrans || lineCount < 3) return false;
    if (static_cast<int>(corners.size()) != lineCount) return false;
    if (static_cast<int>(lineStartAtCorner.size()) != lineCount) return false;

    // Fetch every writable line first; the caller rolls back on any failure
    std::vector<wy3d::SketchLine*> pLines(lineCount, nullptr);
    for (int i = 0; i < lineCount; ++i)
    {
        pLines[i] = wy3d::SketchLine::cast(pTrans->getElementForWrite(lineIds[i]));
        if (!pLines[i]) return false;
    }

    for (int i = 0; i < lineCount; ++i)
    {
        const wy::Vector2& corner = corners[i];
        const wy::Vector2& nextCorner = corners[(i + 1) % lineCount];
        const bool startAtCorner = lineStartAtCorner[i];

        const wy::ErrorStatus startError = startAtCorner
            ? pLines[i]->setStartPoint(corner) : pLines[i]->setStartPoint(nextCorner);
        const wy::ErrorStatus endError = startAtCorner
            ? pLines[i]->setEndPoint(nextCorner) : pLines[i]->setEndPoint(corner);
        if (wy::ErrorStatus::Ok != startError || wy::ErrorStatus::Ok != endError)
        {
            return false;
        }
    }
    return true;
}

double computeClosedLoopSignedArea(const std::vector<wy::Vector2>& corners,
                                   const wy::Vector2& center)
{
    const int lineCount = static_cast<int>(corners.size());
    if (lineCount < 3) return 0.0;

    double area2 = 0.0;
    for (int i = 0; i < lineCount; ++i)
    {
        const wy::Vector2 current = corners[i] - center;
        const wy::Vector2 next = corners[(i + 1) % lineCount] - center;
        area2 += current.cross(next);
    }
    return 0.5 * area2;
}

void reverseClosedLoopRing(SketchClosedLoop& loop)
{
    const int lineCount = static_cast<int>(loop.corners.size());
    if (lineCount < 2) return;

    std::vector<wydb::ElementId> lineIds(lineCount);
    std::vector<wy::Vector2> corners(lineCount);
    std::vector<bool> lineStartAtCorner(lineCount);
    for (int i = 0; i < lineCount; ++i)
    {
        const int lineIndex = (lineCount - 1 - i) % lineCount;
        lineIds[i] = loop.lineIds[lineIndex];
        corners[i] = loop.corners[(lineCount - i) % lineCount];
        lineStartAtCorner[i] = !loop.lineStartAtCorner[lineIndex];
    }

    loop.lineIds = std::move(lineIds);
    loop.corners = std::move(corners);
    loop.lineStartAtCorner = std::move(lineStartAtCorner);
}

void rotateClosedLoopStart(SketchClosedLoop& loop, int startIndex)
{
    const int lineCount = static_cast<int>(loop.corners.size());
    if (lineCount < 2) return;

    int offset = startIndex % lineCount;
    if (offset < 0) offset += lineCount;
    if (0 == offset) return;

    std::rotate(loop.lineIds.begin(), loop.lineIds.begin() + offset, loop.lineIds.end());
    std::rotate(loop.corners.begin(), loop.corners.begin() + offset, loop.corners.end());
    std::rotate(loop.lineStartAtCorner.begin(), loop.lineStartAtCorner.begin() + offset,
                loop.lineStartAtCorner.end());
}

} // namespace wy3dApp
