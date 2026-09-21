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

#include <wyVector3.h>
#include "commands/datumPlane/ParallelDatumPlnCmd.h"
#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "select/SketchPlaneSelFilter.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "widgets/frame/MainWindow.h"
#include <QCoreApplication>
#include <QCursor>
#include <cmath>

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


ParallelDatumPlnCmd::ParallelDatumPlnCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _plane(), _pickUV(), _distance(0.0)
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

ParallelDatumPlnCmd::~ParallelDatumPlnCmd()
{
}

void ParallelDatumPlnCmd::reset()
{
    this->cleanup();
}

void ParallelDatumPlnCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _plane = wy3d::SketchPlane();
    _pickUV.set(0.0, 0.0);
    _distance = 0.0;

    _pPreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _pMakeDatumPlane = nullptr;
    _hoverPopupState.resetValue();
}

wyap::CmdExecution::StartResult ParallelDatumPlnCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::DatumPlane);
    _pointPickOption.selType = wy3d::SelectionType::Face;
    _pointPickOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectDatumPlaneOrFace);

    return wyap::CmdExecution::StartResult::Succeeded;
}

bool ParallelDatumPlnCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(_plane))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        this->gotoStep(Step::SpecifyDistance);
        return true;
    }
    break;

    case Step::SpecifyDistance:
    {
        if (!_pMakeDatumPlane)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        if (!_pMakeDatumPlane->update(wy3d::SketchPlane::offset(_plane, _distance)))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeDatumPlane->commit();
        _pMakeDatumPlane = nullptr;

        // exit
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

void ParallelDatumPlnCmd::gotoStep(Step step)
{
    this->hidePopup();
    _hoverPopupState.resetValue();

    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Select datum plane or solid plane surface."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    }
    break;

    case Step::SpecifyDistance:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify through point or directly input the distance value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 预览
        _pPreview = nullptr;
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void ParallelDatumPlnCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void ParallelDatumPlnCmd::onMouseMove(const MouseEvent& event)
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
    case Step::SelectDatumPlaneOrFace:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SpecifyDistance:
    {
        double distance(0.0);
        if (this->computeHeight2(event.x, event.y, _plane, _pickUV, {}, distance))
        {
            _hoverPopupState.distanceSign = distance < 0.0 ? -1 : 1;
            _hoverPopupState.distance = std::fabs(distance);
            {
                if (_pMakeDatumPlane) _pMakeDatumPlane->update(wy3d::SketchPlane::offset(_plane, distance));
            }
            return;
        }
        else
        {
            assert(false);
            return;
        }
    }
    break;
    }

    return;
}

void ParallelDatumPlnCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyDistance:
    {
        double distance(0.0);
        if (this->computeHeight2(event.x, event.y, _plane, _pickUV, {}, distance))
        {
            _distance = distance;
            this->finishStep(_step);
            return;
        }
        else
        {
            assert(false);
            return;
        }
    }
    break;
    }

    return;
}

void ParallelDatumPlnCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        if (_pPreview && !_pPreview->getSelection().getElementId().isNull())
        {
            // 高亮选中
            wyap::Selection sel = _pPreview->getSelection();
            _pSelSetHighlightor->clearSelections();
            _pSelSetHighlightor->addSelection(sel);
            _pPreview = nullptr;

            // 获取平面
            if (!MakeDatumPlane::getSketchPlane(sel, _plane))
            {
                assert(false);
                _pSelSetHighlightor->clearSelections();
                return;
            }

            // 计算点击点坐标(因为由wyap::Selection::getPickPosition()获取的坐标不准确)
            wy::Vector3 pickPos = this->computePosition3d(event.x, event.y, _plane, {}, false).first; // false --- no snap
            _pickUV = _plane.uv(pickPos);

            // finish step
            this->finishStep(_step);
            return;
        }
    }
    break;
    }

    return;
}

void ParallelDatumPlnCmd::initializePopups()
{
    if (_pDistancePopup)
    {
        return;
    }

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    _pDistancePopup = std::make_unique<GuiCmdHoverInputPopup1>(
        QCoreApplication::translate("DatumPlnCmd", "Distance"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pDistancePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pDistancePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pDistancePopup->hide();
}

void ParallelDatumPlnCmd::showPopup()
{
    if (_step != Step::SpecifyDistance)
    {
        return;
    }
    if (!_pDistancePopup)
    {
        this->initializePopups();
    }
    if (!_pDistancePopup)
    {
        return;
    }

    _pDistancePopup->setValue(_hoverPopupState.distance);
    _pDistancePopup->showAtGlobal(QCursor::pos());
}

void ParallelDatumPlnCmd::hidePopup()
{
    if (_pDistancePopup && _pDistancePopup->isVisible())
    {
        _pDistancePopup->hide();
    }
}

void ParallelDatumPlnCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyDistance)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pDistancePopup && _pDistancePopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void ParallelDatumPlnCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyDistance || !_pDistancePopup)
    {
        return;
    }

    double distance(0.0);
    if (!parseDoubleText(_pDistancePopup->getRowText(), distance))
    {
        return;
    }
    _distance = _hoverPopupState.distanceSign < 0 ? -std::fabs(distance) : std::fabs(distance);

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void ParallelDatumPlnCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void ParallelDatumPlnCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}
