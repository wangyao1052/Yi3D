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

#include <wy3dApp/Colors.h>
#include <wy3dApp/wy3dAppDefs.h>
#include <wy3dColor.h>

#define BrightGreen  osg::Vec4(0.0f, 0.93f, 0.0f, 1.0f)  // #00ED00 亮绿色
#define MediumGreen  osg::Vec4(0.0f, 0.765f, 0.0f, 1.0f) // #00C300 中绿色

#define BrightOrange osg::Vec4(0.9f, 0.39f, 0.1f, 1.0f) // 亮橙色(CREO)

namespace wy3dApp {

osg::Vec4 Colors::toOsgColor(const wy3d::Color& color)
{
    constexpr float inv255 = 1.0f / 255.0f;
    return osg::Vec4(
        static_cast<float>(color.red) * inv255,
        static_cast<float>(color.green) * inv255,
        static_cast<float>(color.blue) * inv255,
        1.0f);
}

namespace {

// mesh 调色板的原始值（不再作为 Colors 的公开常量）
const osg::Vec4 kSolidFaceValue(0.55f, 0.60f, 0.65f, 1.0f);           // 金属色(CREO)
const osg::Vec4 kSolidEdgeValue(0.0f, 0.0f, 0.0f, 1.0f);              // 黑色
const osg::Vec4 kSolidFaceHighlightValue(0.0f, 0.60f, 0.0f, 1.0f);    // 深绿色(CREO)
const osg::Vec4 kSolidEdgeHighlightValue(0.0f, 0.93f, 0.0f, 1.0f);    // 亮绿色
const osg::Vec4 kSolidFacePreviewValue(0.70f, 0.43f, 0.29f, 1.0f);    // 暗橙色
const osg::Vec4 kSolidEdgePreviewValue(0.30f, 0.20f, 0.10f, 1.0f);    // 深棕色

const osg::Vec4 kSheetFaceValue(0.42f, 0.56f, 0.80f, 1.0f);             // 浅蓝色
const osg::Vec4 kSheetEdgeValue(0.20f, 0.35f, 0.55f, 1.0f);             // 深蓝色
const osg::Vec4 kSheetFaceHighlightValue(0.0f, 0.60f, 0.0f, 1.0f);      // 深绿色(同Solid)
const osg::Vec4 kSheetEdgeHighlightValue(0.0f, 0.93f, 0.0f, 1.0f);      // 亮绿色(同Solid)
const osg::Vec4 kSheetFacePreviewValue(0.70f, 0.43f, 0.29f, 1.0f);      // 暗橙色(同Solid)
const osg::Vec4 kSheetEdgePreviewValue(0.30f, 0.20f, 0.10f, 1.0f);      // 深棕色(同Solid)

} // namespace

// sky blue
const osg::Vec4 Colors::kSkyBlue(0.251f, 0.682f, 1.0f, 1.0f);  // 天蓝色 #40AEFF

// rust red
const osg::Vec4 Colors::kRustRed(0.72f, 0.25f, 0.15f, 0.4f); // 铁锈红 #B84026, 40% 不透明

// gray
const osg::Vec4 Colors::kGray(0.5f, 0.5f, 0.5f, 0.5f); // 灰色

// ocean blue
const osg::Vec4 Colors::kOceanBlue(0.0f, 0.498f, 0.698f, 1.0f); // #007FB2

// pink
const osg::Vec4 Colors::kPink(1.0f, 0.5f, 0.75f, 1.0f);  // 粉红色(SolidWorks)

// sketch / curve
const Colors::LinePalette Colors::sketch = {
    osg::Vec4(0.0f, 0.75f, 0.92f, 1.0f),  // 青色(CREO)
    BrightGreen,                          // 绿色++
    BrightOrange                          // 亮橙色(CREO)
};

const Colors::LinePalette Colors::sketchEntity = {
    osg::Vec4(0.44f, 0.19f, 0.63f, 1.0f),  // 紫色
    MediumGreen,                             // #00C300 (Creo 草图元素高亮色)
    osg::Vec4(1.0f, 0.39f, 0.04f, 1.0f)    // 亮橙色++
};

// datum plane
namespace {

Colors::PlanePalette makeDatumPlanePalette()
{
    Colors::PlanePalette p;
    p.face.normal = osg::Vec4(0.47f, 0.32f, 0.32f, 0.05f);  // 暗棕色(CREO)
    p.face.highlight = osg::Vec4(
        kSolidFaceHighlightValue.x(),
        kSolidFaceHighlightValue.y(),
        kSolidFaceHighlightValue.z(),
        0.1f);
    p.face.preview = osg::Vec4(
        kSolidFacePreviewValue.x(),
        kSolidFacePreviewValue.y(),
        kSolidFacePreviewValue.z(),
        0.1f);

    p.edge.normal = osg::Vec4(0.47f, 0.32f, 0.32f, 1.0f);  // 暗棕色(CREO)
    p.edge.highlight = kSolidFaceHighlightValue;
    p.edge.preview = Colors::sketch.preview;
    return p;
}

} // namespace

const Colors::PlanePalette Colors::datumPlane = makeDatumPlanePalette();

namespace {

Colors::Mesh makeMesh(const osg::Vec4& face,
    const osg::Vec4& edge,
    const osg::Vec4& faceHighlight,
    const osg::Vec4& edgeHighlight,
    const osg::Vec4& facePreview,
    const osg::Vec4& edgePreview)
{
    Colors::Mesh m;
    m.element.normal.face = face;
    m.element.normal.edge = edge;
    m.element.highlight.face = faceHighlight;
    m.element.highlight.edge = edgeHighlight;
    m.element.preview.face = facePreview;
    m.element.preview.edge = edgePreview;
    m.element.preview.edgeWireframe = BrightOrange;

    m.face.highlight = faceHighlight;
    m.face.preview = facePreview;
    m.edge.highlight = BrightGreen;
    m.edge.preview = BrightOrange;
    m.inactive = Colors::kRustRed;
    return m;
}

} // namespace

const Colors::Mesh Colors::solidMesh = makeMesh(
    kSolidFaceValue, kSolidEdgeValue,
    kSolidFaceHighlightValue, kSolidEdgeHighlightValue,
    kSolidFacePreviewValue, kSolidEdgePreviewValue);

const Colors::Mesh Colors::sheetMesh = makeMesh(
    kSheetFaceValue, kSheetEdgeValue,
    kSheetFaceHighlightValue, kSheetEdgeHighlightValue,
    kSheetFacePreviewValue, kSheetEdgePreviewValue);

} // namespace wy3dApp
