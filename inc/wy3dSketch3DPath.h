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

#ifndef WY3D_SKETCH3D_PATH_H
#define WY3D_SKETCH3D_PATH_H

#include <vector>
#include <memory>

#include <wy3dDefs.h>
#include <wy3dSketch3D.h>
#include <wy3dSketch3DProfile.h>

NS_WY3D_BEG

// The 3D counterpart of SketchPath: checks that the sketch curves form a single
// open chain or a single closed loop. Purely curve-level, builds no topology;
// the generation consumes getPath() and creates the edges itself
class WY3D_EXPORT Sketch3DPath
{
public:
    explicit Sketch3DPath(const Sketch3D* pSketch3D, double tol = 1e-5);

    // 校验
    bool check();

    // 获取错误
    std::shared_ptr<SketchError> getError() const { return _pError; }

    // 获取有序定向的路径曲线 (校验通过后有效)
    const std::vector<BiCurve3D>& getPath() const { return _path; }

private:
    // 初始化
    bool init();
    void setError(ErrorCode type, const std::vector<wydb::ElementId>& ids);

private:
    const Sketch3D* _pSketch3D;
    double _tol;
    bool _isValid;

    // 路径曲线
    std::vector<BiCurve3D> _path;

    // 错误信息
    std::shared_ptr<SketchError> _pError;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_PATH_H
