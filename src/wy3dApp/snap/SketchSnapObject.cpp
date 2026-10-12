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

#include "SketchSnapObject.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

ParallelSnapObject::ParallelSnapObject(
    const wydb::ElementId& id,
    const wy::Vector2& startPnt,
    const wy::Vector2& endPnt)
    : AngleSnapObject(id)
{
    _angle = computeParallelAngle(startPnt, endPnt);
}

PerpendicularSnapObject::PerpendicularSnapObject(
    const wydb::ElementId& id,
    const wy::Vector2& startPnt,
    const wy::Vector2& endPnt)
    : AngleSnapObject(id)
{
    _angle = computePerpendicularAngle(startPnt, endPnt);
}

} // namespace wy3dApp
