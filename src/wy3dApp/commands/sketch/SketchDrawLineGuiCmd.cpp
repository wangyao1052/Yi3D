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

#include "SketchDrawLineGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>
#include <vector>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"
#include "snap/SketchSnapSystem.h"
#include "commands/sketch/SketchTrimExtendUtil.h"
#include "utils/MathUtils.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


SketchDrawLineGuiCmd::SketchDrawLineGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _startPnt(), _endPnt(),
    _pXYPopup(nullptr), _pLengthAnglePopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawLineGuiCmd::~SketchDrawLineGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawLineGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 初始化
    this->gotoStep(Step::SpecifyStartPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawLineGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyStartPnt);
}

void SketchDrawLineGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _startPnt.set(0.0, 0.0);
    _pSnapStartTangent = nullptr;
    _endPnt.set(0.0, 0.0);
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();
    _pMakeSketchLine = nullptr;
}

void SketchDrawLineGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyStartPnt || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
    else
    {
        assert(false);
    }
}

bool SketchDrawLineGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchLine = std::make_shared<MakeSketchLine>(this);
            if (!_pMakeSketchLine->init(_startPnt, _sketchInfo.sketchId))
            {
                _pMakeSketchLine = nullptr;
                return false;
            }
        }
        else
        {
            this->reset();
            return false;
        }

        // next step
        this->gotoStep(Step::SpecifyEndPnt);
        return true;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        if (_pMakeSketchLine)
        {
            if (!_pMakeSketchLine->update(_endPnt))
            {
                return false;
            }
            _pMakeSketchLine->commit();
            _pMakeSketchLine = nullptr;
        }

        // modified by wangyao 2025.04.11 {
        // 绘制直线段支持连续绘制
        this->gotoStep(Step::SpecifyStartPnt);
        _startPnt = _endPnt;
        _pSnapStartTangent = nullptr;
        bool result = this->finishStep(Step::SpecifyStartPnt);
        assert(result);
        // }
        
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

void SketchDrawLineGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawLine",
            "Specify the start point; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyEndPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawLine",
            "Specify the end point; you can directly input the values."));

        SketchDrawLineContextSPtr pDrawLineContext = std::make_shared<SketchDrawLineContext>(
            _pMakeSketchLine ? _pMakeSketchLine->getId() : wydb::ElementId::kNull, _startPnt);
        pDrawLineContext->setSnapStartTangent(_pSnapStartTangent);
        _pSnapContext = pDrawLineContext;
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

void SketchDrawLineGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawLineGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY) // moved
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        wy::Vector2 startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = startPnt;
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        wy::Vector2 endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        auto lengthAngle = MathUtils::computeLengthAngle(_startPnt, endPnt);
        _hoverPopupState.point = endPnt;
        _hoverPopupState.length = lengthAngle.first;
        _hoverPopupState.angleDeg = wy3d::radiansToDegrees(lengthAngle.second);
        {
            if (_pMakeSketchLine)
            {
                _pMakeSketchLine->update(endPnt);
            }
        }
    }

    return;
}

void SketchDrawLineGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SpecifyStartPnt)
    {
        _startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _pSnapStartTangent = this->computeSnapStartTangent();
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        _endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
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

void SketchDrawLineGuiCmd::initializePopups()
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
            QCoreApplication::translate("SketchDrawLine", "Length"),
            QCoreApplication::translate("SketchDrawLine", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
}

void SketchDrawLineGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pLengthAnglePopup)
    {
        this->initializePopups();
    }

    GuiCmdHoverInputPopup2* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        pActivePopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        pActivePopup->setValues(
            _hoverPopupState.length,
            _hoverPopupState.angleDeg);
    }
    else
    {
        return;
    }
    pActivePopup->showAtGlobal(QCursor::pos());
}

void SketchDrawLineGuiCmd::hidePopup()
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

GuiCmdHoverInputPopup2* SketchDrawLineGuiCmd::getActivePopup() const
{
    if (_step == Step::SpecifyStartPnt)
    {
        return _pXYPopup.get();
    }
    if (_step == Step::SpecifyEndPnt)
    {
        return _pLengthAnglePopup.get();
    }
    else
    {
        return nullptr;
    }
}

