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

#ifndef WY3DAPP_MAKE_DATUM_PLANE_H
#define WY3DAPP_MAKE_DATUM_PLANE_H

#include <Geom_Curve.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelection.h>
#include <wy3dSketchPlane.h>
#include <wy3dDatumPlane.h>
#include <wy3dImpl.h>
#include "commands/OsgGuiCommand.h"

class MakeDatumPlane : public GuiCmdMakeElement
{
public:
    explicit MakeDatumPlane(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pDatumPlane(nullptr) {}
    ~MakeDatumPlane() {}

    wydb::ElementId getId() const
    {
        if (_pDatumPlane) return _pDatumPlane->getId();
        else return wydb::ElementId::kNull;
    }

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool create(const wy3d::SketchPlane& sketchPlane);
    // 更新
    bool update(const wy3d::SketchPlane& sketchPlane);

    // 获取草图平面
    static bool getSketchPlane(const wyap::Selection& sel, wy3d::SketchPlane& sketchPlane);
    // 获取边的起点和终点
    static bool getSolidEdgeEndPoints(const wyap::Selection& sel, wy::Vector3& startPnt, wy::Vector3& endPnt);
    // 获取边的曲线
    static Handle(Geom_Curve) getSolidEdgeGeomCurve(const wyap::Selection& sel);
    static Handle(Geom_Curve) getSketchCurveGeomCurve(const wyap::Selection& sel);
    static Handle(Geom_Curve) getCurveGeomCurve(const wyap::Selection& sel);
    static Handle(Geom_Curve) getSketchCurve3DGeomCurve(const wyap::Selection& sel);

    // 获取实体圆柱面的中心平面
    static bool getSolidCylindricalFaceCenterPlane(
        const wyap::Selection& sel,
        wy3d::SketchPlane& plane,
        double& radius);

    // 求出直线与基准面的交线
    // plnMinPnt,plnMaxPnt基准面的两个对角点
    static bool intersect(
        const wy::Vector2& plnMinPnt, const wy::Vector2& plnMaxPnt,
        const wy::Vector2& lineOrigin, const wy::Vector2& lineDir,
        wy::Vector2& intStartPnt, wy::Vector2& intEndPnt,
        double tol = wy3d::TOL);

    // 求两个SketchPlane的交线
    static bool intersect(const wy3d::SketchPlane& lhs, const wy3d::SketchPlane& rhs,
        wy::Vector3& linePoint, wy::Vector3& lineDir, double tol = wy3d::TOL);

    // 过3点求平面
    static bool computeThrough3PointsPlane(const wy::Vector3& pnt1, const wy::Vector3& pnt2, const wy::Vector3& pnt3,
        wy3d::SketchPlane& sketchPlane, double tol = wy3d::TOL);

private:
    wy3d::DatumPlane* _pDatumPlane;
};

#endif // WY3DAPP_MAKE_DATUM_PLANE_H