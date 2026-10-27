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

#include "topo/BooleanRefineUtil.h"

#include <BRepTools_History.hxx>

#include <modelrefine/refine.h>

NS_WY3D_BEG

TopoDS_Shape refineBooleanResult(BRepAlgoAPI_BooleanOperation& booleanAlgo)
{
    const TopoDS_Shape rawShape = booleanAlgo.Shape();
    if (rawShape.IsNull())
    {
        return rawShape;
    }

    TopoDS_Shape refinedShape;
    Handle(BRepTools_History) modelRefineHistory;
    if (!modelrefine::refine(rawShape, refinedShape, modelRefineHistory))
    {
        return rawShape;
    }

    // Compose the boolean history with the raw-to-refined history, so that topological naming
    // maps old sub-shapes directly to the refined ones.
    Handle(BRepTools_History) history = booleanAlgo.History();
    if (!history.IsNull())
    {
        history->Merge(modelRefineHistory);
    }

    return refinedShape;
}

NS_WY3D_END
