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

#ifndef WY3DAPP_TANGENT_DATUM_PLN_CMD_H
#define WY3DAPP_TANGENT_DATUM_PLN_CMD_H

#include <cfloat>
#include <memory>
#include <set>

#include <wydbElementId.h>
#include <wyapSelection.h>
#include <wy3dPlane.h>
#include <wy3dDatumPlane.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/datumPlane/MakeDatumPlane.h"
#include "commands/transient/BasicTransient.h"
#include "commands/transient/SketchBasicTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCmdHoverInputPopup1;

class TangentDatumPlnCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(TangentDatumPlnCmd, wy3dApp::TangentDatumPlnCmd, OsgGuiCommand)
public:
    TangentDatumPlnCmd();
    ~TangentDatumPlnCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanUp() override;
    virtual void reset();

protected:
    enum class Step
    {
        Undefined = 0,
        SelectCylindricalFace = 1,
        SpecifyRotateAngle = 2,
    };
    virtual bool finishStep(Step step);
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
        double angle;
        int angleSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , angle(0.0)
            , angleSign(1)
        {}

        void resetValue()
        {
            angle = 0.0;
            angleSign = 1;
        }
    };

private:
    wy3d::Plane computeRotationPlane(double angle) const;

protected:
    Step _step;
    wy3d::Plane _rotationPlane;
    double _radius;
    double _angle;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;
    // 旋转弧
    SketchArcTransientSPtr _pRotationArcTransient;
    // 创建基准面
    std::shared_ptr<MakeDatumPlane> _pMakeDatumPlane;
    // 捕捉排除项
    std::set<wydb::ElementId> _snapExcludeIds;
    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup1> _pAnglePopup;
    HoverPopupState _hoverPopupState;
};

} // namespace wy3dApp

#endif // WY3DAPP_TANGENT_DATUM_PLN_CMD_H
