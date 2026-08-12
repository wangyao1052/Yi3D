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

#include "SketchOffsetGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include "snap/SnapSystemBase.h"
#include <wy3dSketch.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "snap/SketchSnapSystem.h"
#include "select/filters/CommonSelFilters.h"
#include "scene/nodes/ElementNodeType.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


SketchOffsetGuiCmd::SketchOffsetGuiCmd()
    : OsgGuiCommand(),
      _step(Step::Undefined),
      _id(wydb::ElementId::kNull),
      _offset(0.0),
      _pOffsetPopup(nullptr),
      _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchOffsetGuiCmd::~SketchOffsetGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchOffsetGuiCmd::onStart()
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
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
        wy3d::SketchEntity::classInfo(), wydb::ElementId::kNull);
    this->gotoStep(Step::SelectElement);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchOffsetGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SelectElement);
}

void SketchOffsetGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _id = wydb::ElementId::kNull;
    _offset = 0.0;
    _pSnapContext = nullptr;

    _pCurve = nullptr;
    _pOffsetCurve = nullptr;
    _hoverPopupState.resetValue();
}

bool SketchOffsetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectElement:
    {
        const wy3d::SketchCurve* pSketchCurve = this->getSketchCurve(_id);
        if (!pSketchCurve)
        {
            assert(false);
            this->reset();
            return false;
        }

        if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
        {
            _pOffsetCurve = std::make_shared<OffsetSketchLine>(this);
        }
        else if (const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
        {
            _pOffsetCurve = std::make_shared<OffsetSketchCenterLine>(this);
        }
        else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
        {
            _pOffsetCurve = std::make_shared<OffsetSketchCircle>(this);
        }
        else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
        {
            _pOffsetCurve = std::make_shared<OffsetSketchArc>(this);
        }
        else
        {
            assert(false);
            this->reset();
            return false;
        }

        if (!_pOffsetCurve->init(pSketchCurve))
        {
            _pOffsetCurve = nullptr;
            return false;
        }

        // next step
        this->gotoStep(Step::SpecifyOffset);
        return true;
    }
    break;

    case Step::SpecifyOffset:
    {
        if (_pOffsetCurve)
        {
            if (!_pOffsetCurve->update(_offset))
            {
                return false;
            }
            _pOffsetCurve->commit();
            _pOffsetCurve = nullptr;
        }

        _pCurve = nullptr;

        // next step
        this->gotoStep(Step::SelectElement);
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

void SketchOffsetGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectElement:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchOffset", "Select the sketch curve to offset."));
        Application::instance().setCursor(CursorType::SelectElements);

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
            _sketchInfo.pSketchSnapSys->clearSnapResult();
        }
    }
    break;

    case Step::SpecifyOffset:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchOffset", "Specify through point or input offset distance."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(_id);
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

void SketchOffsetGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchOffsetGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SelectElement)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
            //wy3d::SketchEntity::classInfo()->className());
        wydb::ElementId id = pickRet.first;
        previewCurve(id);

        if (!id.isNull() && !_pCurve)
            Application::instance().setCursor(CursorType::Forbid);
        else
            Application::instance().setCursor(CursorType::SelectElements);
    }
    else if (_step == Step::SpecifyOffset)
    {
        wy::Vector2 pos = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (_pOffsetCurve)
        {
            double offset = _pOffsetCurve->computeOffset(pos);
            _hoverPopupState.offset = offset;
            {
                _pOffsetCurve->update(offset);
            }
        }
        else
        {
            assert(false);
        }
    }

    return;
}

void SketchOffsetGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SelectElement:
    {
        if (_pCurve)
        {
            _id = _pCurve->getId();
            if (this->finishStep(_step))
            {
                this->simulateMouseMoveFromPopup();
            }
        }
    }
    break;

    case Step::SpecifyOffset:
    {
        wy::Vector2 pos = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (_pOffsetCurve)
        {
            _offset = _pOffsetCurve->computeOffset(pos);
            if (this->finishStep(_step))
            {
                this->simulateMouseMoveFromPopup();
            }
        }
        else
        {
            assert(false);
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return;
}

void SketchOffsetGuiCmd::initializePopups()
{
    if (_pOffsetPopup)
    {
        return;
    }

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    _pOffsetPopup = std::make_unique<GuiCmdHoverInputPopup1>(
        QCoreApplication::translate("SketchOffset", "Offset Distance"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pOffsetPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pOffsetPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pOffsetPopup->hide();
}

void SketchOffsetGuiCmd::showPopup()
{
    if (_step != Step::SpecifyOffset)
    {
        return;
    }
    if (!_pOffsetPopup)
    {
        this->initializePopups();
    }
    if (!_pOffsetPopup)
    {
        return;
    }

    _pOffsetPopup->setValue(_hoverPopupState.offset);
    _pOffsetPopup->showAtGlobal(QCursor::pos());
}

void SketchOffsetGuiCmd::hidePopup()
{
    if (_pOffsetPopup && _pOffsetPopup->isVisible())
    {
        _pOffsetPopup->hide();
    }
}

void SketchOffsetGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyOffset)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pOffsetPopup && _pOffsetPopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchOffsetGuiCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyOffset || !_pOffsetPopup)
    {
        return;
    }

    double offset(0.0);
    if (!parseDoubleText(_pOffsetPopup->getRowText(), offset))
    {
        return;
    }
    _offset = offset;

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchOffsetGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchOffsetGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

const wy3d::SketchCurve* SketchOffsetGuiCmd::getSketchCurve(const wydb::ElementId& id)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    return wy3d::SketchCurve::cast(pDb->getElement(id));
}

std::set<wydb::ElementId> SketchOffsetGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> ids;
    if (!_sketchInfo.sketchId.isNull()) ids.insert(_sketchInfo.sketchId);
    if (!_id.isNull()) ids.insert(_id);
    if (_pOffsetCurve) _pOffsetCurve->collectElements(ids);
    return ids;
}

void SketchOffsetGuiCmd::previewCurve(const wydb::ElementId& id)
{
    const wy3d::SketchCurve* pSketchCurve = getSketchCurve(id);
    if (!pSketchCurve)
    {
        _pCurve = nullptr;
        return;
    }

    // 目前只支持:直线段&中心线&圆&圆弧
    if (pSketchCurve->isKindOf(wy3d::SketchLine::classInfo()) ||
        pSketchCurve->isKindOf(wy3d::SketchCenterLine::classInfo()) ||
        pSketchCurve->isKindOf(wy3d::SketchCircle::classInfo()) ||
        pSketchCurve->isKindOf(wy3d::SketchArc::classInfo()))
    {
        if (_pCurve && _pCurve->getId() == id)
        {
            assert(_pCurve);
            return;
        }
        else
        {
            _pCurve = std::make_shared<SketchCurveTransient>(pSketchCurve, 0.0, 1.0);
        }
    }
    else
    {
        _pCurve = nullptr;
    }
}

OffsetSketchLine::OffsetSketchLine(GuiCommand* pGuiCmd)
    : OffsetSketchCurve(pGuiCmd), _startPnt(), _endPnt(), _lineDir(), _offsetDir(), _pSketchLine(nullptr)
{}

bool OffsetSketchLine::init(const wy3d::SketchCurve* pSketchCurve)
{
    if (!_pDb || !_pTopTrans || _pSketchLine || _isFinished)
    {
        return false;
    }

    const wy3d::SketchLine* pBaseLine = wy3d::SketchLine::cast(pSketchCurve);
    if (!pBaseLine)
    {
        return false;
    }
    wydb::ElementId sketchId = pBaseLine->getParent();
    _startPnt = pBaseLine->getStartPoint();
    _endPnt = pBaseLine->getEndPoint();
    wy::Vector2 lineVec = _endPnt - _startPnt;
    if (lineVec.length() <= wy3d::EPS)
    {
        return false;
    }
    _lineDir = lineVec;
    _lineDir.normalize();
    _offsetDir.set(-_lineDir.y(), _lineDir.x());

    // 创建SketchLine
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    wy3d::SketchLine* pSketchLine(nullptr);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, _startPnt, _endPnt, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine = pSketchLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchLine = nullptr;
    return false;
}

