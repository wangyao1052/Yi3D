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

#ifndef WY3DAPP_SKETCH_OFFSET_GUI_CMD_H
#define WY3DAPP_SKETCH_OFFSET_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

#include "commands/transient/SketchCurveTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class OffsetSketchCurve;
class GuiCmdHoverInputPopup1;

class SketchOffsetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchOffsetGuiCmd, wy3dApp::SketchOffsetGuiCmd, OsgGuiCommand)
public:
    SketchOffsetGuiCmd();
    virtual ~SketchOffsetGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectElement = 1,
        SpecifyOffset = 2,
    };
    virtual void reset();
    virtual void cleanup() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    void onFrame(double time) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

    void previewCurve(const wydb::ElementId& id);
    const wy3d::SketchCurve* getSketchCurve(const wydb::ElementId& id);
    std::set<wydb::ElementId> getSnapExcludeIds() const;

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        double offset;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , offset(0.0)
        {}

        void resetValue()
        {
            offset = 0.0;
        }
    };

private:
    Step _step;
    wydb::ElementId _id;
    double _offset;

    // 点选选项
    PointPickOption _pointPickOption;

    SketchSnapContextSPtr _pSnapContext;
    std::shared_ptr<SketchCurveTransient> _pCurve;
    std::shared_ptr<OffsetSketchCurve> _pOffsetCurve;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pOffsetPopup;
    HoverPopupState _hoverPopupState;
};

class OffsetSketchCurve : public GuiCmdMakeElement
{
public:
    OffsetSketchCurve(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd) {}
    ~OffsetSketchCurve() {}

    // 创建
    virtual bool init(const wy3d::SketchCurve* pSketchCurve) = 0;
    // 计算偏移
    virtual double computeOffset(const wy::Vector2& pos) = 0;
    // 更新
    virtual bool update(double offset) = 0;
};

class OffsetSketchLine : public OffsetSketchCurve
{
public:
    OffsetSketchLine(GuiCommand* pGuiCmd);

    // 创建
    virtual bool init(const wy3d::SketchCurve* pSketchCurve) override;
    // 计算偏移
    virtual double computeOffset(const wy::Vector2& pos) override;
    // 更新
    virtual bool update(double offset) override;
    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

private:
    wy::Vector2 _startPnt;
    wy::Vector2 _endPnt;
    wy::Vector2 _lineDir;
    wy::Vector2 _offsetDir;
    wy3d::SketchLine* _pSketchLine;
};

class OffsetSketchCenterLine : public OffsetSketchCurve
{
public:
    OffsetSketchCenterLine(GuiCommand* pGuiCmd);

    // 创建
    virtual bool init(const wy3d::SketchCurve* pSketchCurve) override;
    // 计算偏移
    virtual double computeOffset(const wy::Vector2& pos) override;
    // 更新
    virtual bool update(double offset) override;
    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

private:
    wy::Vector2 _startPnt;
    wy::Vector2 _endPnt;
    wy::Vector2 _lineDir;
    wy::Vector2 _offsetDir;
    wy3d::SketchCenterLine* _pSketchCenterLine;
};

class OffsetSketchCircle : public OffsetSketchCurve
{
public:
    OffsetSketchCircle(GuiCommand* pGuiCmd);

    // 创建
    virtual bool init(const wy3d::SketchCurve* pSketchCurve) override;
    // 计算偏移
    virtual double computeOffset(const wy::Vector2& pos) override;
    // 更新
    virtual bool update(double offset) override;
    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

private:
    wy::Vector2 _center;
    double _radius;
    bool _orient; // true --- 表示向外偏移
    wy3d::SketchCircle* _pSketchCircle;
};

class OffsetSketchArc : public OffsetSketchCurve
{
public:
    OffsetSketchArc(GuiCommand* pGuiCmd);

    // 创建
    virtual bool init(const wy3d::SketchCurve* pSketchCurve) override;
    // 计算偏移
    virtual double computeOffset(const wy::Vector2& pos) override;
    // 更新
    virtual bool update(double offset) override;
    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

private:
    wy::Vector2 _center;
    double _radius;
    double _startAngle;
    double _endAngle;
    bool _orient; // true --- 表示向外偏移
    wy3d::SketchArc* _pSketchArc;
};

}

#endif // WY3DAPP_SKETCH_OFFSET_GUI_CMD_H
