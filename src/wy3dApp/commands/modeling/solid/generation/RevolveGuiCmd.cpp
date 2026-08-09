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

#include "commands/modeling/solid/generation/RevolveGuiCmd.h"

#include <QCoreApplication>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dImpl.h>
#include <wy3dSketchProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dSolid.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "utils/GuiCommandUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "select/filters/SolidToCutSelFilter.h"


RevolveGuiCmd::RevolveGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull), _axisCurveId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

RevolveGuiCmd::~RevolveGuiCmd()
{
}

wyap::CmdExecution::StartResult RevolveGuiCmd::onStart()
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
        this->clearSelections();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        this->clearSelections();
        this->gotoStep(Step::SelectSketch);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void RevolveGuiCmd::onEnd()
{
    // 基类
    GuiCommand::onEnd();

}
void RevolveGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    GuiCommand::onAbort(cause);

}

void RevolveGuiCmd::cleanup()
{
    _pMakeRevolution = nullptr;

    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _axisCurveId = wydb::ElementId::kNull;
    _pValidSketch = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _pAxisCurvePreview = nullptr;
}

void RevolveGuiCmd::reset()
{
    this->cleanup();
}

void RevolveCutGuiCmd::reset()
{
    RevolveGuiCmd::cleanup();
    _pSolidToCutPreview = nullptr;
}

bool RevolveGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        assert(!_sketchId.isNull());
        _pInvalidSketchTooltip = nullptr;
        this->gotoStep(Step::SelectAxisCurve);
        return true;
    }
    break;

    case Step::SelectAxisCurve:
    {
        assert(!_sketchId.isNull());
        assert(!_axisCurveId.isNull());
        _pAxisCurvePreview = nullptr;
        _pMakeRevolution = std::make_shared<MakeRevolution>(this, false); // isCut = false
        unsigned int errorCode(0);
        if (!_pMakeRevolution->create(_sketchId, _axisCurveId, errorCode))
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeRevolution->commit();
        _pMakeRevolution = nullptr;

        _sketchId = wydb::ElementId::kNull;
        _axisCurveId = wydb::ElementId::kNull;
        _pValidSketch = nullptr;
        _pInvalidSketchTooltip = nullptr;

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

bool RevolveCutGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        assert(!_sketchId.isNull());
        _pInvalidSketchTooltip = nullptr;
        this->gotoStep(Step::SelectAxisCurve);
        return true;
    }
    break;

    case Step::SelectAxisCurve:
    {
        assert(!_sketchId.isNull());
        assert(!_axisCurveId.isNull());
        _pAxisCurvePreview = nullptr;
        _pMakeRevolution = std::make_shared<MakeRevolution>(this, true); // isCut = true
        unsigned int errorCode(0);
        if (!_pMakeRevolution->create(_sketchId, _axisCurveId, errorCode))
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        _sketchId = wydb::ElementId::kNull;
        _axisCurveId = wydb::ElementId::kNull;
        _pValidSketch = nullptr;
        _pInvalidSketchTooltip = nullptr;

        if (const wy3d::Solid* pSolidToCut = GuiCommandUtil::autoGetSolidToCut(
            Application::instance().getActiveDatabase()))
        {
            unsigned int errorCode(0);
            if (_pMakeRevolution->cutSolid(pSolidToCut, errorCode))
            {
                _pMakeRevolution->commit();
                _pMakeRevolution = nullptr;
                this->requestEnd();
                return true;
            }
            else
            {
                if (0 != errorCode)
                {
                    MessageBoxUtil::showError(errorCode);
                }
                _pMakeRevolution = nullptr;
                this->requestAbort(AbortCause::ErrorTerminate);
                return false;
            }
        }

        // next step
        this->gotoStep(Step::SpecifySolidToCut);
        return true;
    }
    break;

    case Step::SpecifySolidToCut:
    {
        if (!_pMakeRevolution)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        unsigned int errorCode(0);
        if (_pMakeRevolution->cutSolid(this->getSolidToCut(), errorCode))
        {
            _pMakeRevolution->commit();
            _pMakeRevolution = nullptr;
            this->requestEnd();
            return true;
        }
        else
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            _pMakeRevolution = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
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

void RevolveGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectSketch:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RevolveGuiCmd",
            "Select the sketch to revolve."));

        // 设置鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectAxisCurve:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
        _pointPickOption.selType = wy3d::SelectionType::SketchCurve;
        _pointPickOption.pSelFilter = nullptr;

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RevolveGuiCmd",
            "Select the axis line."));

        // 设置鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
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

void RevolveCutGuiCmd::gotoStep(Step step)
{
    if (Step::SpecifySolidToCut == step)
    {
        _step = step;

        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RevolveGuiCmd",
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
        return RevolveGuiCmd::gotoStep(step);
    }
}

void RevolveGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;

        preview(pickedSketchId);

        if (!pickedSketchId.isNull() && !_pValidSketch)
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
    else if (_step == Step::SelectAxisCurve)
    {
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pAxisCurvePreview);
        bool valid = false;
        if (_pAxisCurvePreview)
        {
            const wyap::Selection& sel = _pAxisCurvePreview->getSelection();
            wydb::ElementId curveId(static_cast<std::uint64_t>(std::stoul(sel.getSubPath())));
            const wydb::Database* pDb = Application::instance().getActiveDatabase();
            if (pDb)
            {
                const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(curveId));
                if (pCurve && (wy3d::SketchLine::cast(pCurve) || wy3d::SketchCenterLine::cast(pCurve)))
                {
                    valid = true;
                }
            }
            if (!valid)
            {
                _pAxisCurvePreview = nullptr;
            }
        }
        Application::instance().setCursor(valid ? CursorType::SelectElements : CursorType::Forbid);
    }
    else
    {
        assert(false);
    }

    return;
}

void RevolveCutGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (Step::SpecifySolidToCut == _step)
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pSolidToCutPreview);
    }
    else
    {
        return RevolveGuiCmd::onMouseMove(event);
    }
}

void RevolveGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        if (_pValidSketch)
        {
            _sketchId = _pValidSketch->getSketchId();
            this->finishStep(_step);
        }
    }
    else if (_step == Step::SelectAxisCurve)
    {
        if (_pAxisCurvePreview)
        {
            const wyap::Selection& sel = _pAxisCurvePreview->getSelection();
            _axisCurveId = wydb::ElementId(static_cast<std::uint64_t>(std::stoul(sel.getSubPath())));
            this->finishStep(_step);
        }
    }

    return;
}

void RevolveCutGuiCmd::onLeftMouseUp(const MouseEvent& event)
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
        return RevolveGuiCmd::onLeftMouseUp(event);
    }
}

void RevolveGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
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
        this->clearSelections();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        MessageBoxUtil::showWarning(error);
    }
}

bool RevolveGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
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

bool RevolveGuiCmd::isValidSketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch) return false;
    if (!pSketch->getParent().isNull()) return false;

    // 轴由用户手动选择，不要求草图中必须有中心线
    return SketchUtil::isValidExtrusionProfile(*pSketch, error);
}

void RevolveGuiCmd::preview(wydb::ElementId sketchId)
{
    if (wydb::ElementId::kNull == sketchId)
    {
        _pValidSketch = nullptr;
        return;
    }

    if (_pValidSketch && _pValidSketch->getSketchId() == sketchId)
    {
        return;
    }
    _pValidSketch = nullptr;

    auto iter = _sketchId2ValidInfo.find(sketchId);
    if (iter != _sketchId2ValidInfo.cend())
    {
        if (iter->second.valid)
        {
            _pValidSketch = std::make_shared<ValidSketchTransient>(sketchId);
        }
    }
    else
    {
        QString error;
        SketchValidInfo info;
        if (this->isValidSketch(sketchId, error))
        {
            _pValidSketch = std::make_shared<ValidSketchTransient>(sketchId);
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

const wy3d::Solid* RevolveCutGuiCmd::getSolidToCut() const
{
    if (!_pSolidToCutPreview) return nullptr;
    wydb::ElementId id = _pSolidToCutPreview->getSelection().getElementId();
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wy3d::Solid* pSolidToCut = wy3d::Solid::cast(pDb->getElement(id));
    return pSolidToCut;
}

void MakeRevolution::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pRevolution) idSet.insert(_pRevolution->getId());
}

bool MakeRevolution::create(const wydb::ElementId& sketchId, const wydb::ElementId& axisCurveId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pRevolution || _isFinished)
    {
        return false;
    }
    if (sketchId.isNull() || axisCurveId.isNull())
    {
        return false;
    }

    // 获取草图
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(_pDb->getElement(sketchId));
    if (!pConstSketch) return false;
    if (!pConstSketch->getParent().isNull())
    {
        return false;
    }

    // 旋转轴线（由用户指定）
    const wy3d::SketchCurve* pAxis = wy3d::SketchCurve::cast(_pDb->getElement(axisCurveId));
    if (!pAxis)
    {
        errorCode = static_cast<unsigned int>(wy3d::ErrorCode::REVOLUTION_NoRevolutionAxisLine);
        return false;
    }

    // 创建旋转体
    wy3d::Revolution* pRevolution = nullptr;
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
        if (wy::ErrorStatus::Ok != wy3d::Revolution::createCut(pTrans, pSketch, pAxis, 0.0, wy3d::TWO_PI, nullptr, pRevolution) || !pRevolution)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else
    {
        if (wy::ErrorStatus::Ok != wy3d::Revolution::create(pTrans, pSketch, pAxis, 0.0, wy3d::TWO_PI, pRevolution) || !pRevolution)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    _pRevolution = pRevolution;
    // added by wangyao 2025.04.16 {
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pRevolution->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    // }
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    if (_pRevolution)
    {
        wydb::deleteElement(_pRevolution);
        _pRevolution = nullptr;
    }
    return false;
}

bool MakeRevolution::cutSolid(const wy3d::Solid* pConstSolidToCut, unsigned int& errorCode)
{
    if (!_pDb || !_pTopTrans || !_pRevolution || _isFinished)
    {
        return false;
    }
    if (!pConstSolidToCut)
    {
        return false;
    }
    if (pConstSolidToCut->getId() == _pRevolution->getId())
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
    if (wy::ErrorStatus::Ok != _pRevolution->upgradeForWrite())
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    
    if (wy::ErrorStatus::Ok != pSolidToCut->addModification(_pRevolution))
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();

    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(_pRevolution->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }

    return true;
}
