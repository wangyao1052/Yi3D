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

class Colors
{
public:
    // solid
    static const osg::Vec4 kSolidFace;
    static const osg::Vec4 kSolidEdge;
    static const osg::Vec4 kSolidFace_Highlight;
    static const osg::Vec4 kSolidEdge_Highlight;
    static const osg::Vec4 kSolidFace_Preview;
    static const osg::Vec4 kSolidEdge_Preview;

    // cut
    static const osg::Vec4 kCutFace;
    static const osg::Vec4 kCutEdge;
    
    // solid edge
    static const osg::Vec4 kEdge_Highlight;
    static const osg::Vec4 kEdge_Preview;

    // sketch
    static const osg::Vec4 kSketch;
    static const osg::Vec4 kSketch_Highlight;
    static const osg::Vec4 kSketch_Preview;

    // sketch entity
    static const osg::Vec4 kSketchEntity;
    static const osg::Vec4 kSketchEntity_Highlight;
    static const osg::Vec4 kSketchEntity_Preview;
    // sketch entity construction
    static const osg::Vec4 kSketchEntityConstruction;
    static const osg::Vec4 kSketchEntityConstruction_Highlight;
    static const osg::Vec4 kSketchEntityConstruction_Preview;

    // datum plane
    static const osg::Vec4 kDatumPlaneFace;
    static const osg::Vec4 kDatumPlaneEdge;
    static const osg::Vec4 kDatumPlaneFace_Highlight;
    static const osg::Vec4 kDatumPlaneEdge_Highlight;
    static const osg::Vec4 kDatumPlaneFace_Preview;
    static const osg::Vec4 kDatumPlaneEdge_Preview;

    // sheet
    static const osg::Vec4 kSheetFace;
    static const osg::Vec4 kSheetEdge;
    static const osg::Vec4 kSheetFace_Highlight;
    static const osg::Vec4 kSheetEdge_Highlight;
    static const osg::Vec4 kSheetFace_Preview;
    static const osg::Vec4 kSheetEdge_Preview;

    // transparent
    static const osg::Vec4 kTransparent;

    // ghost gizmo
    static const osg::Vec4 kGhostGizmo;

    // 备选的一些颜色
    // 粉色
    static const osg::Vec4 kPink;
};

#endif // WY3DAPP_COLORS_H