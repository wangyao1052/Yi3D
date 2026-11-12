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

namespace wy3dApp {

class WY3DAPP_EXPORT Colors
{
public:
    struct MeshPalette
    {
        struct
        {
            struct
            {
                osg::Vec4 face;
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
    };

    static const MeshPalette solidMesh;
    static const MeshPalette sheetMesh;

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

    static const PlanePalette datumPlane;

    struct LinePalette
    {
        osg::Vec4 normal;
        osg::Vec4 highlight;
        osg::Vec4 preview;
    };

    static const LinePalette sketch;
    static const LinePalette sketchEntity;
};

} // namespace wy3dApp

#endif // WY3DAPP_COLORS_H
