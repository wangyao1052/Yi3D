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

#ifndef WY3DAPP_ELEMENT_RENDER_DATA_H
#define WY3DAPP_ELEMENT_RENDER_DATA_H

#include <osg/Array>
#include <osg/Matrix>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT ElementRenderData
{
public:
    ElementRenderData();
    virtual ~ElementRenderData();

    ElementRenderData(const ElementRenderData&) = delete;
    ElementRenderData& operator=(const ElementRenderData&) = delete;

    // 顶点数据
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    // 局部坐标到世界坐标的模型变换
    osg::Matrix matrix = osg::Matrix::identity();
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_RENDER_DATA_H
