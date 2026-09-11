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

#ifndef WY3DAPP_SKETCH_DRAW_RECTANGLE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_RECTANGLE3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include "commands/transient/BasicTransient.h"
#include <cfloat>
#include <memory>
#include <set>
#include <utility>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dSketchPlane.h>
#include <wydbElementId.h>
#include <wy3dSketchLine3D.h>

class GuiCmdHoverInputPopup2;
class GuiCmdHoverInputPopup3;

// 3D草图矩形:4条SketchLine3D,由两角点(或中心+角点)确定,平面=工作平面
class MakeSketchRectangle3D : public GuiCmdMakeElement
{
public:
    enum class Mode
    {
        CornerRect = 0,
        CenterRect = 1,
    };

public:
    MakeSketchRectangle3D(GuiCommand* pGuiCmd, Mode mode) : GuiCmdMakeElement(pGuiCmd), _mode(mode),
        _corner1World(), _plane(), _uvOffset(), _hasExtent(false), _cornerPnt(),
        _pSketchLine1st(nullptr), _pSketchLine2nd(nullptr), _pSketchLine3rd(nullptr), _pSketchLine4th(nullptr)
    {}
    ~MakeSketchRectangle3D() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建(须先让工作平面原点落在锚点上)
    bool init(const wy::Vector3& corner1Pnt, const wy3d::SketchPlane& plane, wydb::ElementId sketch3dId);
    // 更新
    bool update(const wy::Vector3& endPnt);
    // 切换工作平面:长宽不变,绕锚点转向新平面
    bool updatePlane(const wy3d::SketchPlane& plane);

    // 最近一次生效的对角点相对锚点的平面内偏移
    bool getUVOffset(wy::Vector2& uvOffset) const;
    // 最近一次生效的四个角点(世界坐标,pnt1->pnt4依次相邻)
    bool getCorners(wy::Vector3& pnt1, wy::Vector3& pnt2, wy::Vector3& pnt3, wy::Vector3& pnt4) const;

private:
    // 由平面内坐标算出四个角点(世界坐标),并校验长宽有效
    bool buildCorners(const wy::Vector2& startUv, const wy::Vector2& endUv);
    void computeRectEndPoints(const wy::Vector2& startPnt, const wy::Vector2& endPnt,
        wy::Vector2& pnt1, wy::Vector2& pnt2, wy::Vector2& pnt3, wy::Vector2& pnt4) const;
    bool checkValid(const wy::Vector2& pnt1, const wy::Vector2& pnt2,
        const wy::Vector2& pnt3, const wy::Vector2& pnt4) const;
    // 把四个角点写进4条线(一个子事务)
    bool applyCorners();

private:
    Mode _mode;
    wy::Vector3 _corner1World;
    wy3d::SketchPlane _plane;
    wy::Vector2 _uvOffset;
    bool _hasExtent;
    wy::Vector3 _cornerPnt[4];
    wy3d::SketchLine3D* _pSketchLine1st;
    wy3d::SketchLine3D* _pSketchLine2nd;
    wy3d::SketchLine3D* _pSketchLine3rd;
    wy3d::SketchLine3D* _pSketchLine4th;
};

class SketchDrawRectangle3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawRectangle3DGuiCmd, SketchDrawRectangle3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawRectangle3DGuiCmd();
    virtual ~SketchDrawRectangle3DGuiCmd();

protected:
    enum class Step
    {
        SpecifyStartPnt = 1,
        SpecifyEndPnt = 2,
    };

    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;
    virtual void onEscapeKey() override;
    bool finishStep(unsigned int step);
    void gotoStep(unsigned int step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;
    virtual void onSpaceKey() override;

    // 状态栏提示(中心式覆写)
    virtual QString getStartTip() const;
    virtual QString getEndTip() const;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;
    std::pair<wy::Vector3, bool> computePoint3d(double x, double y);
    // 平面内偏移 -> 弹窗显示的长宽(中心式为全宽)
    void updateHoverExtents(const wy::Vector2& uvOffset);
    void updateDiagonalTransients();
    void destroyDiagonalTransients();

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
        double length;
        double width;
        bool snapped;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
            , length(0.0)
            , width(0.0)
            , snapped(false)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            length = 0.0;
            width = 0.0;
            snapped = false;
        }
    };

protected:
    MakeSketchRectangle3D::Mode _mode;

private:
    unsigned int _step;
    wy::Vector3 _startPnt;
    wy::Vector3 _endPnt;
    std::shared_ptr<MakeSketchRectangle3D> _pMakeSketchRectangle3D;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthWidthPopup;
    HoverPopupState _hoverPopupState;

    // 中心式矩形的两条对角线橡皮筋
    LineTransientSPtr _pDiagonal1st;
    LineTransientSPtr _pDiagonal2nd;
};

#endif // WY3DAPP_SKETCH_DRAW_RECTANGLE3D_GUI_CMD_H
