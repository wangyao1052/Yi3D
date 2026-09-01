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

#ifndef WY3DAPP_MEASURE_GUI_CMD_H
#define WY3DAPP_MEASURE_GUI_CMD_H

#include <QLabel>
#include <QList>
#include <QWidget>
#include <QLineEdit>
#include <QString>
#include <QVector>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dSketchPlane.h>
#include "commands/GuiCommandMenu.h"
#include "commands/OsgGuiCommand.h"
#include "commands/transient/BasicTransient.h"
#include "commands/utilities/MeasureCmdPanel.h"

class QMenu;
class MeasureGuiCmdControls;

class MeasureGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(MeasureGuiCmd, MeasureGuiCmd, OsgGuiCommand)
public:
    MeasureGuiCmd();

    // 清空全部测量结果(几何高亮+结果行;面板右键菜单与命令右键菜单共用)
    void clearResults();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyStartPnt = 1,
        SpecifyEndPnt = 2,
    };
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

    // 初始化控件
    virtual GuiCmdControlsSPtr initControls() override;

    // 上下文菜单
    virtual GuiCmdMenu* initContextMenu() override;

    // 测量模式
    void setMode(MeasureMode mode);
    void cycleMode();
    void applyPickOptionForMode();
    void commitMeasure(const MouseEvent& event);
    QVector<MeasureValue> measureSelectionValues(const wyap::Selection& sel);
    void refreshAccumulatedHighlight();

    // 面板
    bool createCmdPanel();
    void destroyCmdPanel();

private:
    void onResultHovered(const wyap::Selection& sel);
    void onResultRowHovered(int row);
    // 行悬停临时移出测量集的项恢复(调用方负责refresh)
    void restoreTempUnhighlightedSel();

private:
    // 点到点状态
    Step _step;
    wy3d::SketchPlane _workPln;
    wy::Vector3 _startPnt;
    wy::Vector2 _startPnt2d;
    wy::Vector3 _endPnt;
    wy::Vector2 _endPnt2d;
    SketchSnapContextSPtr _pSketchSnapContext;
    std::shared_ptr<LineTransient> _pLineTransient;
    // 点到点持久测量线(与结果行一一对应)及行悬停预览中的线
    QList<LineTransientSPtr> _pMeasureLines;
    LineTransientSPtr _pP2pHoverLine;
    MeasureGuiCmdControls* _pMeasureGuiCmdCtrls;

    // 测量状态
    MeasureMode _mode;
    PointPickOption _pointPickOption;
    SelectPreviewSPtr _pPreview;
    SelectPreviewSPtr _pRowPreview;
    SelectionSetHighlightorSPtr _pRowFacePreviewHighlightor;
    wyap::SelectionSet _measuredSet;
    SelectionSetHighlightorSPtr _pMeasuredHighlightor;
    // 行悬停时临时移出测量集的项(离行恢复;边/体的高亮守卫会挡预览)
    wyap::Selection _tempUnhighlightedSel;
    MeasureCmdPanel* _pCmdPanel;
};

class MeasureGuiCmdMenu : public GuiCmdMenu
{
    Q_OBJECT
public:
    explicit MeasureGuiCmdMenu(MeasureGuiCmd* pCmd) : GuiCmdMenu(pCmd) {}

protected:
    // 初始化客制化菜单项(清空测量结果)
    virtual bool initCustomHeaderActions(QMenu* menu) override;

private slots:
    void onClearMeasureResults();
};

class MeasureGuiCmdControls : public GuiCmdControls
{
public:
    MeasureGuiCmdControls();
    ~MeasureGuiCmdControls();

    void showLength();
    void hideLength();
    void setLength(double length);

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    GuiCmdLabel* _pLengthLabel;
};

#endif // WY3DAPP_MEASURE_GUI_CMD_H
