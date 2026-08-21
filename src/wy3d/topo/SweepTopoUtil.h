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

#ifndef WY3D_SWEEP_TOPO_UTIL_H
#define WY3D_SWEEP_TOPO_UTIL_H

#include <vector>

#include <TopoDS_Shape.hxx>

#include <wy3dDefs.h>
#include <wyVector3.h>
#include <wy3dErrorCode.h>
#include <wy3dTopoNaming.h>

#include "topo/SketchTopoBuilder.h"

NS_WY3D_BEG

class Sketch;
class Curve;

// 扫掠体&扫掠曲面共用的路径Wire与管道构建（内部使用，不对外导出）
class SweepTopoUtil
{
public:
    // 草图路径 → 路径Wire + 起点位置/方向（Sweep与SweptSheet共用）
    static ErrorCode createPathWire(
        const wy3d::Sketch& pathSketch,
        TopoUtil::WireInfo& pathWireInfo,
        wy::Vector3& pathStartPos,
        wy::Vector3& pathStartDir);

    // 3D曲线路径 → 路径Wire + 起点位置/方向（Sweep与SweptSheet共用）
    static ErrorCode createPathWire(
        const wy3d::Curve& pathCurve,
        TopoUtil::WireInfo& pathWireInfo,
        wy::Vector3& pathStartPos,
        wy::Vector3& pathStartDir);

    // 单个轮廓沿路径的管道构建 + 拓扑命名
    // bMakeSolid = true → Sweep（固体）；false → SweptSheet（壳体）
    static ErrorCode makePipeShell(
        unsigned int elemIdValue,
        const TopoUtil::WireInfo& pathWireInfo,
        const TopoUtil::WireInfo& profileWireInfo,
        bool bMakeSolid,
        TopoDS_Shape& resultShape,
        TopoNaming& topoNaming);
};

NS_WY3D_END

#endif // WY3D_SWEEP_TOPO_UTIL_H
