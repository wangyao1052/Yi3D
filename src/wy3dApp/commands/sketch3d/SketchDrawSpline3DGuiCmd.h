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

#ifndef WY3DAPP_SKETCH_DRAW_SPLINE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_SPLINE3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include "commands/transient/BasicTransient.h"
#include <cfloat>
#include <cstddef>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>
#include <wyVector3.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline3D.h>
#include <wydbElementId.h>
#include "snap/SnapObject.h"

class GuiCmdHoverInputPopup3;

class MakeSketchSpline3D : public GuiCmdMakeElement
{
public:
    MakeSketchSpline3D(GuiCommand* pGuiCmd, wy3d::SplineMode mode)
        : GuiCmdMakeElement(pGuiCmd), _mode(mode), _pSketchSpline3D(nullptr)
    {}
    ~MakeSketchSpline3D() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchSpline3D ? _pSketchSpline3D->getId() : wydb::ElementId::kNull;
    }

    // 以 startPnt(加 seedDir*kMinValue 作退化种子)建实体,后续 update 逐步改写
    bool init(const wy::Vector3& startPnt, const wy::Vector3& seedDir, wydb::ElementId sketch3dId);
    // 写已落点(控制点式按点数定次数)
    bool update(const std::vector<wy::Vector3>& points);
    // 预览:已落点 + 当前光标点
    bool update(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPnt);

private:
    wy3d::SplineMode _mode;
    std::vector<wy::Vector3> _pnts;
    wy3d::SketchSpline3D* _pSketchSpline3D;
};

// 3D草图样条:插值点式/控制点式共用基类(不注册),子类只设模式与闭合阈值
class SketchDrawSpline3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawSpline3DGuiCmd, SketchDrawSpline3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawSpline3DGuiCmd();
    virtual ~SketchDrawSpline3DGuiCmd();

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyStartPnt = 1,
        SpecifyNextPnt = 2,
    };

    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;
    virtual void onEscapeKey() override;
    bool finishStep(unsigned int step);
    void gotoStep(unsigned int step);
    void reset();

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseDoubleClicked(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;
    std::pair<wy::Vector3, bool> computePoint3d(double x, double y);
    // 落点够多且与首点重合即闭合(子类给出 _closureMinPoints)
    bool isClosed(const std::vector<wy::Vector3>& points) const;
    bool isAllowedClosurePoint(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPoint) const;
    bool isDisallowedDuplicatePoint(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPoint) const;
    wy::Vector3 tryReviseNextPoint(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPoint) const;

    void updateStartPointSnapObject();
    void removeStartPointSnapObject();
    void clearTransients();
    void updatePointTransient(const wy::Vector3& pnt);
    void updatePolygonTransients();
    void updateActivePathTransient(const wy::Vector3& nextPnt);

    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void onPopupSpaceKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        wy::Vector3 point;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
        }
    };

protected:
    // 由子类在构造时设置
    wy3d::SplineMode _splineMode;
    // 判定闭合所需的最少点数(插值式 4 / 控制点式 5,与实体一致)
    std::size_t _closureMinPoints;

private:
    unsigned int _step;
    wy::Vector3 _startPoint;
    wy::Vector3 _nextPoint;
    std::vector<wy::Vector3> _points;
    std::shared_ptr<MakeSketchSpline3D> _pMakeSketchSpline3D;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    HoverPopupState _hoverPopupState;

    std::list<PointTransientSPtr> _pointTransients;
    std::list<LineTransientSPtr> _pathTransients;
    LineTransientSPtr _pActivePathTransient;
    // 首点驻留吸附:点回首点即闭合
    wyap::SnapObjectSPtr _pStartPointSnapObject;
};

// 插值点式:曲线过每个点
class SketchDrawSpline3DGuiCmd_FitPoints : public SketchDrawSpline3DGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawSpline3DGuiCmd_FitPoints, SketchDrawSpline3DGuiCmd_FitPoints, SketchDrawSpline3DGuiCmd)
public:
    SketchDrawSpline3DGuiCmd_FitPoints() : SketchDrawSpline3DGuiCmd()
    {
        _splineMode = wy3d::SplineMode::InterpolationPoints;
        _closureMinPoints = 4;
    }
};

// 控制点式:点作控制多边形顶点
class SketchDrawSpline3DGuiCmd_ControlPoints : public SketchDrawSpline3DGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawSpline3DGuiCmd_ControlPoints, SketchDrawSpline3DGuiCmd_ControlPoints, SketchDrawSpline3DGuiCmd)
public:
    SketchDrawSpline3DGuiCmd_ControlPoints() : SketchDrawSpline3DGuiCmd()
    {
        _splineMode = wy3d::SplineMode::ControlPoints;
        _closureMinPoints = 5;
    }
};

#endif // WY3DAPP_SKETCH_DRAW_SPLINE3D_GUI_CMD_H
