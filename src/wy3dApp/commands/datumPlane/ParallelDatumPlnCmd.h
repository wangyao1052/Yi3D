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

#ifndef WY3DAPP_PARALLEL_DATUM_PLN_CMD_H
#define WY3DAPP_PARALLEL_DATUM_PLN_CMD_H

#include <wyVector2.h>
#include <wyapSelection.h>
#include <wy3dPlane.h>
#include <wy3dDatumPlane.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/datumPlane/MakeDatumPlane.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCmdHoverInputPopup1;

class ParallelDatumPlnCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(ParallelDatumPlnCmd, wy3dApp::ParallelDatumPlnCmd, OsgGuiCommand)
public:
    ParallelDatumPlnCmd();
    ~ParallelDatumPlnCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectDatumPlaneOrFace = 1,
        SpecifyDistance = 2,
    };
    virtual void cleanup() override;
    virtual void reset();
    bool finishStep(Step step);
    void gotoStep(Step step);

    void onFrame(double time) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        double distance;
        int distanceSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , distance(0.0)
            , distanceSign(1)
        {}

        void resetValue()
        {
            distance = 0.0;
            distanceSign = 1;
        }
    };

    Step _step;
    wy3d::Plane _plane;
    wy::Vector2 _pickUV;
    double _distance;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;
    // 创建基准面
    std::shared_ptr<MakeDatumPlane> _pMakeDatumPlane;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pDistancePopup;
    HoverPopupState _hoverPopupState;
};

}

#endif // WY3DAPP_PARALLEL_DATUM_PLN_CMD_H
