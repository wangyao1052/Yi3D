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

#ifndef WY3DAPP_SKETCH_SNAP_TYPE_H
#define WY3DAPP_SKETCH_SNAP_TYPE_H

namespace wy3dApp {

// 草图捕捉类型
enum class SketchSnapType
{
    Undefined = 0,
    // 水平
    Horizontal,
    // 竖直
    Vertical,
    // 平行
    Parallel,
    // 垂直
    Perpendicular,
    // X 对齐
    AlignX,
    // Y 对齐
    AlignY,
    // 相等
    Equal,
    // 点在线上
    PointOnCurve,
    // 垂足
    PerpendicularPoint,
    // 切点
    TangetPoint,
    // 交点
    IntersectionPoint,
    // 关键点
    KeyPoint,
    // 外部:端点
    EndPoint,
    // 外部:中点
    MiddlePoint,
    // 外部:中心点
    CenterPoint,
    // 坐标原点
    Origin,
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_SNAP_TYPE_H
