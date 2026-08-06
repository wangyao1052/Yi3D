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

#include "scene/Colors.h"

#define BrightGreen  osg::Vec4(0.0f, 0.85f, 0.0f, 1.0f) // 亮绿色
#define BrightOrange osg::Vec4(0.9f, 0.39f, 0.1f, 1.0f) // 亮橙色(CREO)

// solid
const osg::Vec4 Colors::kSolidFace(0.55f, 0.60f, 0.65f, 1.0f);           // 金属色(CREO)
const osg::Vec4 Colors::kSolidEdge(0.0f, 0.0f, 0.0f, 1.0f);              // 黑色
const osg::Vec4 Colors::kSolidFace_Highlight(0.0f, 0.665f, 0.0f, 1.0f);  // 深绿色(CREO)
const osg::Vec4 Colors::kSolidEdge_Highlight(0.0f, 0.85f, 0.0f, 1.0f); // 亮绿色
const osg::Vec4 Colors::kSolidFace_Preview(0.70f, 0.43f, 0.29f, 1.0f);   // 暗橙色
const osg::Vec4 Colors::kSolidEdge_Preview(0.30f, 0.20f, 0.10f, 1.0f);   // 根据DeepSeek的建议改为深棕色

// cut
const osg::Vec4 Colors::kCutFace(0.0f, 0.75f, 1.0f, 1.0f);   // 半透明青蓝色
const osg::Vec4 Colors::kCutEdge(0.0f, 0.3f, 0.6f, 1.0f);    // 深青色边框

// solid edge
const osg::Vec4 Colors::kEdge_Highlight = BrightGreen;
const osg::Vec4 Colors::kEdge_Preview = BrightOrange;

// sketch
const osg::Vec4 Colors::kSketch(0.0f, 0.75f, 0.92f, 1.0f);               // 青色(CREO)
const osg::Vec4 Colors::kSketch_Highlight = BrightGreen;                 // 绿色++
const osg::Vec4 Colors::kSketch_Preview = BrightOrange;                  // 亮橙色(CREO)

// sketch entity
const osg::Vec4 Colors::kSketchEntity(0.44f, 0.19f, 0.63f, 1.0f);        // 紫色
const osg::Vec4 Colors::kSketchEntity_Highlight = kSketch_Highlight;     // 绿色++
const osg::Vec4 Colors::kSketchEntity_Preview(1.0f, 0.39f, 0.04f, 1.0f); // 亮橙色++

// sketch entity construction
const osg::Vec4 Colors::kSketchEntityConstruction(0.0f, 0.0f, 0.0f, 1.0); // 黑色
const osg::Vec4 Colors::kSketchEntityConstruction_Highlight(1.0f, 0.0f, 0.0f, 1.0f); // 红色
const osg::Vec4 Colors::kSketchEntityConstruction_Preview = Colors::kSketch_Preview;

// datum plane
const osg::Vec4 Colors::kDatumPlaneFace(0.47f, 0.32f, 0.32f, 0.05f); // 暗棕色(CREO)
const osg::Vec4 Colors::kDatumPlaneEdge(0.47f, 0.32f, 0.32f, 1.0f);  // 暗棕色(CREO)
const osg::Vec4 Colors::kDatumPlaneFace_Highlight(
    Colors::kSolidFace_Highlight.x(),
    Colors::kSolidFace_Highlight.y(),
    Colors::kSolidFace_Highlight.z(),
    0.1f);
const osg::Vec4 Colors::kDatumPlaneEdge_Highlight = Colors::kSolidFace_Highlight;
const osg::Vec4 Colors::kDatumPlaneFace_Preview(
    Colors::kSolidFace_Preview.x(),
    Colors::kSolidFace_Preview.y(),
    Colors::kSolidFace_Preview.z(),
    0.1f);
const osg::Vec4 Colors::kDatumPlaneEdge_Preview = kSketch_Preview;

// transparent
const osg::Vec4 Colors::kTransparent(0.72f, 0.25f, 0.15f, 0.4f); // 暗黄色

// ghost gizmo
const osg::Vec4 Colors::kGhostGizmo(0.5f, 0.5f, 0.5f, 0.5f); // 灰色

// pink
const osg::Vec4 Colors::kPink(1.0f, 0.5f, 0.75f, 1.0f);  // 粉红色(SolidWorks)