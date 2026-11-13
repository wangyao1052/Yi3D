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

#include "SketchDrawStraightSlotGuiCmd.h"

#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <wy3dSketch.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchLine.h>
#include <wy3dApp/Application.h>
#include <wy3dApp/GuiCmdHoverInputPopup.h>
#include <wy3dApp/Snap2dSystem.h>

#include "application/ApplicationImpl.h"
#include "commands/transient/SlotGuideTransient.h"
#include "utils/SketchSlotGeometry.h"
#include "widgets/CursorType.h"
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/GuiCommandUtil.h>

namespace wy3dApp {

namespace {

constexpr double kMinDistance = 1.0e-6;
// wy3d::kMinValue is 0.001, so a width smaller than 0.002 would produce an
// invalid arc radius. Use the smallest valid width for the live entities
// created after the second click; it is replaced as soon as the mouse moves.
const double kMinWidth = 2.0 * wy3d::kMinValue;
const double kProvisionalWidth = kMinWidth;

bool parseDoubleText(const QString& text, double& value)
{
    bool ok = false;
    value = text.trimmed().toDouble(&ok);
    return ok;
}

double distance(const wy::Vector2& lhs, const wy::Vector2& rhs)
{
    return std::hypot(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

SlotPoint toSlotPoint(const wy::Vector2& point)
{
    SlotPoint result;
    result.x = point.x();
    result.y = point.y();
    return result;
}

} // namespace

SketchDrawStraightSlotGuiCmd::SketchDrawStraightSlotGuiCmd()
    : OsgGuiCommand()
{
}

SketchDrawStraightSlotGuiCmd::~SketchDrawStraightSlotGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawStraightSlotGuiCmd::onStart()
{
    const wyap::CmdExecution::StartResult result = GuiCommand::onStart();
    if (result != wyap::CmdExecution::StartResult::Succeeded)
    {
        return result;
    }

    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.sketchId.isNull() || _sketchInfo.pSnap2dSys == nullptr)
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }

    Application* pApp = getApplication();
    if (pApp)
    {
        pApp->setCursor(CursorType::Locate);
    }

