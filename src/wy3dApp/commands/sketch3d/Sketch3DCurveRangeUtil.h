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

#ifndef WY3DAPP_SKETCH3D_CURVE_RANGE_UTIL_H
#define WY3DAPP_SKETCH3D_CURVE_RANGE_UTIL_H

#include <wy3dDefs.h>

NS_WY3D_BEG
class SketchCurve3D;
NS_WY3D_END

// Leaves a 3D sketch curve covering a normalized parameter range and nothing else, extending it
// where the range runs past its own ends.
//
// The fillet and the chamfer both trim their two curves this way, and both read a parameter range
// the opposite way round from the trim and extend commands: (0.0, 1.0) here is the whole curve
// rather than nothing, and a parameter outside [0,1] is an extension rather than a refusal. That
// inversion is why neither command can call Sketch3DTrimGuiCmd's appliers.
class Sketch3DCurveRangeUtil
{
public:
    // False refuses the caller's whole operation, not just this curve: a range of nothing would
    // leave a degenerate curve behind, and a spline's range cannot leave its own domain at all.
    static bool apply(wy3d::SketchCurve3D* pCurve, double startParam, double endParam);
};

#endif // WY3DAPP_SKETCH3D_CURVE_RANGE_UTIL_H
