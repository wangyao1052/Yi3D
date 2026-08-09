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

#include "utils/RevolutionUtil.h"

#include <cassert>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCurve.h>

NS_WY3D_BEG

std::pair<ErrorCode, gp_Ax1> computeRevolutionAxis(
    const wydb::Database* pDb,
    const wydb::ElementId& axisCurveId)
{
    if (axisCurveId.isNull())
        return { ErrorCode::REVOLUTION_UnspecifiedAxisLine, gp_Ax1() };

    const wy3d::SketchCurve* pAxisCurve = wy3d::SketchCurve::cast(pDb->getElement(axisCurveId));
    if (!pAxisCurve)
        return { ErrorCode::REVOLUTION_InvalidRevolutionAxisLine, gp_Ax1() };

    if (!wy3d::SketchLine::cast(pAxisCurve) && !wy3d::SketchCenterLine::cast(pAxisCurve))
        return { ErrorCode::REVOLUTION_InvalidRevolutionAxisLine, gp_Ax1() };

    const wy3d::Sketch* pAxisSketch = wy3d::Sketch::cast(pDb->getElement(pAxisCurve->getParent()));
    if (!pAxisSketch)
        return { ErrorCode::REVOLUTION_InvalidRevolutionAxisLine, gp_Ax1() };

    const wy3d::SketchPlane& axisSketchPlane = pAxisSketch->getPlane();
    if (!axisSketchPlane.isValid())
        return { ErrorCode::REVOLUTION_InvalidRevolutionAxisLine, gp_Ax1() };

    wy::Vector3 axisStartPnt = axisSketchPlane.value(pAxisCurve->getStartPoint());
    wy::Vector3 axisEndPnt = axisSketchPlane.value(pAxisCurve->getEndPoint());
    wy::Vector3 axisDir = axisEndPnt - axisStartPnt;
    if (axisDir.length() <= wy3d::EPS)
        return { ErrorCode::REVOLUTION_InvalidRevolutionAxisLine, gp_Ax1() };

    axisDir.normalize();
    gp_Ax1 axis(gp_Pnt(axisStartPnt.x(), axisStartPnt.y(), axisStartPnt.z()),
                gp_Dir(axisDir.x(), axisDir.y(), axisDir.z()));
    return { ErrorCode::NoError, axis };
}

NS_WY3D_END
