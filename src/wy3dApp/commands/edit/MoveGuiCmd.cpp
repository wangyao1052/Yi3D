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

#include "MoveGuiCmd.h"

#include <cmath>
#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapEnvironment.h>
#include <wyapEnvManager.h>
#include <wy3dPrimitive.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include <wy3dMove.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "view/OsgView.h"
#include "commands/OsgGuiEventDispatcher.h"
#include "scene/nodes/ElementNode.h"
#include "environments/sketch/SketchEnvironment.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "snap/SketchSnapSystem.h"
#include "select/filters/CommonSelFilters.h"
#include "select/SketchPlaneSelFilter.h"
#include "utils/GuiCommandUtil.h"
#include "snap/SnapObject.h"
#include "commands/edit/MoveRotateGuiCmdUtil.h"
#include "widgets/frame/MainWindow.h"
#include "utils/MathUtils.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


MoveGuiCmd::MoveGuiCmd()
    : OsgGuiCommand(),
      _step(Step::Undefined),
      _sels(),
      _pickWorkPlnOption(),
      _workPln(),
      _startPnt2d(0.0, 0.0),
      _startPnt(0.0, 0.0, 0.0),
      _moveVec2d(0.0, 0.0),
      _moveVec(0.0, 0.0, 0.0),
      _pSnapContext(nullptr),
      _pWorkPlnPreview(nullptr),
      _pWorkPlnOriginSnapObject(nullptr),
      _pWorkPlnSnapSystem(nullptr),
      _pMoveElements(nullptr),
      _pLineTransient(nullptr),
      _pXYPopup(nullptr),
      _pLengthAnglePopup(nullptr),
      _pXYZPopup(nullptr),
      _hoverPopupState()
{
    _options.pointSelect = true;
    _options.boxSelect = true;
}

MoveGuiCmd::~MoveGuiCmd()
{
}

wyap::CmdExecution::StartResult MoveGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 建模环境
    onStart_EnvSpecific();

    // 初始化
    _pLineTransient = std::make_shared<LineTransient>();
    _pLineTransient->hide();
    if (Application::instance().getSelManager()->getSelections().isEmpty())
    {
        this->gotoStep(Step::SelectElements);
    }
    else
    {
        this->finishStep(Step::SelectElements);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void MoveGuiCmd::onEnd()
{
    this->hidePopup();

    // 取消移动
    if (_pMoveElements)
    {
        _pMoveElements = nullptr;
    }

    __baseClass::onEnd();

    // 建模环境
    cleanupEnvSpecific();

    // 清空工作平面捕捉对象
    if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();

}
void MoveGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();

    // 取消移动
    if (_pMoveElements)
    {
        _pMoveElements = nullptr;
    }

    __baseClass::onAbort(cause);

    // 建模环境
    cleanupEnvSpecific();

    // 清空工作平面捕捉对象
    if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();
}

