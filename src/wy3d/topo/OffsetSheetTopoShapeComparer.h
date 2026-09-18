///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3D_OFFSET_SHEET_TOPO_SHAPE_COMPARER_H
#define WY3D_OFFSET_SHEET_TOPO_SHAPE_COMPARER_H

#include <memory>
#include <vector>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <wy3dDefs.h>
#include "topo/TopoShapeComparer.h"

NS_WY3D_BEG

class OffsetSheetTopoShapeComparer : public TopoShapeComparer
{
public:
    OffsetSheetTopoShapeComparer(
        const std::vector<std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>>& offsetAlgos,
        const TopoDS_Shape& oldShape,
        const TopoDS_Shape& newShape);

protected:
    virtual void recordAdded() override;

private:
    std::vector<std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>> _offsetAlgos;
};

NS_WY3D_END

#endif // WY3D_OFFSET_SHEET_TOPO_SHAPE_COMPARER_H
