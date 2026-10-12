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

#ifndef WY3DAPP_SPLINE_UTIL_H
#define WY3DAPP_SPLINE_UTIL_H

#include <wyVector2.h>
#include <wy3dPlane.h>
#include <wy3dSketchSpline.h>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SplineUtil
{
public:
	// 截取样条曲线的一段
	// 输出:
	// degree --- 次数
	// controlPoints --- 控制点
	// knots --- 节点
	// multiplicities --- 重数
	static bool segment(const wy3d::SketchSpline* pSketchSpline, double startParam, double endParam,
		unsigned int& degree,
		std::vector<wy::Vector2>& controlPoints,
		std::vector<double>& knots,
		std::vector<unsigned int>& multiplicities);

    // 在样条曲线的起点或者终点处添加一直线段
    static Handle(Geom2d_BSplineCurve) addLineSegmentToBSpline(
        const Handle(Geom2d_BSplineCurve)& origCurve,
        const wy::Vector2& newPoint,
        const bool atStart);

    // 提取样条曲线数据
    static bool getBSplineData(const Handle(Geom2d_BSplineCurve)& pBSpline,
        unsigned int& degree,
        std::vector<wy::Vector2>& controlPoints,
        std::vector<double>& knots,
        std::vector<unsigned int>& multiplicities);

    // 2D样条曲线转3D样条曲线
    static Handle(Geom_BSplineCurve) convertToBSplineCurve3D(
        const Handle(Geom2d_BSplineCurve)& pBSpline2d,
        const wy3d::Plane& plane);
};

}

#endif // WY3DAPP_SPLINE_UTIL_H
