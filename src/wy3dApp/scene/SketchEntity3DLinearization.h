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

#ifndef WY3DAPP_SKETCH_ENTITY3D_LINEARIZATION_H
#define WY3DAPP_SKETCH_ENTITY3D_LINEARIZATION_H

#include <vector>
#include <wyVector3.h>
#include <wy3dSketchEntity3D.h>

class SketchEntity3DLinearization
{
public:
    explicit SketchEntity3DLinearization(const wy3d::SketchEntity3D* pEntity);

    // 直线段
    SketchEntity3DLinearization(const wy::Vector3& startPnt, const wy::Vector3& endPnt);
    // 圆
    SketchEntity3DLinearization(const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius);

    const std::vector<wy::Vector3>& getVertices() const { return _vertices; }
    const std::vector<unsigned int>& getIndices() const { return _indices; }

private:
    std::vector<wy::Vector3> _vertices;
    std::vector<unsigned int> _indices;
};

#endif // WY3DAPP_SKETCH_ENTITY3D_LINEARIZATION_H
