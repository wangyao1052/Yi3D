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

#ifndef WY3DAPP_SKETCH_POLAR_ARRAY_GUI_CMD_H
#define WY3DAPP_SKETCH_POLAR_ARRAY_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <vector>
#include <set>
#include <memory>
#include <osg/MatrixTransform>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wyapSelection.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchPolarArrayElements;

class SketchPolarArrayGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchPolarArrayGuiCmd, wy3dApp::SketchPolarArrayGuiCmd, OsgGuiCommand)
public:
    SketchPolarArrayGuiCmd();
    virtual ~SketchPolarArrayGuiCmd();

    static bool isValidTotalAngleAndCount(double totalAngle, unsigned int count);

    // 最小元素数量
    static inline const unsigned int MIN_COUNT = 2;
    // 最大元素数量
    static inline const unsigned int MAX_COUNT = 10000;

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        Step1_SelectElements = 1,
        Step2_SpecifyCenter = 2,
        Step3_SpecifyItems = 3
    };
    virtual void reset();
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    // Enter键响应
    virtual void onEnterKey() override;
    // Space键响应
    virtual void onSpaceKey() override;

    virtual void initializeSelectOptions(SelectOptions& selectOptions) override;

    // 上下文菜单
    virtual void configureContextMenu(ContextMenu& menu) override;
    virtual void onCompleteSelectionMenuItem() override;
    virtual void onClearSelectionMenuItem() override;

private:
    Step _step;
    std::set<wydb::ElementId> _ids;
    wy::Vector2 _center;
    double _totalAngle;
    unsigned int _count;
    bool _isCCW;
    Snap2dContextSPtr _pSnapContext;

    std::shared_ptr<SketchPolarArrayElements> _pPolarArray;
};

class SketchPolarArrayElements : public GuiCmdMakeElement
{
public:
    SketchPolarArrayElements(GuiCommand* pGuiCmd, const wy3d::Plane& sketchPlane, wydb::ElementId sketchId);
    ~SketchPolarArrayElements();

    bool init(const std::set<wydb::ElementId>& ids, const wy::Vector2& center,
        double totalAngle, unsigned int count, bool isCCW = true);
    bool perform(const std::set<wydb::ElementId>& ids, const wy::Vector2& center,
        double totalAngle, unsigned int count, bool isCCW = true);

private:
    std::vector<osg::ref_ptr<osg::MatrixTransform>> _items;
    wy3d::Plane _sketchPlane;
    wydb::ElementId _sketchId;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLAR_ARRAY_GUI_CMD_H
