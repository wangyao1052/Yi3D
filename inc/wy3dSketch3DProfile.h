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

#ifndef WY3D_SKETCH3D_PROFILE_H
#define WY3D_SKETCH3D_PROFILE_H

#include <vector>
#include <memory>

#include <wy3dDefs.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurveGraph.h>

NS_WY3D_BEG

class SketchCurve3D;

// A curve together with its orientation along the loop
struct BiCurve3D
{
    BiCurve3D(const SketchCurve3D* pCurve, bool bOrient) :
        curve(pCurve), orient(bOrient)
    {}

    const SketchCurve3D* curve;
    bool orient;
};

// The 3D counterpart of SketchProfile: checks that the sketch curves form a
// single closed loop. Purely curve-level, builds no topology; the generation
// consumes getLoop() and creates the edges itself
class WY3D_EXPORT Sketch3DProfile
{
public:
    explicit Sketch3DProfile(const Sketch3D* pSketch3D, double tol = 1e-5);

    // 校验
    bool check();

    // 获取错误
    std::shared_ptr<SketchError> getError() const { return _pError; }

    // 获取有序定向的曲线环 (校验通过后有效)
    const std::vector<BiCurve3D>& getLoop() const { return _loopCurves; }

private:
    // 初始化
    bool init();
    void setError(ErrorCode type, const std::vector<wydb::ElementId>& ids);

private:
    const Sketch3D* _pSketch3D;
    double _tol;
    bool _isValid;

    // 曲线环
    std::vector<BiCurve3D> _loopCurves;

    // 错误信息
    std::shared_ptr<SketchError> _pError;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_PROFILE_H