    _guide = std::make_unique<SlotGuideTransient>(_sketchInfo.sketchPlane);
    this->initializePopups();
    this->resetToFirstCenter();

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawStraightSlotGuiCmd::cleanUp()
{
    this->hidePopups();
    this->abortSlotPreview();

    ApplicationImpl::instance().setStatusBarTip(QString());

    _pXYPopup.reset();
    _pLengthAnglePopup.reset();
    _pWidthPopup.reset();
    _guide.reset();
    _snapContext.reset();
    _step = Step::FirstCenter;
    _firstCenter = wy::Vector2();
    _secondCenter = wy::Vector2();
    _currentWidth = 0.0;
    _hover = HoverPopupState();
}

void SketchDrawStraightSlotGuiCmd::resetToFirstCenter()
{
    this->hidePopups();
    this->abortSlotPreview();

    _step = Step::FirstCenter;
    _firstCenter = wy::Vector2();
    _secondCenter = wy::Vector2();
    _currentWidth = 0.0;
    _hover.resetValue();
    _snapContext = std::make_shared<Locate2dContext>();

    if (_guide)
    {
        _guide->hideLine();
    }
    if (_sketchInfo.pSnap2dSys)
    {
        _sketchInfo.pSnap2dSys->clearSnapResult();
    }

    this->updateStatusTip();
}

void SketchDrawStraightSlotGuiCmd::updateStatusTip()
{
    switch (_step)
    {
    case Step::FirstCenter:
        ApplicationImpl::instance().setStatusBarTip(QCoreApplication::translate(
            "SketchStraightSlot",
            "Specify the first slot center."));
        break;

    case Step::SecondCenter:
        ApplicationImpl::instance().setStatusBarTip(QCoreApplication::translate(
            "SketchStraightSlot",
            "Specify the second slot center."));
        break;

    case Step::Width:
    {
        const QString format = QCoreApplication::translate(
            "SketchStraightSlot",
            "Slot width: %1. Move and click to finish.");
        ApplicationImpl::instance().setStatusBarTip(format.arg(_currentWidth, 0, 'f', 3));
        break;
    }
    }
}

wy::Vector2 SketchDrawStraightSlotGuiCmd::computePoint(const MouseEvent& event)
{
    if (!_sketchInfo.pSnap2dSys || !_snapContext)
    {
        return wy::Vector2();
    }

    return this->computePoint2d(
        event.x,
        event.y,
        _sketchInfo.pSnap2dSys,
        _snapContext,
        this->getSnapExcludeIds()).first;
}

std::set<wydb::ElementId> SketchDrawStraightSlotGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> ids;
    if (!_sketchInfo.sketchId.isNull())
    {
        ids.insert(_sketchInfo.sketchId);
    }
    if (_slotElements.isValid())
    {
        ids.insert(_slotElements.rightArc);
        ids.insert(_slotElements.upperLine);
        ids.insert(_slotElements.leftArc);
        ids.insert(_slotElements.lowerLine);
    }
    return ids;
}

double SketchDrawStraightSlotGuiCmd::computeWidth(const wy::Vector2& cursor) const
{
    const double deltaX = _secondCenter.x() - _firstCenter.x();
    const double deltaY = _secondCenter.y() - _firstCenter.y();
    const double centerDistance = std::hypot(deltaX, deltaY);
    if (centerDistance < kMinDistance)
    {
        return 0.0;
    }

    // Distance from the cursor to the centerline, doubled for the full width.
    const double cross = deltaX * (cursor.y() - _firstCenter.y())
        - deltaY * (cursor.x() - _firstCenter.x());
    return 2.0 * std::fabs(cross) / centerDistance;
}

bool SketchDrawStraightSlotGuiCmd::startSlotPreview(double width)
{
    if (_pTransGroup || _slotElements.isValid())
    {
        return false;
    }

    Application* pApp = getApplication();
    wydb::Database* pDb = pApp ? pApp->getActiveDatabase() : nullptr;
    if (!pDb)
    {
        return false;
    }

    SlotGeometry slot;
    if (!SlotGeometry::compute(
        toSlotPoint(_firstCenter),
        toSlotPoint(_secondCenter),
        width,
        slot))
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    _pTransGroup = pTransMgr->startTransactionGroup("Straight Slot");
    if (!_pTransGroup)
    {
        return false;
    }

    wydb::TransactionOption option;
    option = GuiCommandUtil::makeSketchTransactionOption();
    wydb::Transaction* pTrans = pTransMgr->startTransaction("", option);
    if (!pTrans)
    {
        pTransMgr->abortTransaction();
        _pTransGroup = nullptr;
        return false;
    }

    bool success = false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(
        pTrans->getElementForWrite(_sketchInfo.sketchId));

    if (pSketch)
    {
        success = true;

        wy3d::SketchArc* pArc = nullptr;
        success = success
            && wy::ErrorStatus::Ok == wy3d::SketchArc::create(
                pTrans,
                wy::Vector2(slot.rightArc.center.x, slot.rightArc.center.y),
                slot.rightArc.radius,
                slot.rightArc.startAngle,
                slot.rightArc.endAngle,
                pArc)
            && pArc != nullptr
            && wy::ErrorStatus::Ok == pSketch->addEntity(pArc);
        if (success)
        {
            _slotElements.rightArc = pArc->getId();
        }

        wy3d::SketchLine* pLine = nullptr;
        success = success
            && wy::ErrorStatus::Ok == wy3d::SketchLine::create(
                pTrans,
                wy::Vector2(slot.upperStart.x, slot.upperStart.y),
                wy::Vector2(slot.upperEnd.x, slot.upperEnd.y),
                pLine)
            && pLine != nullptr
            && wy::ErrorStatus::Ok == pSketch->addEntity(pLine);
        if (success)
        {
            _slotElements.upperLine = pLine->getId();
        }

        success = success
            && wy::ErrorStatus::Ok == wy3d::SketchArc::create(
                pTrans,
                wy::Vector2(slot.leftArc.center.x, slot.leftArc.center.y),
                slot.leftArc.radius,
                slot.leftArc.startAngle,
                slot.leftArc.endAngle,
                pArc)
            && pArc != nullptr
            && wy::ErrorStatus::Ok == pSketch->addEntity(pArc);
        if (success)
        {
            _slotElements.leftArc = pArc->getId();
        }

        success = success
            && wy::ErrorStatus::Ok == wy3d::SketchLine::create(
                pTrans,
                wy::Vector2(slot.lowerStart.x, slot.lowerStart.y),
                wy::Vector2(slot.lowerEnd.x, slot.lowerEnd.y),
                pLine)
            && pLine != nullptr
            && wy::ErrorStatus::Ok == pSketch->addEntity(pLine);
        if (success)
        {
            _slotElements.lowerLine = pLine->getId();
        }
    }

    if (!success)
    {
        // Abort the child transaction first, then the group. Ignore errors:
        // if the child abort already unwound the group, the second call is a
        // harmless no-op.
        pTransMgr->abortTransaction();
        pTransMgr->abortTransaction();
        _pTransGroup = nullptr;
        _slotElements.reset();
        return false;
    }

    if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
    {
        // Abort the child (if still active) and then the group.
        pTransMgr->abortTransaction();
        pTransMgr->abortTransaction();
        _pTransGroup = nullptr;
        _slotElements.reset();
        return false;
    }

    return true;
}

bool SketchDrawStraightSlotGuiCmd::updateSlotPreview(double width)
{
    if (!_pTransGroup || !_slotElements.isValid())
    {
        return false;
    }

    SlotGeometry slot;
    if (!SlotGeometry::compute(
        toSlotPoint(_firstCenter),
        toSlotPoint(_secondCenter),
        width,
        slot))
    {
        return false;
    }

    Application* pApp = getApplication();
    wydb::Database* pDb = pApp ? pApp->getActiveDatabase() : nullptr;
    if (!pDb)
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    wydb::TransactionOption option;
    option = GuiCommandUtil::makeSketchTransactionOption();
    wydb::Transaction* pTrans = pTransMgr->startTransaction("", option);
    if (!pTrans)
    {
        return false;
    }

    wy3d::SketchArc* pRightArc = wy3d::SketchArc::cast(
        pTrans->getElementForWrite(_slotElements.rightArc));
    wy3d::SketchLine* pUpperLine = wy3d::SketchLine::cast(
        pTrans->getElementForWrite(_slotElements.upperLine));
    wy3d::SketchArc* pLeftArc = wy3d::SketchArc::cast(
        pTrans->getElementForWrite(_slotElements.leftArc));
    wy3d::SketchLine* pLowerLine = wy3d::SketchLine::cast(
        pTrans->getElementForWrite(_slotElements.lowerLine));

    if (!pRightArc || !pUpperLine || !pLeftArc || !pLowerLine)
    {
        pTransMgr->abortTransaction();
        return false;
    }

    pRightArc->setCenter(wy::Vector2(slot.rightArc.center.x, slot.rightArc.center.y));
    pRightArc->setRadius(slot.rightArc.radius);
    pRightArc->setStartAngle(slot.rightArc.startAngle);
    pRightArc->setEndAngle(slot.rightArc.endAngle);

    pUpperLine->setStartPoint(wy::Vector2(slot.upperStart.x, slot.upperStart.y));
    pUpperLine->setEndPoint(wy::Vector2(slot.upperEnd.x, slot.upperEnd.y));

    pLeftArc->setCenter(wy::Vector2(slot.leftArc.center.x, slot.leftArc.center.y));
    pLeftArc->setRadius(slot.leftArc.radius);
    pLeftArc->setStartAngle(slot.leftArc.startAngle);
    pLeftArc->setEndAngle(slot.leftArc.endAngle);

    pLowerLine->setStartPoint(wy::Vector2(slot.lowerStart.x, slot.lowerStart.y));
    pLowerLine->setEndPoint(wy::Vector2(slot.lowerEnd.x, slot.lowerEnd.y));

    if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
    {
        pTransMgr->abortTransaction();
        return false;
    }

    pTransMgr->mergeTransaction();
    return true;
}

bool SketchDrawStraightSlotGuiCmd::commitSlot(double width)
{
    if (!_pTransGroup || !_slotElements.isValid())
    {
        return false;
    }

    if (!this->updateSlotPreview(width))
    {
        return false;
    }

    Application* pApp = getApplication();
    wydb::Database* pDb = pApp ? pApp->getActiveDatabase() : nullptr;
    if (!pDb || wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction())
    {
        this->abortSlotPreview();
        this->resetToFirstCenter();
        return false;
    }

    _pTransGroup = nullptr;
    _slotElements.reset();
    this->resetToFirstCenter();
    return true;
}

void SketchDrawStraightSlotGuiCmd::abortSlotPreview()
{
    if (_pTransGroup)
    {
        Application* pApp = getApplication();
        wydb::Database* pDb = pApp ? pApp->getActiveDatabase() : nullptr;
        if (pDb)
        {
            pDb->getTransactionManager()->abortTransaction();
        }
        _pTransGroup = nullptr;
    }

    _slotElements.reset();
}

void SketchDrawStraightSlotGuiCmd::initializePopups()
{
    if (_pXYPopup && _pLengthAnglePopup && _pWidthPopup)
    {
        return;
    }

    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
    QWidget* pParent = pMainWindow;

    if (!_pXYPopup)
    {
        _pXYPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchStraightSlot", "X"),
            QCoreApplication::translate("SketchStraightSlot", "Y"),
            QStringLiteral("-1234.56"),
            pParent);
        _pXYPopup->setAcceptHandler([this]() { this->onPopupAccept(); });
        _pXYPopup->setCancelHandler([this]() { this->onPopupCancel(); });
        _pXYPopup->hide();
    }