bool MoveGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectElements:
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
        gotoNextStepAfterSelectElements();
        return true;
    }
    break;

    case Step::SelectWorkingPlane:
    {
        // 工作平面
        if (!_pWorkPlnPreview)
        {
            assert(false);
            return false;
        }
        if (!GuiCommandUtil::getWorkingPlane(_pWorkPlnPreview->getSelection(), _workPln))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pWorkPlnPreview = nullptr;

        // 添加工作平面坐标原点为常驻捕捉对象
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        assert(pSnapSys);
        pSnapSys->beginChange();
        {
            _pWorkPlnOriginSnapObject = std::make_shared<SnapCoordinatePoint>(_workPln.getOrigin());
            pSnapSys->addResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();

        // 显示工作平面坐标系
        if (Scene* pScene = Application::instance().getActiveScene())
        {
            pScene->showSketchCSYS(_workPln);
        }
        else
        {
            assert(false);
        }

        // 工作平面捕捉系统
        _pWorkPlnSnapSystem = std::make_shared<SketchSnapSystem>(_workPln);

        // 下一步
        this->gotoStep(Step::SpecifyStartPnt);
        return true;
    }
    break;

    case Step::SpecifyStartPnt:
    {
        // 移动元素操作对象
        _pMoveElements = std::make_shared<MoveElemens>(this);
        if (!_pMoveElements->init(_sels))
        {
            assert(false);
            _pMoveElements = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 数据同步
        syncStartPntData();

        // 显示移动向量线
        if (_pLineTransient)
        {
            _pLineTransient->update(_startPnt, _startPnt);
            _pLineTransient->show();
        }

        // 下一步
        this->gotoStep(Step::SpecifyEndPnt);
        return true;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        // 校验
        if (!_pMoveElements)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 数据同步
        syncEndPntData();

        // 移动元素
        if (executeMove())
        {
            _pMoveElements->commit();
        }
        else
        {
            assert(false);
        }
        _pMoveElements = nullptr;

        // 隐藏移动向量线
        if (_pLineTransient) _pLineTransient->hide();

        // 清空捕捉结果
        Application::instance().getSnapSystem()->clearSnapResult();
        // 清空草图捕捉结果
        if (getSketchSnapSys()) getSketchSnapSys()->clearSnapResult();
        if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();

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

void MoveGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (getSketchSnapSys()) getSketchSnapSys()->clearSnapResult();
    if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();

    switch (step)
    {
    case Step::SelectElements:
    {
        // 选择配置项:<1>建模环境实体 <2>草图环境草绘图元
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        configureSelectElementOptions(selOptions);
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MoveGuiCmd",
            "Select elements to move; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectWorkingPlane:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MoveGuiCmd",
            "Select datum plane as working plane."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选配置项:选择基准面
        // 不允许选择实体面,因为若Hover到要镜像的实体特征的表面,再移开,该面会恢复到原始的颜色,但是该实体特征当前是选中态,这就冲突了.
        _pickWorkPlnOption.pickMask = static_cast<unsigned int>(/*ElementNodeType::Solid | */ElementNodeType::DatumPlane);
        _pickWorkPlnOption.selType = wy3d::SelectionType::Element;
        //_pointPickOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();
    }
    break;

    case Step::SpecifyStartPnt:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MoveGuiCmd",
            "Specify the base point; you can directly input the coordinate values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 局部更新草图捕捉系统
        if (getSketchSnapSys())
        {
            getSketchSnapSys()->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyEndPnt:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MoveGuiCmd",
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

        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
    }
    break;
    }
}

void MoveGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void MoveGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::SelectElements:
    {
        return;
    }
    break;

    case Step::SelectWorkingPlane:
    {
        wyap::Selection sel = this->pointPick(event.x, event.y, _pickWorkPlnOption);
        if (_pWorkPlnPreview)
        {
            if (sel.getElementId().isNull())
            {
                _pWorkPlnPreview = nullptr;
            }
            else
            {
                if (!_pWorkPlnPreview->isEqual(sel))
                {
                    _pWorkPlnPreview = std::make_shared<SelectPreview>(sel);
                }
            }
        }
        else
        {
            if (!sel.getElementId().isNull())
            {
                _pWorkPlnPreview = std::make_shared<SelectPreview>(sel);
            }
        }

        return;
    }
    break;

    case Step::SpecifyStartPnt:
    {
        onMouseMove_SpecifyStartPnt(event.x, event.y);
        return;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        onMouseMove_SpecifyEndPnt(event.x, event.y);
        return;
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }

    return;
}

void MoveGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SelectElements:
    {
        return;
    }
    break;

    case Step::SelectWorkingPlane:
    {
        if (_pWorkPlnPreview)
        {
            this->finishStep(_step);
        }

        return;
    }
    break;

    case Step::SpecifyStartPnt:
    {
        onLeftMouseDown_SpecifyStartPnt(event.x, event.y);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        onLeftMouseDown_SpecifyEndPnt(event.x, event.y);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }

    return;
}

void MoveGuiCmd::initializePopups()
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
            QCoreApplication::translate("MoveGuiCmd", "Length"),
            QCoreApplication::translate("MoveGuiCmd", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
    if (!_pXYZPopup)
    {
        _pXYZPopup = std::make_unique<GuiCmdHoverInputPopup3>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("Z"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pXYZPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pXYZPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pXYZPopup->hide();
    }
}

void MoveGuiCmd::showPopup()
{
    if (_step != Step::SpecifyStartPnt && _step != Step::SpecifyEndPnt)
    {
        return;
    }
    if (!_pXYPopup || !_pLengthAnglePopup || !_pXYZPopup)
    {
        this->initializePopups();
    }

    showCoordinatePopup();
}

void MoveGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthAnglePopup && _pLengthAnglePopup->isVisible())
    {
        _pLengthAnglePopup->hide();
    }
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
}

void MoveGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyStartPnt && _step != Step::SpecifyEndPnt)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()) ||
        (_pXYZPopup && _pXYZPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void MoveGuiCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyStartPnt && _step != Step::SpecifyEndPnt)
    {
        return;
    }

    handlePopupEnterKey();

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void MoveGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void MoveGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MoveGuiCmd::onEnterKey()
{
    if (Step::SelectElements == _step)
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

void MoveGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool MoveGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectElements == _step;
}

void MoveGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool MoveGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectElements == _step;
}

void MoveGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}
