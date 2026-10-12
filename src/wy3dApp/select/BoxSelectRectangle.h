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

#pragma once

#include <osg/Camera>
#include <osg/View>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 框选框
class BoxSelectRectangle : public osg::Camera
{
public:
    BoxSelectRectangle(osg::View* view);

    // 显示
    void show();
    // 隐藏
    void hide();
    // 更新
    void update(const osg::Vec2d& pnt1, const osg::Vec2d& pnt2);
    // 

public:
    osg::View* view_;
    osg::ref_ptr<osg::Geometry> geom_;
    osg::ref_ptr<osg::Vec3Array> vertices_;
    osg::ref_ptr<osg::StateAttribute> lineStipple_;

    osg::Vec2d pnt1_;
    osg::Vec2d pnt2_;
};

} // namespace wy3dApp
