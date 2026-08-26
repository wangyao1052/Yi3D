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

#ifndef WY3DAPP_COMMAND_NAMES_H
#define WY3DAPP_COMMAND_NAMES_H

#include <string>

class CommandNames
{
public:
    // 默认命令组
    static inline const std::string DefaultGroupName = "yi3d";

    // 选择
    static inline const std::string Select = "Select";
    static inline const std::string Show   = "Show";
    static inline const std::string Hide   = "Hide";

    // 撤销&重做
    static inline const std::string Undo = "Undo";
    static inline const std::string Redo = "Redo";

    // 建模命令
    static inline const std::string MakeBox = "MakeBox";
    static inline const std::string MakeCylinder = "MakeCylinder";
    static inline const std::string MakeSphere = "MakeSphere";
    static inline const std::string MakeCone = "MakeCone";
    static inline const std::string MakeTorus = "MakeTorus";
    static inline const std::string MakeTube = "MakeTube";
    static inline const std::string Union = "Union";
    static inline const std::string Subtract = "Subtract";
    static inline const std::string Intersect = "Intersect";
    static inline const std::string Extrude = "Extrude";
    static inline const std::string ExtrudeCut = "ExtrudeCut";
    static inline const std::string ExtrudedSheet = "ExtrudedSheet";
    static inline const std::string RevolvedSheet = "RevolvedSheet";
    static inline const std::string SweptSheet = "SweptSheet";
    static inline const std::string LoftedSheet = "LoftedSheet";
    static inline const std::string PlanarSheet = "PlanarSheet";
    static inline const std::string SewnSheet = "SewnSheet";
    static inline const std::string Thicken = "Thicken";
    static inline const std::string Solidify = "Solidify";
    static inline const std::string OffsetSheet = "OffsetSheet";
    static inline const std::string Revolve = "Revolve";
    static inline const std::string RevolveCut = "RevolveCut";
    static inline const std::string Sweep = "Sweep";
    static inline const std::string SweepCut = "SweepCut";
    static inline const std::string Loft = "Loft";
    static inline const std::string LoftCut = "LoftCut";
    static inline const std::string Merge = "Merge";

    // 实体编辑命令
    static inline const std::string Chamfer = "Chamfer";
    static inline const std::string Fillet = "Fillet";
    static inline const std::string Shell = "Shell";
    static inline const std::string Draft = "Draft";

    // 实体测试命令
    static inline const std::string TopoName = "TopoName";
    static inline const std::string CheckTopoName = "CheckTopoName";

    // 基准平面命令
    static inline const std::string DatumPlaneSeries = "DatumPlaneSeries";
    static inline const std::string ParallelDatumPlane = "ParallelDatumPlane";
    static inline const std::string CoincidentDatumPlane = "CoincidentDatumPlane";
    static inline const std::string AngularDatumPlane = "AngularDatumPlane";
    static inline const std::string PerpendicularDatumPlane = "PerpendicularDatumPlane";
    static inline const std::string ThroughAxisDatumPlane = "ThroughAxisDatumPlane";
    static inline const std::string NormalToCurveDatumPlane = "NormalToCurveDatumPlane";
    static inline const std::string Through3PointsDatumPlane = "Through3PointsDatumPlane";
    static inline const std::string TangentDatumPlane = "TangentDatumPlane";

    // 草图命令
    static inline const std::string NewSketch    = "NewSketch";
    static inline const std::string EditSketch   = "EditSketch";
    static inline const std::string EndSketch    = "EndSketch";
    static inline const std::string CancelSketch = "CancelSketch";
    static inline const std::string RelocateSketchCSYS = "RelocateSketchCSYS";

    // 草绘图元命令
    static inline const std::string Point = "Point";
    static inline const std::string LineSeries = "LineSeries";
    static inline const std::string Line = "Line";
    static inline const std::string LineTangent = "LineTangent";
    static inline const std::string CenterLine = "CenterLine";
    static inline const std::string Rectangle = "Rectangle";
    static inline const std::string CenterRectangle = "CenterRectangle";
    static inline const std::string Polygon = "Polygon";
    static inline const std::string Circle = "Circle";
    static inline const std::string CircleBy3Points = "CircleBy3Points";
    static inline const std::string Arc = "Arc";
    static inline const std::string ArcBy3Points = "ArcBy3Points";
    static inline const std::string Ellipse = "Ellipse";
    static inline const std::string EllipseArc = "EllipseArc";
    static inline const std::string SplineSeries = "SplineSeries";
    static inline const std::string Spline = "Spline";
    static inline const std::string EquationDrivenSpline = "EquationDrivenSpline";
    static inline const std::string StyleSpline = "StyleSpline";
    static inline const std::string SketchText = "SketchText";

