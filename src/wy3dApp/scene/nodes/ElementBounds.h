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

#ifndef WY3DAPP_ELEMENT_BOUNDS_H
#define WY3DAPP_ELEMENT_BOUNDS_H

#include <osg/Array>
#include <osg/BoundingBox>
#include <osg/Matrix>

namespace wy3dApp {

// 元素节点包围盒工具(内部实现,不导出)
struct ElementBounds
{
    // 由顶点数组计算局部包围盒
    static osg::BoundingBox compute(const osg::Vec3Array& vertices);
    // 按模型矩阵把包围盒变换到世界坐标
    static void transform(osg::BoundingBox& bbox, const osg::Matrix& matrix);
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_BOUNDS_H
