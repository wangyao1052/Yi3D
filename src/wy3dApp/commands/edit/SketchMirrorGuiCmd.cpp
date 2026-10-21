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

#include "SketchMirrorGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapEnvironment.h>
#include <wyapEnvManager.h>
#include <wyapClipboard.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include "view/OsgView.h"
#include "commands/OsgGuiEventDispatcher.h"
#include <wy3dApp/ElementNode.h>
#include <wy3dApp/SketchEnvironment.h>
#include "snap/SketchSnapSystem.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "utils/MathUtils.h"
#include <wy3dApp/CommonSelFilters.h>
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


SketchMirrorGuiCmd::SketchMirrorGuiCmd()
    : OsgGuiCommand(),
      _step(Step::Undefined),
      _sels(),
      _axisStartPnt(),
      _axisEndPnt(),
      _pSnapContext(nullptr),
      _pMirrorElements(nullptr),
      _pLineTransient(nullptr),
      _pXYPopup(nullptr),
      _pLengthAnglePopup(nullptr),
      _hoverPopupState()
{
}

void SketchMirrorGuiCmd::initializeSelectOptions(SelectOptions& selectOptions)
{
    selectOptions.pointSelect = true;
    selectOptions.boxSelect = true;
}

SketchMirrorGuiCmd::~SketchMirrorGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchMirrorGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    _sketchInfo = GuiCommandUtil::initSketchInfo();

    // 初始化
    _pLineTransient = std::make_shared<LineTransient>();
    _pLineTransient->hide();
    if (Application::instance().getSelManager()->getSelections().isEmpty())
    {
        this->gotoStep(Step::Step1_SelectElements);
    }
    else
    {
        this->finishStep(Step::Step1_SelectElements);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchMirrorGuiCmd::onEnd()
{
    this->hidePopup();

    // 取消移动
    if (_pMirrorElements)
    {
        _pMirrorElements = nullptr;
    }

    __baseClass::onEnd();

}
void SketchMirrorGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();

    // 取消移动
    if (_pMirrorElements)
    {
        _pMirrorElements = nullptr;
    }

    __baseClass::onAbort(cause);

}

void SketchMirrorGuiCmd::reset()
{
    this->hidePopup();

    if (_pMirrorElements) _pMirrorElements = nullptr;
    if (_pLineTransient) _pLineTransient->hide();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    _step = Step::Undefined;
    _sels.clear();
    _axisStartPnt.set(0.0, 0.0);
    _axisEndPnt.set(0.0, 0.0);
    _pSnapContext = nullptr;

    this->gotoStep(Step::Step1_SelectElements);
}

