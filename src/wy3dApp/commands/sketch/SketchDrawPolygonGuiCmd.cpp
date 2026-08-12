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

#include "SketchDrawPolygonGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/SketchPolygonDialog.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "snap/SketchSnapSystem.h"
#include "widgets/frame/MainWindow.h"

#define MIN_SIDES 3
#define MAX_SIDES 100


static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawPolygonGuiCmd::SketchDrawPolygonGuiCmd() : OsgGuiCommand(),
    _polygonType(DrawPolygonType::InscribedPolygon), _numSides(5), _step(Step::Undefined), _centerPnt(), _startVec(),
    _pCenterPopup(nullptr), _pRadiusPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawPolygonGuiCmd::~SketchDrawPolygonGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawPolygonGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 交互:多边形边数
    SketchPolygonDialog dialog(5, MIN_SIDES, MAX_SIDES);
    if (QDialog::Accepted != dialog.exec())
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }
    _polygonType = dialog.isInscribedPolygon() ? DrawPolygonType::InscribedPolygon : DrawPolygonType::CircumscribedPolygon;
    _numSides = dialog.getNumOfSides();
    assert(_numSides >= MIN_SIDES && _numSides <= MAX_SIDES);
    if (_numSides < MIN_SIDES || _numSides > MAX_SIDES)
    {
        return wyap::CmdExecution::StartResult::Failed;
    }

    // 初始化
    this->gotoStep(Step::SpecifyCenterPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawPolygonGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _centerPnt.set(0.0, 0.0);
    _startVec.set(0.0, 0.0);
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();

    _pCircleTransient = nullptr;
    _pCenterPointTransient = nullptr;

    _pMakeSketchPolygon = nullptr;
}

void SketchDrawPolygonGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyCenterPnt);
}

void SketchDrawPolygonGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyCenterPnt || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
}

bool SketchDrawPolygonGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchPolygon = std::make_shared<MakeSketchPolygon>(this, _sketchInfo.sketchId);
            if (!_pMakeSketchPolygon->init(_polygonType, _numSides, _centerPnt))
            {
                _pMakeSketchPolygon = nullptr;
                return false;
            }
        }
        else
        {
            this->reset();
            return false;
        }

        _pCircleTransient = std::make_shared<SketchCircleTransient>(_sketchInfo.sketchPlane);
        _pCircleTransient->update(_centerPnt, 0.0);
        _pCircleTransient->show();

        _pCenterPointTransient = std::make_shared<CenterPointTransient>();
        _pCenterPointTransient->update(_sketchInfo.sketchPlane, _centerPnt);
        _pCenterPointTransient->show();

        // next step
        this->gotoStep(Step::SpecifyRadius);
        return true;
    }
    break;

    case Step::SpecifyRadius:
    {
        if (_pMakeSketchPolygon)
        {
            if (!_pMakeSketchPolygon->update(_startVec))
            {
                return false;
            }
            _pMakeSketchPolygon->commit();
            _pMakeSketchPolygon = nullptr;
        }

        _pCircleTransient = nullptr;
        _pCenterPointTransient = nullptr;

        _centerPnt.set(0.0, 0.0);
        _startVec.set(0.0, 0.0);

        // next step
        this->gotoStep(Step::SpecifyCenterPnt);
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

void SketchDrawPolygonGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawPolygon",
            "Specify the center point of the regular polygon; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawPolygon",
            "Specify the radius of the circumscribed circle; you can directly input the value."));

        SketchDrawCircleContextSPtr pDrawCircleCtx = std::make_shared
            <SketchDrawCircleContext>(wydb::ElementId::kNull, _centerPnt);
        pDrawCircleCtx->setIsForDrawArc(true);
        _pSnapContext = pDrawCircleCtx;
        _pSnapContext->setExcludedIds(std::move(this->getSnapExcludeIds()));
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

void SketchDrawPolygonGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawPolygonGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SpecifyCenterPnt)
    {
        wy::Vector2 centerPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = centerPnt;
    }
    else if (_step == Step::SpecifyRadius)
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        double radius = (pnt - _centerPnt).length();
        _hoverPopupState.point = pnt;
        _hoverPopupState.radius = radius;
        {
            if (_pMakeSketchPolygon)
            {
                _pMakeSketchPolygon->update(pnt - _centerPnt);
            }
            if (_pCircleTransient)
            {
                _pCircleTransient->update(_centerPnt, radius);
            }
            _startVec = pnt - _centerPnt;
        }
    }
    else
    {
        //assert(false);
    }

    return;
}

void SketchDrawPolygonGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SpecifyCenterPnt)
    {
        _centerPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::SpecifyRadius)
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _startVec = pnt - _centerPnt;
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

void SketchDrawPolygonGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pCenterPopup)
    {
        _pCenterPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pCenterPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pCenterPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pCenterPopup->hide();
    }
    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawPolygon", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }
}

void SketchDrawPolygonGuiCmd::showPopup()
{
    if (!_pCenterPopup || !_pRadiusPopup)
    {
        this->initializePopups();
    }

    if (_step == Step::SpecifyCenterPnt)
    {
        if (!_pCenterPopup) return;
        _pCenterPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
        _pCenterPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyRadius)
    {
        if (!_pRadiusPopup) return;
        _pRadiusPopup->setValue(_hoverPopupState.radius);
        _pRadiusPopup->showAtGlobal(QCursor::pos());
    }
}