    // 三维曲线命令
    static inline const std::string Helix = "Helix";

    // 编辑命令
    static inline const std::string Move             = "Move";             // 移动
    static inline const std::string Copy             = "Copy";             // 复制
    static inline const std::string CopyClip         = "CopyClip";         // 复制到剪贴板
    static inline const std::string PasteClip        = "PasteClip";        // 从剪贴板粘贴
    static inline const std::string Rotate           = "Rotate";           // 旋转
    static inline const std::string Mirror           = "Mirror";           // 镜像
    static inline const std::string SketchMirror     = "SketchMirror";     // 草图镜像
    static inline const std::string SketchScale      = "SketchScale";      // 草图缩放
    static inline const std::string Trim             = "Trim";             // 草图修剪
    static inline const std::string Extend           = "Extend";           // 草图延伸
    static inline const std::string SketchFillet     = "SketchFillet";     // 草图圆角
    static inline const std::string SketchChamfer    = "SketchChamfer";    // 草图倒角
    static inline const std::string SketchOffset     = "SketchOffset";     // 草图偏移
    static inline const std::string SketchProject   = "SketchProject";   // 草图投影
    static inline const std::string SketchRectArray  = "SketchRectArray";  // 草图矩形阵列
    static inline const std::string SketchPolarArray = "SketchPolarArray"; // 草图环形阵列
    static inline const std::string LinearPattern    = "LinearPattern";    // 线性阵列
    static inline const std::string CircularPattern  = "CircularPattern";  // 圆周阵列

    // 文件命令
    static inline const std::string NewFile      = "NewFile";
    static inline const std::string OpenFile     = "OpenFile";
    static inline const std::string SaveFile     = "SaveFile";
    static inline const std::string SaveAsFile   = "SaveAsFile";
    static inline const std::string ExportFile   = "ExportFile";
    static inline const std::string ExportSelected  = "ExportSelected";
    static inline const std::string ImportFile   = "ImportFile";
    static inline const std::string ImportSketch = "ImportSketch";
    static inline const std::string ExportSketch = "ExportSketch";

    // 视图命令
    static inline const std::string FitView = "FitView";
    static inline const std::string FitSelection = "FitSelection";
    static inline const std::string IsometricView = "IsometricView";
    static inline const std::string FrontView = "FrontView";
    static inline const std::string BackView = "BackView";
    static inline const std::string LeftView = "LeftView";
    static inline const std::string RightView = "RightView";
    static inline const std::string TopView = "TopView";
    static inline const std::string BottomView = "BottomView";
    static inline const std::string OrientToSketch = "OrientToSketch";
    static inline const std::string ViewNormalTo = "ViewNormalTo";

    // 相机命令
    static inline const std::string OrthoCamera = "OrthoCamera";
    static inline const std::string PerspectiveCamera = "PerspectiveCamera";

    // 显示模式命令
    static inline const std::string ShadedWithEdgesDisplay = "ShadedWithEdgesDisplay";
    static inline const std::string ShadedDisplay = "ShadedDisplay";
    static inline const std::string WireframeDisplay = "WireframeDisplay";

    // 实用工具命令
    static inline const std::string MeasureDistance = "MeasureDistance";
    static inline const std::string RunScript = "RunScript";
    static inline const std::string SetColor = "SetColor";
    static inline const std::string FindElementById = "FindElementById";

    // 帮助命令
    static inline const std::string About = "About";
    static inline const std::string HelpDocumentation = "HelpDocumentation";
    static inline const std::string ShortcutKeys = "ShortcutKeys";

    // 测试命令
    static inline const std::string OsgNewBox = "OsgNewBox";
    static inline const std::string OpenGLNewBox = "OpenGLNewBox";
    static inline const std::string OsgNewCylinder = "OsgNewCylinder";
    static inline const std::string OsgNewSphere = "OsgNewSphere";
    static inline const std::string OsgNewCow = "OsgNewCow";
    static inline const std::string SketchTestIntersection = "SketchTestIntersection";
    static inline const std::string SketchTestBoundingBox = "SketchTestBoundingBox";
};

#endif // WY3DAPP_COMMAND_NAMES_H
