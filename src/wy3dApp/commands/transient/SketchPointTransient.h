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

#ifndef WY3DAPP_SKETCH_POINT_TRANSIENT_H
#define WY3DAPP_SKETCH_POINT_TRANSIENT_H

#include <osg/Array>
#include <osg/Vec4>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dPlane.h>

#include <wy3dApp/GuiCmdTransient.h>

class SketchPointTransient : public GuiCmdTransient
{
public:
    SketchPointTransient(const wy3d::Plane& sketchPlane, const wy::Vector2& point,
        float pointSize, const osg::Vec4& color);

    void update(const wy::Vector2& point);

private:
    wy3d::Plane _sketchPlane;
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
};

#endif // WY3DAPP_SKETCH_POINT_TRANSIENT_H