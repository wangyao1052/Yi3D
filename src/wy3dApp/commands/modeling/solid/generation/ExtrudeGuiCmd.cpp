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

#include "commands/modeling/solid/generation/ExtrudeGuiCmd.h"
#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <QToolTip>
#include <cmath>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
#include <wy3dSolid.h>
#include <wy3dSolid.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "utils/GuiCommandUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "select/filters/SolidToCutSelFilter.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


ExtrudeGuiCmd::ExtrudeGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull), _pickPos(), _depth(0.0),
    _direction(wy3d::ExtrusionDirection::OneSide),
    _pDepthPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

ExtrudeGuiCmd::~ExtrudeGuiCmd()
{
}

wyap::CmdExecution::StartResult ExtrudeGuiCmd::onStart()
{
    // 是否有可用的草图
    if (!SketchUtil::hasUnusedSketch(Application::instance().getActiveDatabase()))
    {
        MessageBoxUtil::showInformation_NoAvailableSketches();
        return wyap::CmdExecution::StartResult::Rejected;
    }

    // 基类
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化:点选选项
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Sketch::classInfo());

    // 初始化:步骤
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    wydb::ElementId sketchId(wydb::ElementId::kNull);
    if (this->isValidSketchSelectionSet(ss, sketchId) && !sketchId.isNull())
    {
        _sketchId = sketchId;
        _pickPos = SketchUtil::getSketchOrigin(Application::instance().getActiveDatabase(), _sketchId);
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->gotoStep(Step::SelectSketch);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void ExtrudeGuiCmd::onEnd()
{
    this->hidePopup();
    // 基类
    GuiCommand::onEnd();

    // 放弃当前的拉伸体
    if (_pMakeExtrusion)
    {
        _pMakeExtrusion = nullptr;
    }

}
void ExtrudeGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();
    // 基类
    GuiCommand::onAbort(cause);

    // 放弃当前的拉伸体
    if (_pMakeExtrusion)
    {
        _pMakeExtrusion = nullptr;
    }

}

void ExtrudeGuiCmd::reset()
{
    this->cleanup();
}

void ExtrudeGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _pickPos.set(0.0, 0.0, 0.0);
    _depth = 0.0;
    _direction = wy3d::ExtrusionDirection::OneSide;

    _pValidSketchPreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _hoverPopupState.resetValue();
    _pMakeExtrusion = nullptr;
}

void ExtrudeCutGuiCmd::reset()
{
    ExtrudeGuiCmd::reset();
    _pSolidToCutPreview = nullptr;
}

std::shared_ptr<MakeExtrusion> ExtrudeGuiCmd::newMakeExtrusion()
{
    return std::make_shared<MakeExtrusion>(this, false); // isCut = false
}

std::shared_ptr<MakeExtrusion> ExtrudeCutGuiCmd::newMakeExtrusion()
{
    return std::make_shared<MakeExtrusion>(this, true); // isCut = true
}

bool ExtrudeGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        _pMakeExtrusion = this->newMakeExtrusion();
        unsigned int errorCode(0);
        if (!_pMakeExtrusion->init(_sketchId, errorCode))
        {
            _pValidSketchPreview = nullptr;
            _pInvalidSketchTooltip = nullptr;
            _pMakeExtrusion = nullptr;
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;

        // next step
        this->gotoStep(Step::SpecifyDepth);
        return true;
    }
    break;

    case Step::SpecifyDepth:
    {
        if (_pMakeExtrusion)
        {
            if (!_pMakeExtrusion->update(_depth))
            {
                return false;
            }
            _pMakeExtrusion->commit();
            _pMakeExtrusion = nullptr;
        }

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

bool ExtrudeCutGuiCmd::finishStep(Step step)
{
    if (Step::SpecifyDepth == step)
    {
        if (!_pMakeExtrusion)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        if (!_pMakeExtrusion->update(_depth))
        {
            return false;
        }
        
        // 自动获取切除的实体
        if (const wy3d::Solid* pSolidToCut = GuiCommandUtil::autoGetSolidToCut(
            Application::instance().getActiveDatabase()))
        {
            unsigned int errorCode(0);
            if (_pMakeExtrusion->cutSolid(pSolidToCut, errorCode))
            {
                _pMakeExtrusion->commit();
                _pMakeExtrusion = nullptr;
                this->requestEnd();
                return true;
            }
            else
            {
                if (0 != errorCode)
                {
                    MessageBoxUtil::showError(errorCode);
                }
                this->requestAbort(AbortCause::ErrorTerminate);
                return false;
            }
        }

        // next step
        this->gotoStep(Step::SpecifySolidToCut);
        return true;
    }
    else if (Step::SpecifySolidToCut == step)
    {
        if (!_pMakeExtrusion)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        unsigned int errorCode(0);
        if (_pMakeExtrusion->cutSolid(this->getSolidToCut(), errorCode))
        {
            _pMakeExtrusion->commit();
            _pMakeExtrusion = nullptr;
            this->requestEnd();
            return true;
        }
        else
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            _pMakeExtrusion = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
    }
    else
    {
        return ExtrudeGuiCmd::finishStep(step);
    }
}

void ExtrudeGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SelectSketch:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ExtrudeGuiCmd",
            "Select the sketch to extrude."));

        // 设置鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SpecifyDepth:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ExtrudeGuiCmd",
            "Specify the extrusion depth; you can directly input the value. Press Tab to switch the direction (One Side / Symmetric)."));

        // 设置鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    default:
    {
        // 清空提示
        Application::instance().getStatusBar()->setTips("");
        // 设置鼠标样式
        Application::instance().setCursor(CursorType::Select);

        assert(false);
    }
    break;
    }
}

