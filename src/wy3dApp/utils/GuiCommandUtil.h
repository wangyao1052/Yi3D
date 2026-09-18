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

#ifndef WY3DAPP_GUI_COMMAND_UTIL_H
#define WY3DAPP_GUI_COMMAND_UTIL_H

#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wyapSelection.h>
#include <wy3dSketchPlane.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>

class SketchSnapSystem;

struct GuiCmdSketchInfo
{
    wy3d::SketchPlane sketchPlane;
    wydb::ElementId sketchId;
    SketchSnapSystem* pSketchSnapSys;

    GuiCmdSketchInfo() : sketchPlane(), sketchId(wydb::ElementId::kNull), pSketchSnapSys(nullptr) {}
};

struct GuiCmdSketch3DInfo
{
    wydb::ElementId sketch3dId;

    GuiCmdSketch3DInfo() : sketch3dId(wydb::ElementId::kNull) {}
};

class GuiCommandUtil
{
public:
    // 获取工作平面
    static bool getWorkingPlane(const wyap::Selection& sel, wy3d::SketchPlane& workPln);

    // 从选择集合中过滤出顶层的实体特征
    static wyap::SelectionSet filterTopSolidFeaturesFrom(const wyap::SelectionSet& ss);

    // 从选择集中过滤出线性阵列和圆周阵列的源对象
    static wydb::ElementId filterPatternSourceFrom(const wyap::SelectionSet& ss);

    // 从选择集中过滤出镜像的源对象
    static wydb::ElementId filterMirrorSourceFrom(const wyap::SelectionSet& ss);

    // 自动获取切除的实体
    static const wy3d::Solid* autoGetSolidToCut(const wydb::Database* pDb);

    static bool isTopLevelBody(const wydb::Element* pElem);

    // 清除选择集
    static void clearSelections();

    // 初始化草图环境信息
    static GuiCmdSketchInfo initSketchInfo();

    // 初始化3D草图环境信息
    static bool initSketch3DInfo(GuiCmdSketch3DInfo& info);
};

#endif // WY3DAPP_GUI_COMMAND_UTIL_H