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

#ifndef WY3DAPP_MAKE_BOX_GUI_CMD_H
#define WY3DAPP_MAKE_BOX_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wyapSelManager.h>
#include <wy3dBox.h>
#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class MakeBox;
class RectTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class MakeBoxGuiCmd : public MakePrimitiveGuiCmd
{
    WYRX_DECLARE_MEMBERS(MakeBoxGuiCmd, wy3dApp::MakeBoxGuiCmd, MakePrimitiveGuiCmd)
public:
    MakeBoxGuiCmd();
    virtual ~MakeBoxGuiCmd();

protected:
    virtual void reset();
    virtual void cleanup() override;
    virtual bool finishStep(unsigned int step) override;
    virtual void gotoStepImpl(unsigned int step) override;

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

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        wy::Vector2 point;
        double length;
        double width;
        double height;
        int lengthSign;
        int widthSign;
        int heightSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point(0.0, 0.0)
            , length(0.0)
            , width(0.0)
            , height(0.0)
            , lengthSign(1)
            , widthSign(1)
            , heightSign(1)
        {}

        void resetValue()
        {
            point.set(0.0, 0.0);
            length = 0.0;
            width = 0.0;
            height = 0.0;
            lengthSign = 1;
            widthSign = 1;
            heightSign = 1;
        }
    };

private:
    wy::Vector2 _uv1;
    wy::Vector2 _uv2;
    double _height;

    // 创建长方体
    std::shared_ptr<MakeBox> _pMakeBox;
    std::shared_ptr<RectTransient> _pRectTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthWidthPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pHeightPopup;
    HoverPopupState _hoverPopupState;
};

class MakeBox : public GuiCmdMakeElement
{
public:
    MakeBox(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pBox(nullptr) {}
    ~MakeBox() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy::Vector2& pnt1, const wy::Vector2& pnt2, double z);
    bool init(const osg::Vec3d& pnt1, const osg::Vec3d& pnt2);
    bool init(const wy3d::Plane& workPln, const wy::Vector2& uv1, const wy::Vector2& uv2);
    // 更新
    bool update(double height);

private:
    wy3d::Box* _pBox;
    wy::Vector3 _initOrigin;
    wy::Vector3 _zAxis;
};

class RectTransient : public GuiCmdTransient
{
public:
    RectTransient();
    ~RectTransient();

    void update(const osg::Vec2& pnt1, const osg::Vec2& pnt2, double z);
    void update(const osg::Vec3d& pnt1, const osg::Vec3d& pnt2);

    void update(const wy3d::Plane& workPln,
        const wy::Vector2& pnt1, const wy::Vector2& pnt2);

private:
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
};

}

#endif // WY3DAPP_MAKE_BOX_GUI_CMD_H