void ExtrudeCutGuiCmd::gotoStep(Step step)
{
    if (Step::SpecifySolidToCut == step)
    {
        this->hidePopup();
        _hoverPopupState.resetValue();

        _step = step;

        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ExtrudeGuiCmd",
            "Select the solid to cut."));

        // 设置鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<SolidToCutSelectPreFilter>();
        _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Solid::classInfo());
    }
    else
    {
        return ExtrudeGuiCmd::gotoStep(step);
    }
}

void ExtrudeGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SelectSketch)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;
        _pickPos = pickRet.second;

        preview(pickedSketchId);

        if (!pickedSketchId.isNull() && !_pValidSketchPreview)
        {
            if (!_pInvalidSketchTooltip || _pInvalidSketchTooltip->getSketchId() != pickedSketchId)
            {
                _pInvalidSketchTooltip = std::make_shared<InvalidSketchToolTip>(pickedSketchId,
                    _sketchId2ValidInfo[pickedSketchId].error);
            }
            Application::instance().setCursor(CursorType::Forbid);
        }
        else
        {
            _pInvalidSketchTooltip = nullptr;
            Application::instance().setCursor(CursorType::SelectElements);
        }
    }
    else if (_step == Step::SpecifyDepth) // 确定拉伸深度
    {
        double height(0.0);
        if (this->computeHeight(event.x, event.y, _pickPos, height, _pMakeExtrusion.get())) // height可以小于0
        {
            if (wy3d::ExtrusionDirection::Symmetric == _direction)
            {
                // 对称拉伸下鼠标所在侧不决定方向;鼠标定位的是体的远端,
                // 所以总深度 = 2 * 鼠标到草图面的距离
                _hoverPopupState.depthSign = 1;
                _hoverPopupState.depth = 2.0 * std::fabs(height);
                if (_pMakeExtrusion) _pMakeExtrusion->update(_hoverPopupState.depth);
            }
            else
            {
                _hoverPopupState.depthSign = height < 0.0 ? -1 : 1;
                _hoverPopupState.depth = std::fabs(height);
                {
                    if (_pMakeExtrusion) _pMakeExtrusion->update(height);
                }
            }
        }
        else
        {
            assert(false);
        }
    }

    return;
}

void ExtrudeCutGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (Step::SpecifySolidToCut == _step)
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pSolidToCutPreview);
        return;
    }
    else
    {
        return ExtrudeGuiCmd::onMouseMove(event);
    }
}

void ExtrudeGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SelectSketch)
    {
    }
    else if (_step == Step::SpecifyDepth)
    {
        double height(0.0);
        if (this->computeHeight(event.x, event.y, _pickPos, height, _pMakeExtrusion.get())) // height可以小于0
        {
            _depth = (wy3d::ExtrusionDirection::Symmetric == _direction) ? 2.0 * std::fabs(height) : height;
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

    return;
}

void ExtrudeCutGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (Step::SpecifySolidToCut == _step)
    {
        // do nothing
        return;
    }
    else
    {
        return ExtrudeGuiCmd::onLeftMouseDown(event);
    }
}

void ExtrudeGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        if (_pValidSketchPreview)
        {
            _sketchId = _pValidSketchPreview->getSketchId();
            this->finishStep(_step);
        }
    }

    return;
}

void ExtrudeCutGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::SpecifySolidToCut == _step)
    {
        if (_pSolidToCutPreview)
        {
            this->finishStep(_step);
        }
        return;
    }
    else
    {
        return ExtrudeGuiCmd::onLeftMouseUp(event);
    }
}

void ExtrudeGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void ExtrudeGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pDepthPopup)
    {
        _pDepthPopup = std::make_unique<GuiCmdHoverInputPopup2_2ndTabLabel>(
            QCoreApplication::translate("ExtrudeGuiCmd", "Depth"),
            QCoreApplication::translate("ExtrudeGuiCmd", "Direction"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pDepthPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pDepthPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pDepthPopup->setDirectionToggleHandler([this]()
        {
            // Tab toggles the direction; the preview updates immediately
            _direction = (wy3d::ExtrusionDirection::Symmetric == _direction)
                ? wy3d::ExtrusionDirection::OneSide
                : wy3d::ExtrusionDirection::Symmetric;
            if (wy3d::ExtrusionDirection::Symmetric == _direction)
            {
                _hoverPopupState.depthSign = 1;
            }
            if (_pMakeExtrusion)
            {
                _pMakeExtrusion->setDirection(_direction);
                _pMakeExtrusion->update(_hoverPopupState.depth);
            }
            this->updateDirectionLabel();
        });
        this->updateDirectionLabel();
        _pDepthPopup->hide();
    }
}

void ExtrudeGuiCmd::showPopup()
{
    if (_step != Step::SpecifyDepth)
    {
        return;
    }
    if (!_pDepthPopup)
    {
        this->initializePopups();
    }
    if (!_pDepthPopup)
    {
        return;
    }
    _pDepthPopup->setValue(_hoverPopupState.depth);
    this->updateDirectionLabel();
    _pDepthPopup->showAtGlobal(QCursor::pos());
}

void ExtrudeGuiCmd::updateDirectionLabel()
{
    if (!_pDepthPopup)
    {
        return;
    }
    const QString directionText = (wy3d::ExtrusionDirection::Symmetric == _direction)
        ? QCoreApplication::translate("ExtrudeGuiCmd", "Symmetric")
        : QCoreApplication::translate("ExtrudeGuiCmd", "One Side");
    _pDepthPopup->setDirectionLabel(directionText);
}

void ExtrudeGuiCmd::hidePopup()
{
    if (_pDepthPopup && _pDepthPopup->isVisible())
    {
        _pDepthPopup->hide();
    }
}

void ExtrudeGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyDepth)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pDepthPopup && _pDepthPopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void ExtrudeGuiCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyDepth || !_pDepthPopup)
    {
        return;
    }

    double depth(0.0);
    if (!parseDoubleText(_pDepthPopup->getRowText(), depth))
    {
        return;
    }
    _depth = (wy3d::ExtrusionDirection::Symmetric == _direction)
        ? std::fabs(depth)
        : (_hoverPopupState.depthSign < 0 ? -depth : depth);

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void ExtrudeGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void ExtrudeGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void ExtrudeGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSketch != _step) return;
    if (!_sketchId.isNull()) return;

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(id));
    if (!pSketch) return;
    if (!pSketch->getParent().isNull()) return;

    QString error;
    if (this->isValidSketch(id, error))
    {
        _sketchId = id;
        _pickPos = SketchUtil::getSketchOrigin(Application::instance().getActiveDatabase(), _sketchId);
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        MessageBoxUtil::showWarning(error);
    }
}

bool ExtrudeGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
{
    sketchId = wydb::ElementId::kNull;
    if (ss.getCount() != 1)
    {
        return false;
    }
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
    {
        return false;
    }
    wydb::ElementId id = sel.getElementId();
    QString error;
    if (this->isValidSketch(id, error))
    {
        sketchId = id;
        return true;
    }
    else
    {
        return false;
    }
}

bool ExtrudeGuiCmd::isValidSketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch) return false;
    if (!pSketch->getParent().isNull()) return false;

    return SketchUtil::isValidExtrusionProfile(*pSketch, error);
}

