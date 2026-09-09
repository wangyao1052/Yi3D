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

#ifndef WY3DAPP_RENDER_CONST_H
#define WY3DAPP_RENDER_CONST_H

#include <string>

class RenderBinNumers
{
public:
    static constexpr unsigned int SketchElement      = 10; // 草图
    static constexpr unsigned int DATUM_PLANE        = 15; // 参照面(透明)
    static constexpr unsigned int DATUM_PLANE_LINE   = 20; // 参照面(透明)
    static constexpr unsigned int WCS                = 30; // WCS
    static constexpr unsigned int SketchEntity       = 35; // 草绘图元
    static constexpr unsigned int SketchEntity3D     = 36; // 3D草绘图元
    static constexpr unsigned int Preview            = 40;
    static constexpr unsigned int Highlight          = 40;
    static constexpr unsigned int GuiTransientObject = 50; // 交互临时对象
    static constexpr unsigned int SnapObject         = 50; // 捕捉对象
    static constexpr unsigned int Gizmo              = 60; // Gizmo
};

class RenderBinNames
{
public:
    static inline std::string RenderBin = "RenderBin";
    static inline std::string DepthSortedBin = "DepthSortedBin";
};

// 中心线
static const inline int CENTER_LINE_STIPPLE_FACTOR = 2;
static const inline unsigned short CENTER_LINE_STIPPLE_PATTERN = 0xFF18;

// 点划线
static const inline int DOT_LINE_STIPPLE_FACTOR = 2;
static const inline unsigned short DOT_LINE_STIPPLE_PATTERN = 0xAAAA;

#define PICK_MASK 0x0000FFFF

#endif // WY3DAPP_RENDER_CONST_H