double OffsetSketchLine::computeOffset(const wy::Vector2& pos)
{
    double offset = (pos - _startPnt).dot(_offsetDir);
    if (offset < 0.0)
    {
        _offsetDir = -_offsetDir;
        offset = -offset;
    }
    return offset;
}

bool OffsetSketchLine::update(double offset)
{
    if (!_pDb || !_pTopTrans || !_pSketchLine || _isFinished)
    {
        return false;
    }

    wy::Vector2 offsetVec = _offsetDir * offset;
    wy::Vector2 newStartPnt = _startPnt + offsetVec;
    wy::Vector2 newEndPnt = _endPnt + offsetVec;

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchLine->upgradeForWrite();
        _pSketchLine->setStartPoint(newStartPnt);
        _pSketchLine->setEndPoint(newEndPnt);
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}

void OffsetSketchLine::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchLine) idSet.insert(_pSketchLine->getId());
}

OffsetSketchCenterLine::OffsetSketchCenterLine(GuiCommand* pGuiCmd)
    : OffsetSketchCurve(pGuiCmd), _startPnt(), _endPnt(), _lineDir(), _offsetDir(), _pSketchCenterLine(nullptr)
{}

bool OffsetSketchCenterLine::init(const wy3d::SketchCurve* pSketchCurve)
{
    if (!_pDb || !_pTopTrans || _pSketchCenterLine || _isFinished)
    {
        return false;
    }

    const wy3d::SketchCenterLine* pBaseCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve);
    if (!pBaseCenterLine)
    {
        return false;
    }
    wydb::ElementId sketchId = pBaseCenterLine->getParent();
    _startPnt = pBaseCenterLine->getStartPoint();
    _endPnt = pBaseCenterLine->getEndPoint();
    wy::Vector2 lineVec = _endPnt - _startPnt;
    if (lineVec.length() <= wy3d::EPS)
    {
        return false;
    }
    _lineDir = lineVec;
    _lineDir.normalize();
    _offsetDir.set(-_lineDir.y(), _lineDir.x());

    // 创建中心线
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    wy3d::SketchCenterLine* pSketchCenterLine(nullptr);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchCenterLine::create(pTrans, _startPnt, _endPnt, pSketchCenterLine) || !pSketchCenterLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchCenterLine = pSketchCenterLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchCenterLine))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchCenterLine = nullptr;
    return false;
}

double OffsetSketchCenterLine::computeOffset(const wy::Vector2& pos)
{
    double offset = (pos - _startPnt).dot(_offsetDir);
    if (offset < 0.0)
    {
        _offsetDir = -_offsetDir;
        offset = -offset;
    }
    return offset;
}

bool OffsetSketchCenterLine::update(double offset)
{
    if (!_pDb || !_pTopTrans || !_pSketchCenterLine || _isFinished)
    {
        return false;
    }

    wy::Vector2 offsetVec = _offsetDir * offset;
    wy::Vector2 newStartPnt = _startPnt + offsetVec;
    wy::Vector2 newEndPnt = _endPnt + offsetVec;

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchCenterLine->upgradeForWrite();
        _pSketchCenterLine->setStartPoint(newStartPnt);
        _pSketchCenterLine->setEndPoint(newEndPnt);
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}

void OffsetSketchCenterLine::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchCenterLine) idSet.insert(_pSketchCenterLine->getId());
}

OffsetSketchCircle::OffsetSketchCircle(GuiCommand* pGuiCmd)
    : OffsetSketchCurve(pGuiCmd), _center(), _radius(0.0), _orient(true), _pSketchCircle(nullptr)
{}