void ExtrudeGuiCmd::preview(wydb::ElementId sketchId)
{
    if (wydb::ElementId::kNull == sketchId)
    {
        _pValidSketchPreview = nullptr;
        return;
    }

    if (_pValidSketchPreview && _pValidSketchPreview->getSketchId() == sketchId)
    {
        return;
    }

    _pValidSketchPreview = nullptr;

    auto iter = _sketchId2ValidInfo.find(sketchId);
    if (iter != _sketchId2ValidInfo.cend())
    {
        if (iter->second.valid)
        {
            _pValidSketchPreview = std::make_shared<ValidSketchTransient>(sketchId);
        }
    }
    else
    {
        QString error;
        SketchValidInfo info;
        if (this->isValidSketch(sketchId, error))
        {
            _pValidSketchPreview = std::make_shared<ValidSketchTransient>(sketchId);
            info.valid = true;
        }
        else
        {
            info.valid = false;
            info.error = error;
        }
        _sketchId2ValidInfo[sketchId] = info;
    }
}

const wy3d::Solid* ExtrudeCutGuiCmd::getSolidToCut() const
{
    if (!_pSolidToCutPreview) return nullptr;
    wydb::ElementId id = _pSolidToCutPreview->getSelection().getElementId();
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wy3d::Solid* pSolidToCut = wy3d::Solid::cast(pDb->getElement(id));
    return pSolidToCut;
}

void MakeExtrusion::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pExtrusion) idSet.insert(_pExtrusion->getId());
}

bool MakeExtrusion::init(const wydb::ElementId& sketchId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pExtrusion || _isFinished)
    {
        return false;
    }
    if (sketchId.isNull())
    {
        return false;
    }

    // 获取草图
    const wydb::Element* pElem = _pDb->getElement(sketchId);
    if (!pElem)
    {
        return false;
    }
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pElem);
    if (!pConstSketch)
    {
        return false;
    }
    if (!pConstSketch->getParent().isNull())
    {
        return false;
    }
    
    // 创建拉伸体
    wy3d::Extrusion* pExtrusion = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch)
    {
        assert(false);
        goto ABORT_TRANS;
    }
    if (_isCut)
    {
        if (wy::ErrorStatus::Ok != wy3d::Extrusion::createCut(pTrans, pSketch, _direction, wy3d::kMinValue, nullptr, pExtrusion) || !pExtrusion)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else
    {
        if (wy::ErrorStatus::Ok != wy3d::Extrusion::create(pTrans, pSketch, _direction, wy3d::kMinValue, pExtrusion) || !pExtrusion)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    _pExtrusion = pExtrusion;
    _workPlnNormal = pSketch->getPlane().getNormal();
    // added by wangyao 2025.04.16 {
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pExtrusion->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    // }
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pExtrusion = nullptr;
    _workPlnNormal.set(0.0, 0.0, 1.0);
    return false;
}

bool MakeExtrusion::update(double depth)
{
    if (!_pDb || !_pTopTrans || !_pExtrusion || _isFinished)
    {
        return false;
    }
    if (std::fabs(depth) < wy3d::kMinValue || std::fabs(depth) > wy3d::kMaxValue)
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pExtrusion->upgradeForWrite();
        _pExtrusion->setDepth(depth);
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    else
    {
        assert(false);
    }
    return true;
}

bool MakeExtrusion::setDirection(wy3d::ExtrusionDirection direction)
{
    if (!_pDb || !_pTopTrans || !_pExtrusion || _isFinished)
    {
        return false;
    }

    _direction = direction;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pExtrusion->upgradeForWrite();
        _pExtrusion->setDirection(direction);
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    else
    {
        assert(false);
    }
    return true;
}

bool MakeExtrusion::cutSolid(const wy3d::Solid* pConstSolidToCut, unsigned int& errorCode)
{
    if (!_pDb || !_pTopTrans || !_pExtrusion || _isFinished)
    {
        return false;
    }
    if (!pConstSolidToCut)
    {
        return false;
    }
    if (pConstSolidToCut->getId() == _pExtrusion->getId())
    {
        return false;
    }

    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Solid* pSolidToCut = wy3d::Solid::cast(pTrans->getElementForWrite(pConstSolidToCut->getId()));
    if (!pSolidToCut)
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    if (wy::ErrorStatus::Ok != _pExtrusion->upgradeForWrite())
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    
    if (wy::ErrorStatus::Ok != pSolidToCut->addModification(_pExtrusion))
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();

    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(_pExtrusion->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }

    return true;
}
