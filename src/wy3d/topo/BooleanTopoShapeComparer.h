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

#ifndef WY3D_BOOLEAN_TOPO_SHAPE_COMPARER_H
#define WY3D_BOOLEAN_TOPO_SHAPE_COMPARER_H

#include <wy3dDefs.h>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include "topo/TopoShapeComparer.h"

NS_WY3D_BEG

// 布尔运算的拓扑元素比较器
class BooleanTopoShapeComparer : public TopoShapeComparer
{
public:
    explicit BooleanTopoShapeComparer(
        BRepAlgoAPI_BooleanOperation& booleanAlgo,
        const TopoDS_Shape& newShape = TopoDS_Shape());
    ~BooleanTopoShapeComparer();

protected:
    // 初始化
    virtual void init() override;
    // 修改的
    virtual void recordModified() override;
    // 新增的
    virtual void recordAdded() override;

private:
    BRepAlgoAPI_BooleanOperation& _booleanAlgo;

    // 目标体的面和参与体的面
    TopoShapeSet _oldFaceSetTarget;
    TopoShapeSet _oldFaceSetTool;
    // 目标体的边和参与体的边
    TopoShapeSet _oldEdgeSetTarget;
    TopoShapeSet _oldEdgeSetTool;
};

NS_WY3D_END

#endif // WY3D_BOOLEAN_TOPO_SHAPE_COMPARER_H