bool OffsetSketchCircle::init(const wy3d::SketchCurve* pSketchCurve)
{
    if (!_pDb || !_pTopTrans || _pSketchCircle || _isFinished)
    {
        return false;
    }

    const wy3d::SketchCircle* pBaseCircle = wy3d::SketchCircle::cast(pSketchCurve);
    if (!pBaseCircle)
    {
        return false;
    }
    wydb::ElementId sketchId = pBaseCircle->getParent();
    _center = pBaseCircle->getCenter();
    _radius = pBaseCircle->getRadius();
    if (_radius <= wy3d::EPS)
    {
        return false;
    }
    _orient = true;

    // 创建SketchCircle
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    wy3d::SketchCircle* pSketchCircle(nullptr);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchCircle::create(pTrans, _center, _radius, pSketchCircle) || !pSketchCircle)
    {
        goto ABORT_TRANS;
    }
    _pSketchCircle = pSketchCircle;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchCircle))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchCircle = nullptr;
    return false;
}

double OffsetSketchCircle::computeOffset(const wy::Vector2& pos)
{
    double offset = (pos - _center).length() - _radius;
    _orient = offset > 0.0 ? true : false;
    return std::fabs(offset);
}

bool OffsetSketchCircle::update(double offset)
{
    if (!_pDb || !_pTopTrans || !_pSketchCircle || _isFinished)
    {
        return false;
    }

    double newRadius = _radius;
    if (_orient) newRadius += offset;
    else newRadius -= offset;
    if (newRadius <= 0.0)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchCircle->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchCircle->setRadius(newRadius))
        {
            _pDb->getTransactionManager()->abortTransaction();
            return false;
        }
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}

void OffsetSketchCircle::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchCircle) idSet.insert(_pSketchCircle->getId());
}

OffsetSketchArc::OffsetSketchArc(GuiCommand* pGuiCmd) : OffsetSketchCurve(pGuiCmd),
    _center(), _radius(0.0), _startAngle(0.0), _endAngle(0.0), _orient(true), _pSketchArc(nullptr)
{}

bool OffsetSketchArc::init(const wy3d::SketchCurve* pSketchCurve)
{
    if (!_pDb || !_pTopTrans || _pSketchArc || _isFinished)
    {
        return false;
    }

    const wy3d::SketchArc* pBaseArc = wy3d::SketchArc::cast(pSketchCurve);
    if (!pBaseArc)
    {
        return false;
    }
    wydb::ElementId sketchId = pBaseArc->getParent();
    _center = pBaseArc->getCenter();
    _radius = pBaseArc->getRadius();
    _startAngle = pBaseArc->getStartAngle();
    _endAngle = pBaseArc->getEndAngle();
    _startAngle = wy3d::normalizeRadian(_startAngle);
    _endAngle = wy3d::normalizeRadian(_endAngle);
    if (_endAngle < _startAngle) _endAngle += wy3d::TWO_PI;
    if (_radius <= wy3d::EPS || (_endAngle - _startAngle) <= wy3d::EPS)
    {
        return false;
    }
    _orient = true;

    // 创建SketchArc
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    wy3d::SketchArc* pSketchArc(nullptr);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, _center, _radius, _startAngle, _endAngle, pSketchArc) || !pSketchArc)
    {
        goto ABORT_TRANS;
    }
    _pSketchArc = pSketchArc;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchArc))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchArc = nullptr;
    return false;
}

double OffsetSketchArc::computeOffset(const wy::Vector2& pos)
{
    double offset = (pos - _center).length() - _radius;
    _orient = offset > 0.0 ? true : false;
    return std::fabs(offset);
}

bool OffsetSketchArc::update(double offset)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc || _isFinished)
    {
        return false;
    }

    double newRadius = _radius;
    if (_orient) newRadius += offset;
    else newRadius -= offset;
    if (newRadius <= 0.0)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchArc->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchArc->setRadius(newRadius))
        {
            _pDb->getTransactionManager()->abortTransaction();
            return false;
        }
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}

void OffsetSketchArc::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchArc) idSet.insert(_pSketchArc->getId());
}
