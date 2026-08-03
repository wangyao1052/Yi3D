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

#ifndef WY3DAPP_SKETCH_PROJECT_UTIL_H
#define WY3DAPP_SKETCH_PROJECT_UTIL_H

#include <TopoDS_Edge.hxx>
#include <wy3dSketchEntity.h>
#include <wy3dSketchPlane.h>

class SketchProjectUtil
{
public:
    enum class ProjectResult
    {
        Ok = 0,               // 成功
        Degenerate = 1,       // 退化为点
        NullCurve = 2,        // 无法获取曲线
        ProjectFailed = 3,    // 投影失败
        UnsupportedType = 4,  // 不支持的曲线类型
    };

private:
    // 投影实现（不含 try-catch）
    static ProjectResult projectEdgeImpl(
        wydb::Transaction* pTrans,
        const TopoDS_Edge& edge,
        const wy3d::SketchPlane& plane,
        wy3d::SketchEntity*& pOutEntity);

public:
    // 将一条BRep边投影到草图平面，生成对应的2D草图实体（带异常保护）
    static ProjectResult projectEdge(
        wydb::Transaction* pTrans,
        const TopoDS_Edge& edge,
        const wy3d::SketchPlane& plane,
        wy3d::SketchEntity*& pOutEntity);
};

#endif // WY3DAPP_SKETCH_PROJECT_UTIL_H