void SketchDrawPolygonGuiCmd::hidePopup()
{
    if (_pCenterPopup && _pCenterPopup->isVisible())
    {
        _pCenterPopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
}

void SketchDrawPolygonGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyCenterPnt && _step != Step::SpecifyRadius)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pCenterPopup && _pCenterPopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawPolygonGuiCmd::onPopupEnterKey()
{
    if (_step == Step::SpecifyCenterPnt)
    {
        if (!_pCenterPopup) return;
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pCenterPopup->getRow1Text(), x) ||
            !parseDoubleText(_pCenterPopup->getRow2Text(), y))
        {
            return;
        }
        _centerPnt.set(x, y);
    }
    else if (_step == Step::SpecifyRadius)
    {
        if (!_pRadiusPopup) return;
        double radius(0.0);
        if (!parseDoubleText(_pRadiusPopup->getRowText(), radius))
        {
            return;
        }

        if (_startVec.length() <= wy3d::TOL)
        {
            if (_numSides % 2 == 1)
            {
                _startVec.set(0.0, radius);
            }
            else if (_numSides % 4 == 0)
            {
                double startAngle = wy3d::TWO_PI / _numSides / 2;
                _startVec.set(radius * std::cos(startAngle), radius * std::sin(startAngle));
            }
            else
            {
                _startVec.set(radius, 0.0);
            }
        }
        else
        {
            _startVec.normalize();
            _startVec *= radius;
        }
    }
    else
    {
        return;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawPolygonGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawPolygonGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawPolygonGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    if (!_sketchInfo.sketchId.isNull()) snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchPolygon) _pMakeSketchPolygon->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchPolygon::collectElements(std::set<wydb::ElementId>& idSet) const
{
    for (const wy3d::SketchLine* pLine : _lines)
    {
        assert(pLine);
        idSet.insert(pLine->getId());
    }
}

bool MakeSketchPolygon::init(DrawPolygonType type, unsigned int numSides, const wy::Vector2& centerPnt)
{
    if (!_pDb || !_pTopTrans || !_lines.empty() || _isFinished)
    {
        return false;
    }
    if (numSides < MIN_SIDES || numSides > MAX_SIDES) return false;

    _polygonType = type;
    _numSides = numSides;
    _centerPnt = centerPnt;

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;

    std::vector<wy::Vector2> pnts;
    if (_polygonType == DrawPolygonType::InscribedPolygon) // 内接多边形
    {
        MakePolygonUtil::InscribedPolygon(_numSides, _centerPnt, wy3d::kMinValue, 0.0, pnts);
    }
    else // 外接多边形
    {
        MakePolygonUtil::CircumscribedPolygon(_numSides, _centerPnt, wy3d::kMinValue, 0.0, pnts);
    }

    assert(_pGuiCmd);
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchId));
    if (!pSketch) goto ABORT_TRANS;
    for (unsigned int i = 0; i < numSides; ++i)
    {
        wy3d::SketchLine* pSketchLine(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, pnts[i], pnts[(i + 1) % numSides], pSketchLine) || !pSketchLine)
        {
            goto ABORT_TRANS;
        }
        _lines.emplace_back(pSketchLine);
        if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine))
        {
            goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _lines.clear();
    return false;
}

bool MakeSketchPolygon::update(const wy::Vector2& startVec)
{
    if (!_pDb || !_pTopTrans || _lines.empty() || _isFinished)
    {
        return false;
    }
    double radius = startVec.length();
    if (radius < wy3d::kMinValue)
    {
        return false;
    }
    double startAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, startVec);


    std::vector<wy::Vector2> pnts;
    if (_polygonType == DrawPolygonType::InscribedPolygon) // 内接多边形
    {
        MakePolygonUtil::InscribedPolygon(_numSides, _centerPnt, radius, startAngle, pnts);
    }
    else // 外接多边形
    {
        MakePolygonUtil::CircumscribedPolygon(_numSides, _centerPnt, radius, startAngle, pnts);
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    for (unsigned int i = 0; i < _numSides; ++i)
    {
        if (wy::ErrorStatus::Ok != _lines[i]->upgradeForWrite()) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _lines[i]->setStartPoint(pnts[i])) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _lines[i]->setEndPoint(pnts[(i + 1) % _numSides])) goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    return false;
}

// 内接多边形
void MakePolygonUtil::InscribedPolygon(unsigned int numSides,
    const wy::Vector2& center, double radius,
    double startAngle, std::vector<wy::Vector2>& pnts)
{
    assert(radius > 0.0);
    pnts.clear();
    pnts.reserve(numSides);
    if (numSides < 3) return;

    double theta = wy3d::TWO_PI / numSides;
    double angle(0.0);
    for (unsigned int i = 0; i < numSides; ++i)
    {
        angle = startAngle + i * theta;
        pnts.emplace_back(wy::Vector2(
            center.x() + radius * std::cos(angle),
            center.y() + radius * std::sin(angle)));
    }
}

// 外接多边形
void MakePolygonUtil::CircumscribedPolygon(unsigned int numSides,
    const wy::Vector2& center, double radius,
    double startAngle, std::vector<wy::Vector2>& pnts)
{
    assert(radius > 0.0);
    pnts.clear();
    pnts.reserve(numSides);
    if (numSides < 3) return;

    double theta = wy3d::TWO_PI / numSides;
    radius = radius / std::cos(theta / 2);
    double angle(0.0);
    startAngle += theta / 2;
    for (unsigned int i = 0; i < numSides; ++i)
    {
        angle = startAngle + i * theta;
        pnts.emplace_back(wy::Vector2(
            center.x() + radius * std::cos(angle),
            center.y() + radius * std::sin(angle)));
    }
}