    if (!_pLengthAnglePopup)
    {
        _pLengthAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchStraightSlot", "Length"),
            QCoreApplication::translate("SketchStraightSlot", "Angle"),
            QStringLiteral("-1234.56"),
            pParent);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupAccept(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupCancel(); });
        _pLengthAnglePopup->hide();
    }

    if (!_pWidthPopup)
    {
        _pWidthPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchStraightSlot", "Width"),
            QStringLiteral("-1234.56"),
            pParent);
        _pWidthPopup->setAcceptHandler([this]() { this->onPopupAccept(); });
        _pWidthPopup->setCancelHandler([this]() { this->onPopupCancel(); });
        _pWidthPopup->hide();
    }
}

void SketchDrawStraightSlotGuiCmd::hidePopups()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthAnglePopup && _pLengthAnglePopup->isVisible())
    {
        _pLengthAnglePopup->hide();
    }
    if (_pWidthPopup && _pWidthPopup->isVisible())
    {
        _pWidthPopup->hide();
    }
}

GuiCmdHoverInputPopupBase* SketchDrawStraightSlotGuiCmd::getActivePopup() const
{
    switch (_step)
    {
    case Step::FirstCenter:
        return _pXYPopup.get();
    case Step::SecondCenter:
        return _pLengthAnglePopup.get();
    case Step::Width:
        return _pWidthPopup.get();
    }
    return nullptr;
}