bool SketchMirrorGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        // 选择集
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (ss.isEmpty())
        {
            assert(false);
            return false;
        }
        _sels = ss;

        // 下一步
        this->gotoStep(Step::Step2_SpecifyStartPnt);
        return true;
    }
    break;

    case Step::Step2_SpecifyStartPnt:
    {
        // 镜像元素操作对象
        _pMirrorElements = std::make_shared<MirrorElemens>(this);
        if (!_pMirrorElements->init(_sels))
        {
            assert(false);
            _pMirrorElements = nullptr;
            this->reset();
            return false;
        }

        // 显示镜像轴线
        if (_pLineTransient)
        {
            _pLineTransient->update(_sketchInfo.sketchPlane, _axisStartPnt, _axisEndPnt);
            _pLineTransient->show();
        }

        // 下一步
        this->gotoStep(Step::Step3_SpecifyEndPnt);
        return true;
    }
    break;

    case Step::Step3_SpecifyEndPnt:
    {
        if (_pMirrorElements)
        {
            if (!_pMirrorElements->perform(_sels, _sketchInfo.sketchId, _axisStartPnt, _axisEndPnt))
            {
                assert(false);
                _pMirrorElements = nullptr;
                this->reset();
                return false;
            }
            _pMirrorElements->commit();
            _pMirrorElements = nullptr;
        }

        if (_pLineTransient) _pLineTransient->hide();

        // 清空捕捉结果
        Application::instance().getSnapSystem()->clearSnapResult();

        // 结束
        this->requestEnd();
        return true;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void SketchMirrorGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (_sketchInfo.pSketchSnapSys)
    {
        _sketchInfo.pSketchSnapSys->clearSnapResult();
    }

    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        // 选择配置项:草图环境草绘图元
        SelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.allowedElemTypes = ElementNodeType::SketchEntity;
        selOptions.postFilter = SingleClassSelPostFilter::create<wy3d::SketchEntity>();
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Add;
        this->applySelectOptions(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Mirror",
            "Select elements to mirror; press Enter or Spacebar to confirm; press Esc to cancel."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::Step2_SpecifyStartPnt:
    {
        // 选择配置项:禁用选择
        SelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->applySelectOptions(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Mirror",
            "Specify the start point; you can directly input the coordinate values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->clearSnapResult();
        }
        _pSnapContext = std::make_shared<Locate2dContext>();
    }
    break;

    case Step::Step3_SpecifyEndPnt:
    {
        SelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        selOptions.selectionType = wy3d::SelectionType::Element;
        this->applySelectOptions(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Mirror",
            "Specify the end point; you can directly input the values."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<DrawLine2dContext>(_axisStartPnt);
    }
    break;

    default:
    {
        SelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        selOptions.selectionType = wy3d::SelectionType::Element;
        this->applySelectOptions(selOptions);

        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void SketchMirrorGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchMirrorGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::Step1_SelectElements)
    {
    }
    else if (_step == Step::Step2_SpecifyStartPnt)
    {
        wy::Vector2 axisStartPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext);
        _hoverPopupState.point = axisStartPnt;
    }
    else if (_step == Step::Step3_SpecifyEndPnt)
    {
        wy::Vector2 axisEndPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext);
        _hoverPopupState.point = axisEndPnt;
        auto lengthAngle = MathUtils::computeLengthAngle(_axisStartPnt, axisEndPnt);
        _hoverPopupState.length = lengthAngle.first;
        _hoverPopupState.angleDeg = wy3d::radiansToDegrees(lengthAngle.second);
        {
            wy::Vector3 startPnt3d = _sketchInfo.sketchPlane.value(_axisStartPnt);
            wy::Vector3 endPnt3d = _sketchInfo.sketchPlane.value(axisEndPnt);
            if (_pLineTransient) _pLineTransient->update(startPnt3d, endPnt3d);
            if (_pMirrorElements) _pMirrorElements->update(_sketchInfo.sketchPlane, _axisStartPnt, axisEndPnt);
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchMirrorGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::Step1_SelectElements)
    {
    }
    else if (_step == Step::Step2_SpecifyStartPnt)
    {
        _axisStartPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::Step3_SpecifyEndPnt)
    {
        _axisEndPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchMirrorGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pXYPopup)
    {
        _pXYPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pXYPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pXYPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pXYPopup->hide();
    }
    if (!_pLengthAnglePopup)
    {
        _pLengthAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("Mirror", "Length"),
            QCoreApplication::translate("Mirror", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
}

void SketchMirrorGuiCmd::showPopup()
{
    if (_step != Step::Step2_SpecifyStartPnt && _step != Step::Step3_SpecifyEndPnt)
    {
        return;
    }
    if (!_pXYPopup || !_pLengthAnglePopup)
    {
        this->initializePopups();
    }

    if (_step == Step::Step2_SpecifyStartPnt)
    {
        if (!_pXYPopup) return;
        _pXYPopup->setValues(_hoverPopupState.point.x(), _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pLengthAnglePopup) return;
        _pLengthAnglePopup->setValues(_hoverPopupState.length, _hoverPopupState.angleDeg);
        _pLengthAnglePopup->showAtGlobal(QCursor::pos());
    }
}

void SketchMirrorGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthAnglePopup && _pLengthAnglePopup->isVisible())
    {
        _pLengthAnglePopup->hide();
    }
}

void SketchMirrorGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::Step2_SpecifyStartPnt && _step != Step::Step3_SpecifyEndPnt)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void SketchMirrorGuiCmd::onPopupEnterKey()
{
    if (_step != Step::Step2_SpecifyStartPnt && _step != Step::Step3_SpecifyEndPnt)
    {
        return;
    }

    if (_step == Step::Step2_SpecifyStartPnt)
    {
        if (!_pXYPopup) return;
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }
        _axisStartPnt.set(x, y);
    }
    else
    {
        if (!_pLengthAnglePopup) return;
        double length(0.0);
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length))
        {
            return;
        }

        QString angleText = _pLengthAnglePopup->getRow2Text().trimmed();
        double angleDeg(_hoverPopupState.angleDeg);
        if (!angleText.isEmpty() && !parseDoubleText(angleText, angleDeg))
        {
            return;
        }
        const double angleRad = wy3d::degreesToRadians(angleDeg);
        _axisEndPnt = _axisStartPnt + length * wy::Vector2(std::cos(angleRad), std::sin(angleRad));
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchMirrorGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchMirrorGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void SketchMirrorGuiCmd::onEnterKey()
{
    if (Step::Step1_SelectElements == _step)
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (!ss.isEmpty())
        {
            if (this->finishStep(_step))
            {
                this->simulateMouseMoveFromPopup();
            }
        }
    }
}

void SketchMirrorGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

void SketchMirrorGuiCmd::configureContextMenu(ContextMenu& menu)
{
    menu.setCompleteSelectionVisible(Step::Step1_SelectElements == _step);
    menu.setClearSelectionVisible(Step::Step1_SelectElements == _step);
}

void SketchMirrorGuiCmd::onCompleteSelectionMenuItem()
{
    this->onEnterKey();
}

void SketchMirrorGuiCmd::onClearSelectionMenuItem()
{
    if (Step::Step1_SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

} // namespace wy3dApp
