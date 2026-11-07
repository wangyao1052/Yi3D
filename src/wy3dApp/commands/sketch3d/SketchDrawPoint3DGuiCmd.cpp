///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchDrawPoint3DGuiCmd.h"

#include <cassert>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchPoint3D.h>
#include <wy3dImpl.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "commands/OsgCoordUtil.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "snap3d/Sketch3DSnapContext.h"
#include "snap3d/Sketch3DSnapSystem.h"
#include <wy3dApp/GuiCommandUtil.h>
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawPoint3DGuiCmd::SketchDrawPoint3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(Step::Undefined), _position(), _pSnapContext(nullptr),
    _pXYZPopup(nullptr), _hoverPopupState()
{
}

SketchDrawPoint3DGuiCmd::~SketchDrawPoint3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawPoint3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(Step::SpecifyPosition);
    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawPoint3DGuiCmd::cleanUp()
{
    _step = Step::Undefined;
    _position.set(0.0, 0.0, 0.0);
    _pSnapContext = nullptr;

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawPoint3DGuiCmd::onEscapeKey()
{
    this->hidePopup();
    this->postAbortRequest(AbortCause::UserCancel);
}

bool SketchDrawPoint3DGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyPosition:
    {
        std::shared_ptr<MakeSketchPoint3D> pMakeSketchPoint3D = std::make_shared<MakeSketchPoint3D>(this);
        if (pMakeSketchPoint3D->perform(_position, _sketch3DInfo.sketch3dId))
        {
            pMakeSketchPoint3D->commit();
            pMakeSketchPoint3D = nullptr;

            // 下一步(循环)
            this->gotoStep(Step::SpecifyPosition);
            return true;
        }
        else
        {
            assert(false);
            pMakeSketchPoint3D = nullptr;

            // 退出命令
            this->postAbortRequest(AbortCause::ErrorTerminate);
            return false;
        }
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

void SketchDrawPoint3DGuiCmd::gotoStep(Step step)
{
    _step = step;

    this->hidePopup();
    _hoverPopupState.resetValue();

    // 步骤切换时清除残留的捕捉图标
    if (Sketch3DSnapSystem* pSnapSys = this->getSketch3DSnapSystem())
    {
        pSnapSys->clearSnapResult();
    }

    switch (step)
    {
    case Step::SpecifyPosition:
    {
        ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawPoint3DGuiCmd",
            "Specify the position of the point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
        ApplicationImpl::instance().setCursor(CursorType::Locate);
        _pSnapContext = std::make_shared<Sketch3DLocateContext>();
    }
    break;

    default:
    {
        ApplicationImpl::instance().getStatusBar()->setTips("");
        ApplicationImpl::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void SketchDrawPoint3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawPoint3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SpecifyPosition)
    {
        std::pair<wy::Vector3, bool> ret = this->computePoint3d(event.x, event.y);
        _hoverPopupState.point = ret.first;
        _hoverPopupState.snapped = ret.second;
    }
}

void SketchDrawPoint3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    if (_step == Step::SpecifyPosition)
    {
        _position = this->computePoint3d(event.x, event.y).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    else
    {
        assert(false);
    }
}

void SketchDrawPoint3DGuiCmd::initializePopups()
{
    if (_pXYZPopup)
    {
        return;
    }

    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
    if (!pMainWindow)
    {
        assert(false);
        return;
    }

    _pXYZPopup = std::make_unique<GuiCmdHoverInputPopup3>(
        QStringLiteral("X"),
        QStringLiteral("Y"),
        QStringLiteral("Z"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pXYZPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pXYZPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pXYZPopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
    _pXYZPopup->hide();
}

void SketchDrawPoint3DGuiCmd::showPopup()
{
    if (!_pXYZPopup)
    {
        this->initializePopups();
    }
    if (!_pXYZPopup)
    {
        return;
    }
    if (_step != Step::SpecifyPosition)
    {
        return;
    }

    _pXYZPopup->setValues(
        _hoverPopupState.point.x(),
        _hoverPopupState.point.y(),
        _hoverPopupState.point.z());
    _pXYZPopup->showAtGlobal(QCursor::pos());
}

void SketchDrawPoint3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
}

void SketchDrawPoint3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPosition)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    Q_UNUSED(time);
    this->showPopup();
}

void SketchDrawPoint3DGuiCmd::onPopupEnterKey()
{
    if (!_pXYZPopup)
    {
        return;
    }

    double x(0.0);
    double y(0.0);
    double z(0.0);
    if (!parseDoubleText(_pXYZPopup->getRow1Text(), x) ||
        !parseDoubleText(_pXYZPopup->getRow2Text(), y) ||
        !parseDoubleText(_pXYZPopup->getRow3Text(), z))
    {
        return;
    }

    _position.set(x, y, z);
    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawPoint3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawPoint3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
    this->simulateMouseMoveFromPopup();
}

void SketchDrawPoint3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX) return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawPoint3DGuiCmd::computePoint3d(double x, double y)
{
    // 统一入口:全局点捕捉优先,未命中走3D草图体系
    return OsgCoordUtil::computePosition3dForSketch3D(this->getOsgView(), x, y,
        this->getWorkingPlane(), this->getSnapExcludeIds(), _pSnapContext.get(),
        this->getSketch3DSnapSystem(),
        _sketch3DInfo.sketch3dId);
}

std::set<wydb::ElementId> SketchDrawPoint3DGuiCmd::getSnapExcludeIds() const
{
    // 点没有预览实体,无需排除
    return std::set<wydb::ElementId>();
}

void MakeSketchPoint3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchPoint3D) idSet.insert(_pSketchPoint3D->getId());
}

bool MakeSketchPoint3D::perform(const wy::Vector3& position, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchPoint3D || _isFinished)
    {
        return false;
    }

    // 创建3D草绘点
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchPoint3D* pSketchPoint3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchPoint3D::create(pTrans, position, pSketchPoint3D) || !pSketchPoint3D)
    {
        goto ABORT_TRANS;
    }
    _pSketchPoint3D = pSketchPoint3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchPoint3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchPoint3D = nullptr;
    return false;
}

} // namespace wy3dApp