void SketchDrawStraightSlotGuiCmd::showPopup()
{
    const QPoint globalPos = QCursor::pos();
    switch (_step)
    {
    case Step::FirstCenter:
        if (_pXYPopup)
        {
            _pXYPopup->setValues(_hover.point.x(), _hover.point.y(), 3);
            _pXYPopup->showAtGlobal(globalPos);
        }
        break;
    case Step::SecondCenter:
        if (_pLengthAnglePopup)
        {
            _pLengthAnglePopup->setValues(_hover.length, _hover.angleDeg, 3);
            _pLengthAnglePopup->showAtGlobal(globalPos);
        }
        break;
    case Step::Width:
        if (_pWidthPopup)
        {
            _pWidthPopup->setValue(_currentWidth, 3);
            _pWidthPopup->showAtGlobal(globalPos);
        }
        break;
    }
}

void SketchDrawStraightSlotGuiCmd::onPopupAccept()
{
    switch (_step)
    {
    case Step::FirstCenter:
    {
        if (!_pXYPopup)
        {
            return;
        }

        double x = 0.0;
        double y = 0.0;
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x)
            || !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }

        this->confirmFirstCenter(wy::Vector2(x, y));
        this->hidePopups();
        break;
    }

    case Step::SecondCenter:
    {
        if (!_pLengthAnglePopup)
        {
            return;
        }

        double length = 0.0;
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length)
            || length < kMinDistance)
        {
            return;
        }

        double angleDeg = _hover.angleDeg;
        const QString angleText = _pLengthAnglePopup->getRow2Text().trimmed();
        if (!angleText.isEmpty() && !parseDoubleText(angleText, angleDeg))
        {
            return;
        }

        const double angle = wy3d::degreesToRadians(angleDeg);
        const wy::Vector2 secondCenter = _firstCenter
            + length * wy::Vector2(std::cos(angle), std::sin(angle));
        if (this->confirmSecondCenter(secondCenter))
        {
            this->hidePopups();
        }
        break;
    }

    case Step::Width:
    {
        if (!_pWidthPopup)
        {
            return;
        }

        double width = 0.0;
        if (!parseDoubleText(_pWidthPopup->getRowText(), width)
            || width < kMinWidth)
        {
            return;
        }

        if (this->commitSlot(width))
        {
            this->hidePopups();
        }
        break;
    }
    }
}

