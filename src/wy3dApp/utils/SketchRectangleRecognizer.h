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

#ifndef WY3DAPP_SKETCH_RECTANGLE_RECOGNIZER_H
#define WY3DAPP_SKETCH_RECTANGLE_RECOGNIZER_H

#include <array>
#include <memory>
#include <vector>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wy3dSketchLine.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 4 条首尾相接直线构成的矩形信息
struct SketchRectangleInfo
{
    wydb::ElementId sketchId;

    // 环序: lineIds[i] 连接 corners[i] -> corners[(i+1)%4]
    std::array<wydb::ElementId, 4> lineIds{};
    // 识别时的角点快照，仅作实时读取失败时的 fallback
    std::array<wy::Vector2, 4> corners{};

    // 规范参数化: c0 = center - halfWidth*u - halfHeight*v
    wy::Vector2 center;
    wy::Vector2 u = wy::Vector2::kXAxis; // 单位向量，沿 c0 -> c1
    wy::Vector2 v = wy::Vector2::kYAxis; // 单位向量，沿 c0 -> c3，且 u⊥v
    double halfWidth = 0.0;
    double halfHeight = 0.0;

    double angleTol = 1e-4; // 弧度
    double distTol = 1e-6;  // 模型单位
};

// 识别 4 条直线是否构成矩形。输入允许乱序、方向任意；失败返回 nullptr（不抛异常/断言）。
std::shared_ptr<const SketchRectangleInfo> tryDetectSketchRectangle(
    const std::vector<const wy3d::SketchLine*>& lines,
    double angleTol = 1e-4,
    double distTol = 1e-6);

// 从实时数据库读取 4 条线并还原角点。
// lineIds 必须按环序排列：lineIds[i] 连接 corners[i] -> corners[(i+1)%4]
// （即 tryDetectSketchRectangle 输出的顺序）。
// outLineStartAtCorner[i] 表示 lineIds[i] 的存储起点是否对应 corners[i]。
bool readSketchRectangleCorners(const wydb::Database* pDb,
                                const std::array<wydb::ElementId, 4>& lineIds,
                                std::array<wy::Vector2, 4>& outCorners,
                                std::array<bool, 4>* outLineStartAtCorner = nullptr,
                                double joinTol = 1e-6);

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_RECOGNIZER_H
