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

#ifndef WY3D_SKETCH_PROFILE_FOR_SHEET_H
#define WY3D_SKETCH_PROFILE_FOR_SHEET_H

#include <vector>
#include <memory>

#include <wy3dDefs.h>
#include <wy3dSketch.h>
#include <wy3dSketchProfile.h>

NS_WY3D_BEG

// 用于 Sheet（拉伸曲面、旋转曲面等）的轮廓查找。
// 区别于 SketchProfile（只找闭合环），本类查找所有连通曲线链（开放+闭合）。
//
// 接口对齐 SketchProfile：
//   - 构造传入 Sketch*
//   - check() 执行查找
//   - getFaces() 返回 BiCurve 格式结果（与 SketchProfile 一致，可直接传入 makeWires）
//
// 内部组合 SketchCurveGraph_Profile，不继承。
class WY3D_EXPORT SketchProfileForSheet
{
public:
    explicit SketchProfileForSheet(const Sketch* pSketch, double tol = 1e-5);

    // 查找所有连通曲线链
    bool check();

    // 获取错误
    std::shared_ptr<SketchError> getError() const { return _pError; }

    // 获取环集（每个 chain 作为一个 Loop）
    const std::vector<SketchProfile::LoopSPtr>& getLoops() const { return _loops; }

private:
    const Sketch* _pSketch;
    double _tol;

    std::vector<SketchProfile::LoopSPtr> _loops;
    std::shared_ptr<SketchError> _pError;
};

NS_WY3D_END

#endif // WY3D_SKETCH_PROFILE_FOR_SHEET_H
