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
#include <limits>
#include <memory>
#include <utility>

#include <wy3dImpl.h>
#include <wy3dSketch.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

constexpr int kLineCount = 4;
constexpr int kSlotCount = kLineCount * 2;

struct EndSlot
{
    wy::Vector2 point;
    int line = 0;
    bool isStart = false;
};

struct Cluster
{
    int size = 0;
    int firstLine = -1;
    wy::Vector2 sum;
};

int findRoot(std::array<int, kSlotCount>& parent, int index)
{
    while (parent[index] != index)
    {
        parent[index] = parent[parent[index]];
        index = parent[index];
    }
    return index;
}

void unite(std::array<int, kSlotCount>& parent, int a, int b)
{
    a = findRoot(parent, a);
    b = findRoot(parent, b);
    if (a != b) parent[b] = a;
}

} // namespace

static bool detectSketchRectangle(const std::vector<const wy3d::SketchLine*>& lines,
                                  SketchRectangleInfo& outInfo,
                                  double angleTol,
                                  double distTol)
{
    if (lines.size() != kLineCount) return false;
    if (angleTol <= 0.0 || distTol <= 0.0) return false;

    // 1. 前置校验：同一数据库、同一父草图
    const wydb::Database* pDb = nullptr;
    wydb::ElementId parentId;
    const double minLength = std::max(distTol, wy3d::kMinValue);
    for (int i = 0; i < kLineCount; ++i)
    {
        const wy3d::SketchLine* pLine = lines[i];
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

    // 2. 端点聚类（并查集）
    std::array<EndSlot, kSlotCount> slots;
    for (int i = 0; i < kLineCount; ++i)
    {
        slots[2 * i] = EndSlot{ lines[i]->getStartPoint(), i, true };
        slots[2 * i + 1] = EndSlot{ lines[i]->getEndPoint(), i, false };
    }

    std::array<int, kSlotCount> parent;
    for (int i = 0; i < kSlotCount; ++i) parent[i] = i;
    for (int a = 0; a < kSlotCount; ++a)
    {
        for (int b = a + 1; b < kSlotCount; ++b)
        {
            if ((slots[a].point - slots[b].point).length() <= distTol)
            {
                unite(parent, a, b);
            }
        }
    }

    std::array<int, kSlotCount> rootToCluster;
    rootToCluster.fill(-1);
    std::array<Cluster, kLineCount> clusters;
    std::array<int, kSlotCount> slotCluster;
    slotCluster.fill(-1);
    int clusterCount = 0;
    for (int i = 0; i < kSlotCount; ++i)
    {
        const int root = findRoot(parent, i);
        int cluster = rootToCluster[root];
        if (cluster < 0)
        {
            if (clusterCount >= kLineCount) return false;
            cluster = clusterCount;
            rootToCluster[root] = cluster;
            ++clusterCount;
        }

        Cluster& clusterData = clusters[cluster];
        if (clusterData.size >= 2) return false;
        if (clusterData.size == 1 && clusterData.firstLine == slots[i].line) return false;
        if (clusterData.size == 0) clusterData.firstLine = slots[i].line;
        ++clusterData.size;
        clusterData.sum += slots[i].point;
        slotCluster[i] = cluster;
    }

    // 4 个顶点，每个顶点恰好由两条不同线的端点组成
    if (clusterCount != kLineCount) return false;
    for (int i = 0; i < clusterCount; ++i)
    {
        if (clusters[i].size != 2) return false;
    }

    std::array<int, kLineCount> startCluster{};
    std::array<int, kLineCount> endCluster{};
    for (int i = 0; i < kLineCount; ++i)
    {
        startCluster[i] = slotCluster[2 * i];
        endCluster[i] = slotCluster[2 * i + 1];
        if (startCluster[i] < 0 || endCluster[i] < 0) return false;
        if (startCluster[i] == endCluster[i]) return false;
    }

    // 3. 从 line0 出发走环，要求 4 条线全部使用且回到起点
    std::array<bool, kLineCount> used{};
    used.fill(false);
    std::array<int, kLineCount> cycleLines{};
    std::array<int, kLineCount> cycleVerts{};
    int curLine = 0;
    int curVert = startCluster[0];
    cycleVerts[0] = curVert;
    for (int k = 0; k < kLineCount; ++k)
    {
        if (used[curLine]) return false;
        used[curLine] = true;
        cycleLines[k] = curLine;

        const int nextVert = (startCluster[curLine] == curVert)
            ? endCluster[curLine] : startCluster[curLine];
        if (k == kLineCount - 1)
        {
            if (nextVert != cycleVerts[0]) return false;
        }
        else
        {
            cycleVerts[k + 1] = nextVert;

            int nextLine = -1;
            for (int j = 0; j < kLineCount; ++j)
            {
                if (used[j]) continue;
                if (startCluster[j] == nextVert || endCluster[j] == nextVert)
                {
                    if (nextLine >= 0) return false; // 分叉/自交
                    nextLine = j;
                }
            }
            if (nextLine < 0) return false;
            curLine = nextLine;
            curVert = nextVert;
        }
    }

    // 4 个顶点互不相同
    for (int i = 0; i < kLineCount; ++i)
    {
        for (int j = i + 1; j < kLineCount; ++j)
        {
            if (cycleVerts[i] == cycleVerts[j]) return false;
        }
    }

    // 4. 角点取聚类内两个端点的平均
    std::array<wy::Vector2, kLineCount> corners{};
    for (int k = 0; k < kLineCount; ++k)
    {
        corners[k] = clusters[cycleVerts[k]].sum * 0.5;
    }

    // 5. 矩形校验
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

    // 相邻边转向方向一致，排除 bowtie/折叠
    const double firstCross = units[0].cross(units[1]);
    if (std::fabs(firstCross) <= wy::EPS) return false;
    for (int k = 0; k < kLineCount; ++k)
    {
        if (units[k].cross(units[(k + 1) % kLineCount]) * firstCross <= 0.0) return false;
    }

    // 对边等长（容差随角度误差放宽）
    for (int k = 0; k < 2; ++k)
    {
        const double maxLength = std::max(lengths[k], lengths[k + 2]);
        const double lengthTol = std::max(distTol, 2.0 * sinTol * maxLength);
        if (std::fabs(lengths[k] - lengths[k + 2]) > lengthTol) return false;
    }

    // 6. 正交 frame（v 指向 c0 -> c3）
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

    // 7. 填充结果
    outInfo = SketchRectangleInfo();
    outInfo.sketchId = pSketch->getId();
    for (int k = 0; k < kLineCount; ++k)
    {
        outInfo.lineIds[k] = lines[cycleLines[k]]->getId();
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
    if (!pDb || joinTol <= 0.0) return false;

    // 每条线的起点/终点只读一次：ends[2*i] 起点，ends[2*i+1] 终点
    std::array<wy::Vector2, 8> ends{};
    for (int i = 0; i < 4; ++i)
    {
        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pDb->getElement(lineIds[i]));
        if (!pLine) return false;
        ends[2 * i] = pLine->getStartPoint();
        ends[2 * i + 1] = pLine->getEndPoint();
    }

    // atCorner[i]: lineIds[i] 在 corners[i] 处使用的是起点(0)还是终点(1)
    std::array<int, 4> atCorner{};
    const double tol2 = joinTol * joinTol;

    // corner1 由 line0 与 line1 的端点配对（方向未知，比较 4 种组合）
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

        outCorners[1] = (ends[bestA] + ends[2 + bestB]) * 0.5;
        atCorner[1] = bestB;
        atCorner[0] = 1 - bestA;
    }

    // corner2、corner3：上一条线在上一角点的另一端就是下一角点
    for (int i = 1; i < 3; ++i)
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

    // corner0：line3 的另一端与 line0 在 corner0 的端点
    {
        const wy::Vector2 end3 = ends[2 * 3 + (1 - atCorner[3])];
        const wy::Vector2 end0 = ends[atCorner[0]];
        if ((end3 - end0).length2() > tol2) return false;
        outCorners[0] = (end3 + end0) * 0.5;
    }

    if (outLineStartAtCorner)
    {
        for (int i = 0; i < 4; ++i) (*outLineStartAtCorner)[i] = (atCorner[i] == 0);
    }
    return true;
}

} // namespace wy3dApp
