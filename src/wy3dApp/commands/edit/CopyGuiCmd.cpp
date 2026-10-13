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

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapClipboard.h>
#include <cmath>
#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include "CopyGuiCmd.h"
#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include "view/OsgView.h"
#include "commands/OsgGuiEventDispatcher.h"
#include <wy3dApp/ElementNode.h>
#include <wy3dApp/SketchEnvironment.h>
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "snap/SketchSnapSystem.h"
#include <wy3dApp/CommonSelFilters.h>
#include "utils/MathUtils.h"
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


CopyGuiCmd::CopyGuiCmd()
    : OsgGuiCommand(),
      _step(Step::Undefined),
      _sels(),
      _startPnt2d(0.0, 0.0),
      _startPnt(0.0, 0.0, 0.0),
      _moveVec2d(0.0, 0.0),
      _moveVec(0.0, 0.0, 0.0),
      _pSnapContext(nullptr),
      _pCopyElements(nullptr),
      _pLineTransient(nullptr),
      _pXYPopup(nullptr),
      _pLengthAnglePopup(nullptr),
      _hoverPopupState()
{
    _options.pointSelect = true;
    _options.boxSelect = true;
}

CopyGuiCmd::~CopyGuiCmd()
{
}

wyap::CmdExecution::StartResult CopyGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

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
void CopyGuiCmd::onEnd()
{
    this->hidePopup();

    // 取消复制
    if (_pCopyElements)
    {
        _pCopyElements = nullptr;
    }

    __baseClass::onEnd();

}
void CopyGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();

    // 取消移动
    if (_pCopyElements)
    {
        _pCopyElements = nullptr;
    }

    __baseClass::onAbort(cause);

}

bool CopyGuiCmd::finishStep(Step step)
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
        // 复制元素操作对象
        _pCopyElements = std::make_shared<CopyElemens>(this);
        if (!_pCopyElements->init(_sels))
        {
            assert(false);
            _pCopyElements = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 数据同步
        _startPnt = _sketchInfo.sketchPlane.value(_startPnt2d);

        // 显示移动向量线
        if (_pLineTransient)
        {
            _pLineTransient->update(_startPnt, _startPnt);
            _pLineTransient->show();
        }

        // 下一步
        this->gotoStep(Step::Step3_SpecifyEndPnt);
        return true;
    }
    break;

    case Step::Step3_SpecifyEndPnt:
    {
        // 校验
        if (!_pCopyElements)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 数据同步
        _moveVec.set(_moveVec2d.x(), _moveVec2d.y(), 0.0);
        
        // 复制元素
        if (_pCopyElements->perform(_sels, _moveVec, _sketchInfo.sketchId))
        {
            _pCopyElements->commit();
        }
        else
        {
            assert(false);
        }
        _pCopyElements = nullptr;

        // 隐藏移动向量线
        if (_pLineTransient) _pLineTransient->hide();

        // 清空捕捉结果
        Application::instance().getSnapSystem()->clearSnapResult();
        // 清空草图捕捉结果
        if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

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

void CopyGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        // 选择配置项:<1>建模环境实体 <2>草图环境草绘图元
        SelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.allowedElemTypes = ElementNodeType::SketchEntity;
        selOptions.postFilter = std::make_shared<SingleClassSelPostFilter>(wy3d::SketchEntity::classInfo());
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("CopyGuiCmd",
            "Select elements to copy; press Enter or Spacebar to confirm; press Esc to cancel."));
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
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("CopyGuiCmd",
            "Specify the base point; you can directly input the coordinate values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 局部更新草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::Step3_SpecifyEndPnt:
    {
        // 选择配置项:禁用选择
        SelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("CopyGuiCmd",
            "Specify the move vector; you can directly input the values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 草图捕捉上下文:绘制线
        _pSnapContext = std::make_shared<SketchDrawLineContext>(wydb::ElementId::kNull, _startPnt2d);
    }
    break;

    default:
    {
        assert(false);

        SelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
    }
    break;
    }
}

void CopyGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void CopyGuiCmd::onMouseMove(const MouseEvent& event)
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
        wy::Vector2 pnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point.set(pnt2d.x(), pnt2d.y(), 0.0);
    }
    else if (_step == Step::Step3_SpecifyEndPnt)
    {
        wy::Vector2 endPnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        wy::Vector2 moveVec2d = endPnt2d - _startPnt2d;
        _hoverPopupState.vector.set(moveVec2d.x(), moveVec2d.y(), 0.0);
        const double length = moveVec2d.length();
        const double angleRadian = std::atan2(moveVec2d.y(), moveVec2d.x());
        const double normalizedRadian = wy3d::normalizeRadian(angleRadian);
        const double angleDeg = wy3d::radiansToDegrees(normalizedRadian);
        {
            if (_pLineTransient) _pLineTransient->update(_sketchInfo.sketchPlane, _startPnt2d, endPnt2d);
            if (_pCopyElements) _pCopyElements->update(_sketchInfo.sketchPlane, moveVec2d);
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void CopyGuiCmd::onLeftMouseDown(const MouseEvent& event)
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
        _startPnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::Step3_SpecifyEndPnt)
    {
        wy::Vector2 endPnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _moveVec2d = endPnt2d - _startPnt2d;
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

void CopyGuiCmd::initializePopups()
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
            QCoreApplication::translate("CopyGuiCmd", "Length"),
            QCoreApplication::translate("CopyGuiCmd", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
}

void CopyGuiCmd::showPopup()
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
        _pXYPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pLengthAnglePopup) return;
        const wy::Vector2 vec2d(_hoverPopupState.vector.x(), _hoverPopupState.vector.y());
        const double length = vec2d.length();
        const double angleRadian = std::atan2(vec2d.y(), vec2d.x());
        const double normalizedRadian = wy3d::normalizeRadian(angleRadian);
        const double angleDeg = wy3d::radiansToDegrees(normalizedRadian);
        _pLengthAnglePopup->setValues(length, angleDeg);
        _pLengthAnglePopup->showAtGlobal(QCursor::pos());
    }
}

void CopyGuiCmd::hidePopup()
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

void CopyGuiCmd::tryShowPopupOnHover(double time)
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

void CopyGuiCmd::onPopupEnterKey()
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
        _startPnt2d.set(x, y);
    }
    else
    {
        if (!_pLengthAnglePopup) return;
        double length(0.0), angleDeg(0.0);
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length) ||
            !parseDoubleText(_pLengthAnglePopup->getRow2Text(), angleDeg))
        {
            return;
        }
        const double angleRad = wy3d::degreesToRadians(angleDeg);
        _moveVec2d.set(
            length * std::cos(angleRad),
            length * std::sin(angleRad));
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void CopyGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void CopyGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void CopyGuiCmd::onEnterKey()
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

void CopyGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool CopyGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void CopyGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool CopyGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void CopyGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::Step1_SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

} // namespace wy3dApp