void SketchDrawLineGuiCmd::tryShowPopupOnHover(double time)
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
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawLineGuiCmd::onPopupEnterKey()
{
    GuiCmdHoverInputPopup2* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        double x(0.0);
        double y(0.0);
        if (!parseDoubleText(pActivePopup->getRow1Text(), x) ||
            !parseDoubleText(pActivePopup->getRow2Text(), y))
        {
            return;
        }
        _startPnt.set(x, y);
        _pSnapStartTangent = nullptr;
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        double length(0.0);
        if (!parseDoubleText(pActivePopup->getRow1Text(), length))
        {
            return;
        }

        QString angleText = pActivePopup->getRow2Text().trimmed();
        double angleDeg(_hoverPopupState.angleDeg);
        if (!angleText.isEmpty() && !parseDoubleText(angleText, angleDeg))
        {
            return;
        }
        double angle = wy3d::degreesToRadians(angleDeg);
        _endPnt = _startPnt + length * wy::Vector2(std::cos(angle), std::sin(angle));
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawLineGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawLineGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::shared_ptr<SketchDrawLineContext::SnapStartTangent> SketchDrawLineGuiCmd::computeSnapStartTangent() const
{
    if (!_sketchInfo.pSketchSnapSys) return nullptr;
    SketchSnapResultSPtr pSnapResult = _sketchInfo.pSketchSnapSys->getSnapResult();
    if (!pSnapResult) return nullptr;

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;

    wydb::ElementId startElementId(wydb::ElementId::kNull);
    wy::Vector2 startTangentDir(1.0, 0.0);
    const std::vector<SketchSnapResultItem>& snapRetItems = pSnapResult->getItems();
    for (const SketchSnapResultItem& snapRetItem : snapRetItems)
    {
        if (!snapRetItem.pSnapObject)
        {
            assert(false);
            continue;
        }
    
        if (snapRetItem.pSnapObject->getType() != SketchSnapType::EndPoint &&
            snapRetItem.pSnapObject->getType() != SketchSnapType::PointOnCurve &&
            snapRetItem.pSnapObject->getType() != SketchSnapType::MiddlePoint)
        {
            continue;
        }
    
        wydb::ElementId id = snapRetItem.pSnapObject->getId();
        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
        if (!pSketchCurve)
        {
            continue;
        }        double param(0.0);
        const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
        if (classInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pSketchCurve);
            assert(pCircle);
            param = SketchTrimExtendUtil::getParamOfCircle(pCircle, pSnapResult->getPosition());
        }
        else if (classInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve);
            assert(pArc);
            param = SketchTrimExtendUtil::getParamOfArc(pArc, pSnapResult->getPosition());
    
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve);
            assert(pSpline);
            param = SketchTrimExtendUtil::getParamOfSpline(pSpline, pSnapResult->getPosition());
            assert(param == 0.0 || param == 1.0);
        }
        else
        {
            continue;
        }
    
        startElementId = pSketchCurve->getId();
        startTangentDir = pSketchCurve->getDirectionAt(param);
        startTangentDir.normalize();
        if (startTangentDir.length() < 0.5)
        {
            assert(false);
            continue;
        }
        break;
    }

    if (!startElementId.isNull() && startTangentDir.length() > 0.5)
    {
        return std::make_shared<SketchDrawLineContext::SnapStartTangent>(startElementId, startTangentDir);
    }
    else
    {
        return nullptr;
    }
}

std::set<wydb::ElementId> SketchDrawLineGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchLine) _pMakeSketchLine->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchLine::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchLine) idSet.insert(_pSketchLine->getId());
}

bool MakeSketchLine::init(const wy::Vector2& startPnt, wydb::ElementId sketchId)
{
    if (!_pDb || !_pTopTrans || _pSketchLine || _isFinished)
    {
        return false;
    }

    // 创建SketchLine
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchLine* pSketchLine = nullptr;
    wy::Vector2 endPnt;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;

    endPnt = startPnt + wy::Vector2(wy3d::kMinValue, 0.0);
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, startPnt, endPnt, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine = pSketchLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchLine = pSketchLine;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchLine = nullptr;
    return false;
}

bool MakeSketchLine::update(const wy::Vector2& endPnt)
{
    if (!_pDb || !_pTopTrans || !_pSketchLine || _isFinished)
    {
        return false;
    }
    if ((endPnt - _pSketchLine->getStartPoint()).length() < wy3d::kMinValue)
    {
        return false;
    }
    
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchLine->upgradeForWrite();
        _pSketchLine->setEndPoint(endPnt);
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}

