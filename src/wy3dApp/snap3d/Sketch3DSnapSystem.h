///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH3D_SNAP_SYSTEM_H
#define WY3DAPP_SKETCH3D_SNAP_SYSTEM_H

#include <set>

#include <wyVector3.h>
#include <wy3dSketchPlane.h>
#include <wydbElementId.h>

#include <osgViewer/View>

#include "snap3d/Sketch3DSnapContext.h"
#include "snap3d/Sketch3DSnapResult.h"

namespace wydb
{
class Database;
}

// 3D草图捕捉体系(对应2D草图snap/SketchSnapSystem)
// v1:画线上下文优先级:切点相切(终点吸附到从起点向工作平面上的圆所作切线的切点)>
//     水平/竖直>平行/垂直已有线(参考线仅限方向平行于工作平面;
//     平行/垂直命中且长度接近参考线时追加长度相等捕捉,水平/竖直不做相等);
//     画圆上下文做半径相等捕捉(参考圆仅限平面平行于工作平面).
// 点捕捉(端点/中点等)不在本体系,仍由全局SnapSystem在computePosition3d内完成,
// 命令在未点捕捉时才调用本体系,优先级:点捕捉 > 角度/相等捕捉.
// 工作平面是每次snap()的调用参数,空格切平面后无需通知本体系.
class Sketch3DSnapSystem
{
public:
    Sketch3DSnapSystem();
    ~Sketch3DSnapSystem();

    // 捕捉入口:按上下文分派,内部完成结果差分显隐
    // 参数:rawPnt命令算出的原始3D点;
    //       返回空表示未捕捉(此时内部会清除已有显示)
    Sketch3DSnapResultSPtr snap(
        const Sketch3DSnapContext* pContext,
        osgViewer::View* pView,
        const wy::Vector3& rawPnt,
        const wy3d::SketchPlane& workPlane,
        const std::set<wydb::ElementId>& excludeIds,
        const wydb::ElementId& sketch3dId);

    // 清除捕捉显示(命令步骤切换/切平面/退出时调用)
    void clearSnapResult();

    // 结果差分显隐(相同则保持显示,避免图标闪烁;与2D草图setSnapResult同语义,
    // 供统一入口computePosition3dForSketch3D推送全局点捕捉的转换结果)
    void setSnapResult(Sketch3DSnapResultSPtr pResult);

private:
    // 画线:角度捕捉(水平/竖直/平行/垂直)
    Sketch3DSnapResultSPtr snapDrawLine(
        wydb::Database* pDb,
        const Sketch3DDrawLineContext* pContext,
        osgViewer::View* pView,
        const wy::Vector3& rawPnt,
        const wy3d::SketchPlane& workPlane,
        const std::set<wydb::ElementId>& excludeIds,
        const wydb::ElementId& sketch3dId);

    // 画圆:半径相等捕捉(半径由原始点与圆心计算)
    Sketch3DSnapResultSPtr snapDrawCircle(
        wydb::Database* pDb,
        const Sketch3DDrawCircleContext* pContext,
        osgViewer::View* pView,
        const wy::Vector3& rawPnt,
        const wy3d::SketchPlane& workPlane,
        const std::set<wydb::ElementId>& excludeIds,
        const wydb::ElementId& sketch3dId);

    // 像素比例:屏幕1像素对应的世界单位数(与2D草图scaleScreen同语义,退化视图返回0)
    double computePixelScale(
        osgViewer::View* pView,
        const wy3d::SketchPlane& workPlane,
        const wy::Vector3& originPnt) const;

private:
    Sketch3DSnapResultSPtr _pSnapResult;
};

#endif // WY3DAPP_SKETCH3D_SNAP_SYSTEM_H