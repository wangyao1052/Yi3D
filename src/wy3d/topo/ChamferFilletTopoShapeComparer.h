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

#ifndef WY3D_CHAMFER_FILLET_TOPO_SHAPE_COMPARER_H
#define WY3D_CHAMFER_FILLET_TOPO_SHAPE_COMPARER_H

#include <wy3dDefs.h>
#include "topo/TopoShapeComparer.h"

NS_WY3D_BEG

// 拓扑元素比较器
class ChamferFilletTopoShapeComparer : public TopoShapeComparer
{
public:
    // Whether the host body is a solid or a sheet
    enum class HostType : std::int32_t
    {
        Solid = 0,
        Sheet = 1,
    };

    ChamferFilletTopoShapeComparer(BRepBuilderAPI_MakeShape& mkShape, const TopoDS_Shape& oldShape,
        HostType hostType);
    ~ChamferFilletTopoShapeComparer();

protected:
    // 初始化
    virtual void init() override;
    // 修改的
    virtual void recordModified() override;
    // 新增的
    virtual void recordAdded() override;

private:
    void initOldVertex2OldFaces();

private:
    // The host body kind - a sheet names its free edges after the new face
    HostType _hostType;

    // 点
    TopoShapeSet _oldVertexSet;

    // 旧的点 <> 旧的面
    std::unordered_map<TopoDS_Shape, TopoShapeSet, ShapeHasher, ShapeEqual>* _pOldVertex2OldFaces;
};

NS_WY3D_END

#endif // WY3D_CHAMFER_FILLET_TOPO_SHAPE_COMPARER_H