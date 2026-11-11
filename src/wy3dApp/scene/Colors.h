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

#ifndef WY3DAPP_COLORS_H
#define WY3DAPP_COLORS_H

#include <osg/Vec3>
#include <osg/Vec4>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3d { class Color; }

namespace wy3dApp {

class WY3DAPP_EXPORT Colors
{
public:
    static osg::Vec4 toOsgColor(const wy3d::Color& color);

    // mesh 调色板（Solid/Sheet 各一份）
    struct Mesh
    {
        struct
        {
            struct
            {
                osg::Vec4 face;  // 默认常态面色（元素颜色会覆盖）
                osg::Vec4 edge;
            } normal;

            struct
            {
                osg::Vec4 face;
                osg::Vec4 edge;
            } highlight;

            struct
            {
                osg::Vec4 face;
                osg::Vec4 edge;
                osg::Vec4 edgeWireframe;  // 线框模式下的元素整体预览
            } preview;
        } element;

        struct
        {
            osg::Vec4 highlight;
            osg::Vec4 preview;
        } face;

        struct
        {
            osg::Vec4 highlight;
            osg::Vec4 preview;
        } edge;

        osg::Vec4 inactive;
    };

    static const Mesh solidMesh;
    static const Mesh sheetMesh;

    // 线状元素（草图/曲线）调色板
    struct LinePalette
    {
        osg::Vec4 normal;
        osg::Vec4 highlight;
        osg::Vec4 preview;
    };

    // 基准面调色板
    struct PlanePalette
    {
        struct
        {
            osg::Vec4 normal;
            osg::Vec4 highlight;
            osg::Vec4 preview;
        } face;

        struct
        {
            osg::Vec4 normal;
            osg::Vec4 highlight;
            osg::Vec4 preview;
        } edge;
    };

    static const LinePalette sketch;
    static const LinePalette sketchEntity;
    static const PlanePalette datumPlane;

    // sky blue
    static const osg::Vec4 kSkyBlue;

    // rust red
    static const osg::Vec4 kRustRed;

    // gray
    static const osg::Vec4 kGray;

    // ocean blue
    static const osg::Vec4 kOceanBlue;

    // 备选的一些颜色
    // 粉色
    static const osg::Vec4 kPink;
};

} // namespace wy3dApp

#endif // WY3DAPP_COLORS_H