void SketchDrawStraightSlotGuiCmd::onPopupCancel()
{
    this->hidePopups();
    this->onEscapeKey();
}

void SketchDrawStraightSlotGuiCmd::confirmFirstCenter(const wy::Vector2& point)
{
    _firstCenter = point;
    _hover.point = point;
    _hover.length = 0.0;
    _hover.angleDeg = 0.0;

    _step = Step::SecondCenter;
    _snapContext = std::make_shared<DrawLine2dContext>(_firstCenter);
    if (_sketchInfo.pSnap2dSys)
    {
        _sketchInfo.pSnap2dSys->clearSnapResult();
    }
    if (_guide)
    {
        _guide->update(_firstCenter, _firstCenter);
    }
    this->updateStatusTip();
}

bool SketchDrawStraightSlotGuiCmd::confirmSecondCenter(const wy::Vector2& point)
{
    if (distance(point, _firstCenter) < kMinDistance)
    {
        return false;
    }

    _secondCenter = point;
    if (!this->startSlotPreview(kProvisionalWidth))
    {
        _secondCenter = wy::Vector2();
        return false;
    }

    _hover.point = point;
    _hover.length = distance(point, _firstCenter);
    _hover.angleDeg = wy3d::radiansToDegrees(
        std::atan2(point.y() - _firstCenter.y(), point.x() - _firstCenter.x()));
    _currentWidth = kProvisionalWidth;
    _step = Step::Width;
    _snapContext = std::make_shared<Locate2dContext>();
    if (_sketchInfo.pSnap2dSys)
    {
        _sketchInfo.pSnap2dSys->clearSnapResult();
    }
    if (_guide)
    {
        _guide->update(_firstCenter, _secondCenter);
    }
    this->updateStatusTip();
    return true;
}

void SketchDrawStraightSlotGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawStraightSlotGuiCmd::tryShowPopupOnHover(double)
{
    if (!this->getActivePopup())
    {
        return;
    }
    if (_hover.lastMouseMoveTime < 0.0)
    {
        return;
    }

    this->showPopup();
}

void SketchDrawStraightSlotGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hover.lastMouseX || event.y != _hover.lastMouseY)
    {
        _hover.lastMouseX = event.x;
        _hover.lastMouseY = event.y;
        _hover.lastMouseMoveTime = event.time;
    }

    const wy::Vector2 point = this->computePoint(event);
    _hover.point = point;

    if (_step == Step::SecondCenter)
    {
        _hover.length = distance(point, _firstCenter);
        _hover.angleDeg = wy3d::radiansToDegrees(
            std::atan2(point.y() - _firstCenter.y(), point.x() - _firstCenter.x()));
    }

    if (_guide)
    {
        if (_step == Step::SecondCenter)
        {
            _guide->update(_firstCenter, point);
        }
        else if (_step == Step::Width)
        {
            _guide->update(_firstCenter, _secondCenter);
        }
        else
        {
            _guide->hideLine();
        }
    }

    if (_step == Step::Width)
    {
        _currentWidth = this->computeWidth(point);
        this->updateStatusTip();
        if (_currentWidth >= kMinWidth)
        {
            this->updateSlotPreview(_currentWidth);
        }
    }
}

void SketchDrawStraightSlotGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopups();
    _hover.lastMouseX = event.x;
    _hover.lastMouseY = event.y;
    _hover.lastMouseMoveTime = event.time;

    const wy::Vector2 point = this->computePoint(event);
    _hover.point = point;

    switch (_step)
    {
    case Step::FirstCenter:
        this->confirmFirstCenter(point);
        break;

    case Step::SecondCenter:
        this->confirmSecondCenter(point);
        break;

    case Step::Width:
    {
        const double width = this->computeWidth(point);
        if (width >= kMinWidth)
        {
            this->commitSlot(width);
        }
        break;
    }
    }
}

void SketchDrawStraightSlotGuiCmd::onEscapeKey()
{
    this->hidePopups();

    if (_step == Step::FirstCenter)
    {
        this->postAbortRequest(wyap::CmdExecution::AbortCause::UserCancel);
        return;
    }

    this->resetToFirstCenter();
}

} // namespace wy3dApp
