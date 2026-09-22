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

#include "commands/datumPlane/TangentDatumPlnCmd.h"

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/Colors.h"
#include "select/filters/SolidFaceSelFilter.h"
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


TangentDatumPlnCmd::TangentDatumPlnCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _rotationPlane(), _radius(0.0), _angle(0.0)
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

TangentDatumPlnCmd::~TangentDatumPlnCmd()
{
}

wyap::CmdExecution::StartResult TangentDatumPlnCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectCylindricalFace);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void TangentDatumPlnCmd::cleanup()
{
    this->hidePopup();
    _step = Step::Undefined;
    _rotationPlane = wy3d::SketchPlane();
    _radius = 0.0;
    _angle = 0.0;
    _snapExcludeIds.clear();
    _pPreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _pRotationArcTransient = nullptr;
    _pMakeDatumPlane = nullptr;
    _hoverPopupState.resetValue();
}

void TangentDatumPlnCmd::reset()
{
    this->cleanup();
}

bool TangentDatumPlnCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectCylindricalFace:
    {
        // 创建基准面
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(this->computeRotationPlane(0.0)))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        assert(_snapExcludeIds.empty());
        _snapExcludeIds.insert(_pMakeDatumPlane->getId());

        // next step
        this->gotoStep(Step::SpecifyRotateAngle);
        return true;
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        if (!_pMakeDatumPlane)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 旋转基准面
        if (!_pMakeDatumPlane->update(this->computeRotationPlane(_angle)))
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

void TangentDatumPlnCmd::gotoStep(Step step)
{
    this->hidePopup();
    _hoverPopupState.resetValue();
    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectCylindricalFace:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Select a cylindrical face."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::Sheet);
        _pointPickOption.selType = wy3d::SelectionType::Face;
        _pointPickOption.acceptElement = false;
        _pointPickOption.pSelFilter = std::make_shared<FaceSelFilterFunctor<Geom_CylindricalSurface>>();
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify rotation angle."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor)
        {
            assert(_pSelSetHighlightor->getSelectionSet().getCount() == 1);
        }

        // 旋转弧线
        _pRotationArcTransient = std::make_shared<SketchArcTransient>(_rotationPlane, Colors::kEdge_Preview, 2.0);
        _pRotationArcTransient->update(wy::Vector2::kZero, _radius, 0.0, 0.0);

        //
        _angle = 0.0;

        this->simulateMouseMoveFromPopup();
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

void TangentDatumPlnCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void TangentDatumPlnCmd::onMouseMove(const MouseEvent& event)
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
    case Step::SelectCylindricalFace:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        double angle(0.0);
        if (this->computeRotationAngle(event.x, event.y, _rotationPlane, wy::Vector2::kXAxis, _snapExcludeIds, angle))
        {
            _hoverPopupState.angleSign = angle < 0.0 ? -1 : 1;
            _hoverPopupState.angle = std::fabs(angle);
            {
                if (_pMakeDatumPlane)
                {
                    _pMakeDatumPlane->update(this->computeRotationPlane(angle));
                }
                if (_pRotationArcTransient)
                {
                    _pRotationArcTransient->update(wy::Vector2::kZero, _radius, 0.0, angle);
                }
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

void TangentDatumPlnCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (Step::SpecifyRotateAngle == _step)
    {
        double angle(0.0);
        if (this->computeRotationAngle(event.x, event.y, _rotationPlane, wy::Vector2::kXAxis, _snapExcludeIds, angle))
        {
            _angle = angle;
            this->finishStep(_step);
            return;
        }
        else
        {
            assert(false);
            return;
        }
    }

    return;
}

void TangentDatumPlnCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::SelectCylindricalFace == _step)
    {
        if (!_pPreview || _pPreview->getSelection().getElementId().isNull())
        {
            return;
        }
        wyap::Selection sel = _pPreview->getSelection();
        _pPreview = nullptr;

        // 高亮选中
        _pSelSetHighlightor->clearSelections();
        _pSelSetHighlightor->addSelection(sel);

        // 通过圆柱面计算旋转面
        if (!MakeDatumPlane::getSolidCylindricalFaceCenterPlane(sel, _rotationPlane, _radius)
            || !_rotationPlane.isValid()
            || _radius <= 0.0)
        {
            assert(false);
            _pSelSetHighlightor->clearSelections();
            return;
        }

        this->finishStep(_step);
        return;
    }

    return;
}

void TangentDatumPlnCmd::initializePopups()
{
    if (_pAnglePopup)
    {
        return;
    }

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    _pAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
        QCoreApplication::translate("DatumPlnCmd", "Angle"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pAnglePopup->hide();
}

void TangentDatumPlnCmd::showPopup()
{
    if (_step != Step::SpecifyRotateAngle)
    {
        return;
    }
    if (!_pAnglePopup)
    {
        this->initializePopups();
    }
    if (!_pAnglePopup)
    {
        return;
    }

    _pAnglePopup->setValue(wy3d::radiansToDegrees(_hoverPopupState.angle));
    _pAnglePopup->showAtGlobal(QCursor::pos());
}

void TangentDatumPlnCmd::hidePopup()
{
    if (_pAnglePopup && _pAnglePopup->isVisible())
    {
        _pAnglePopup->hide();
    }
}

void TangentDatumPlnCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyRotateAngle)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pAnglePopup && _pAnglePopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void TangentDatumPlnCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyRotateAngle || !_pAnglePopup)
    {
        return;
    }

    double angle(0.0);
    if (!parseDoubleText(_pAnglePopup->getRowText(), angle))
    {
        return;
    }
    _angle = _hoverPopupState.angleSign < 0 ? -wy3d::degreesToRadians(std::fabs(angle)) : wy3d::degreesToRadians(std::fabs(angle));

    this->finishStep(_step);
}

void TangentDatumPlnCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void TangentDatumPlnCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

wy3d::SketchPlane TangentDatumPlnCmd::computeRotationPlane(double angle) const
{
    wy::Vector3 yDir = _rotationPlane.getNormal();
    wy::Vector3 origin = _rotationPlane.value(std::cos(angle) * _radius, std::sin(angle) * _radius);
    wy::Vector3 normal = origin - _rotationPlane.getOrigin();
    normal.normalize();
    wy::Vector3 xDir = yDir.cross(normal);
    return wy3d::SketchPlane(origin, normal, xDir);
}
