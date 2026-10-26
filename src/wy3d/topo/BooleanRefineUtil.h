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

#ifndef WY3D_BOOLEAN_REFINE_UTIL_H
#define WY3D_BOOLEAN_REFINE_UTIL_H

#include <wy3dDefs.h>

#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <TopoDS_Shape.hxx>

NS_WY3D_BEG

// Refine the result of a boolean operation with the ModelRefine library
// (a port of FreeCAD's ModelRefine, linked as a separate LGPL shared library).
// The refinement history is merged into the operation's history so that
// topological naming sees the refined sub-shapes.
// Returns the unrefined result on failure.
TopoDS_Shape refineBooleanResult(BRepAlgoAPI_BooleanOperation& booleanAlgo);

NS_WY3D_END

#endif // WY3D_BOOLEAN_REFINE_UTIL_H
