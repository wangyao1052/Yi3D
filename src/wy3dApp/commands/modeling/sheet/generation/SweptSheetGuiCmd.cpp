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

#include "commands/modeling/sheet/generation/SweptSheetGuiCmd.h"
#include <QCoreApplication>
#include <QToolTip>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dCurve.h>
#include <wy3dImpl.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"


SweptSheetGuiCmd::SweptSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _pathId(wydb::ElementId::kNull), _profileId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SweptSheetGuiCmd::~SweptSheetGuiCmd()
{
}

wyap::CmdExecution::StartResult SweptSheetGuiCmd::onStart()
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
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch | ElementNodeType::Curve);
    _pointPickOption.selType = wy3d::SelectionType::Element;

    // 初始化:步骤
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    wydb::ElementId pathId(wydb::ElementId::kNull);
    wydb::ElementId profileId(wydb::ElementId::kNull);
    if (this->isValidSketchSelectionSet(ss, pathId, profileId))
    {
        if (!pathId.isNull() && !profileId.isNull())
        {
            _pathId = pathId;
            _profileId = profileId;
            this->clearSelections();
            this->finishStep(Step::SelectProfile);
        }
        else if (!pathId.isNull())
        {
            _pathId = pathId;
            this->finishStep(Step::SelectPath);
        }
        else
        {
            assert(false);
            this->clearSelections();
            this->gotoStep(Step::SelectPath);
        }
    }
    else
    {
        this->clearSelections();
        this->gotoStep(Step::SelectPath);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SweptSheetGuiCmd::onEnd()
{
    // 基类
    GuiCommand::onEnd();

}
void SweptSheetGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    GuiCommand::onAbort(cause);

}

void SweptSheetGuiCmd::cleanup()
{
    _pMakeSweptSheet = nullptr;

    _step = Step::Undefined;
    _pathId = wydb::ElementId::kNull;
    _profileId = wydb::ElementId::kNull;

    _pPathPreview = nullptr;
    _pProfilePreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _pPathHighlightor = nullptr;

    _sketchId2ValidInfo.clear(); // 在不同的步骤该数据不一样所以要清空
}

bool SweptSheetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectPath:
    {
        if (_pathId.isNull())
        {
            assert(false);
            return false;
        }

        _pPathPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;
        _sketchId2ValidInfo.clear();

        // 下一步
        this->gotoStep(Step::SelectProfile);
        return true;
    }
    break;

    case Step::SelectProfile:
    {
        if (_profileId.isNull())
        {
            assert(false);
            return false;
        }

        // 创建扫掠曲面
        _pMakeSweptSheet = std::make_shared<MakeSweptSheet>(this);
        unsigned int errorCode(0);
        if (!_pMakeSweptSheet->create(_pathId, _profileId, errorCode))
        {
            _pMakeSweptSheet = nullptr;
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeSweptSheet->commit();
        _pMakeSweptSheet = nullptr;

        // 重置数据
        this->cleanup();

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

void SweptSheetGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectPath:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SweptSheet", "Select the path sketch."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 前置选择过滤器
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::Sketch::classInfo(), wy3d::Curve::classInfo(), wydb::ElementId::kNull);
    }
    break;

    case Step::SelectProfile:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 高亮路径草图
        wyap::SelectionSet ss;
        if (!_pathId.isNull()) ss.add(wyap::Selection(_pathId));
        _pPathHighlightor = std::make_shared<SelectionSetHighlightor>(ss);

        // 更新提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SweptSheet", "Select the profile sketch."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 前置选择过滤器
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::Sketch::classInfo(), _pathId);
    }
    break;

    default:
    {
        // 清空提示
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void SweptSheetGuiCmd::mouseMovePreview(
    double x, double y,
    std::shared_ptr<ValidSketchTransient>& pSketchPreview,
    IsValidSketchFuncPtr isValidSketchFunc,
    const wydb::ElementId& excludeId)
{
    std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(x, y, _pointPickOption);
    wydb::ElementId pickedSketchId = pickRet.first;

    preview(pickedSketchId, pSketchPreview, isValidSketchFunc);

    if (!pickedSketchId.isNull() && !pSketchPreview)
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

void SweptSheetGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectPath:
    {
        mouseMovePreview(event.x, event.y, _pPathPreview, &SweptSheetGuiCmd::isValidPath, wydb::ElementId::kNull);
    }
    break;

    case Step::SelectProfile:
    {
        assert(!_pathId.isNull());
        mouseMovePreview(event.x, event.y, _pProfilePreview, &SweptSheetGuiCmd::isValidProfile, _pathId);
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

void SweptSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectPath:
    {
        if (_pPathPreview)
        {
            _pathId = _pPathPreview->getSketchId();
            this->finishStep(_step);
        }
    }
    break;

    case Step::SelectProfile:
    {
        if (_pProfilePreview)
        {
            _profileId = _pProfilePreview->getSketchId();
            this->finishStep(_step);
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

void SweptSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    switch (_step)
    {
    case Step::SelectPath:
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(id));
        if (!pSketch) return;
        if (!pSketch->getParent().isNull()) return;

        QString error;
        if (this->isValidPath(id, error))
        {
            _pathId = id;
            this->finishStep(Step::SelectPath);
        }
        else
        {
            MessageBoxUtil::showWarning(error);
        }
    }
    break;

    case Step::SelectProfile:
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(id));
        if (!pSketch) return;
        if (!pSketch->getParent().isNull()) return;

        QString error;
        if (this->isValidProfile(id, error))
        {
            _profileId = id;
            this->finishStep(Step::SelectProfile);
        }
        else
        {
            MessageBoxUtil::showWarning(error);
        }
    }
    break;

    default:
    {
        return;
    }
    break;
    }
}

bool SweptSheetGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss,
    wydb::ElementId& pathId, wydb::ElementId& profileId)
{
    pathId = wydb::ElementId::kNull;
    profileId = wydb::ElementId::kNull;
    if (ss.getCount() == 1)
    {
        const wyap::Selection& sel = ss.createIterator().current();
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            return false;
        }
        wydb::ElementId id = sel.getElementId();
        QString error;
        if (this->isValidPath(id, error))
        {
            pathId = id;
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (ss.getCount() == 2)
    {
        auto iter = ss.createIterator();
        const wyap::Selection& sel1st = iter.current();
        if (sel1st.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            return false;
        }
        wydb::ElementId id1st = sel1st.getElementId();

        iter.moveNext();
        const wyap::Selection& sel2nd = iter.current();
        if (sel2nd.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            return false;
        }
        wydb::ElementId id2nd = sel2nd.getElementId();

        QString error;
        if (this->isValidPath(id1st, error) && this->isValidProfile(id2nd, error))
        {
            pathId = id1st;
            profileId = id2nd;
            return true;
        }
        else if (this->isValidPath(id2nd, error) && this->isValidProfile(id1st, error))
        {
            pathId = id2nd;
            profileId = id1st;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool SweptSheetGuiCmd::isValidPath(const wydb::ElementId& pathId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wydb::Element* pElem = pDb->getElement(pathId);
    if (!pElem) return false;

    const wy3d::Curve* pCurve = wy3d::Curve::cast(pElem);
    if (pCurve)
    {
        // TODO: 是否需要区分具体的曲线
        return true;
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch) return false;
    if (!pSketch->getParent().isNull()) return false;
    return SketchUtil::isValidSweepPath(*pSketch, error);
}

bool SweptSheetGuiCmd::isValidProfile(const wydb::ElementId& profileId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(profileId));
    if (!pSketch) return false;
    if (!pSketch->getParent().isNull()) return false;

    return SketchUtil::isValidProfileForSweptSheet(*pSketch, error);
}

void SweptSheetGuiCmd::preview(wydb::ElementId sketchId,
    std::shared_ptr<ValidSketchTransient>& pSketchPreview,
    IsValidSketchFuncPtr isValidSketchFunc)
{
    if (wydb::ElementId::kNull == sketchId)
    {
        pSketchPreview = nullptr;
        return;
    }
    assert(isValidSketchFunc);
    if (!isValidSketchFunc)
    {
        pSketchPreview = nullptr;
        return;
    }

    if (pSketchPreview && pSketchPreview->getSketchId() == sketchId)
    {
        return;
    }

    pSketchPreview = nullptr;

    auto iter = _sketchId2ValidInfo.find(sketchId);
    if (iter != _sketchId2ValidInfo.cend())
    {
        if (iter->second.valid)
        {
            pSketchPreview = std::make_shared<ValidSketchTransient>(sketchId);
        }
    }
    else
    {
        QString error;
        SketchValidInfo info;
        if ((this->*isValidSketchFunc)(sketchId, error))
        {
            pSketchPreview = std::make_shared<ValidSketchTransient>(sketchId);
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

void MakeSweptSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSweptSheet) idSet.insert(_pSweptSheet->getId());
}

bool MakeSweptSheet::create(const wydb::ElementId& pathId, const wydb::ElementId& profileId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pSweptSheet || _isFinished)
    {
        return false;
    }

    // 路径草图
    const wydb::Element* pPathElem = _pDb->getElement(pathId);
    const wy3d::Sketch* pConstPathSketch = wy3d::Sketch::cast(pPathElem);
    const wy3d::Curve* pConstPathCurve = wy3d::Curve::cast(pPathElem);
    if (pConstPathSketch)
    {
        if (!pConstPathSketch->getParent().isNull()) return false;
    }
    else if (pConstPathCurve)
    {
        if (!pConstPathCurve->getParent().isNull()) return false;
    }
    else
    {
        return false;
    }

    // 轮廓草图
    const wy3d::Sketch* pConstProfileSketch = wy3d::Sketch::cast(_pDb->getElement(profileId));
    if (!pConstProfileSketch) return false;
    if (!pConstProfileSketch->getParent().isNull()) return false;

    // 创建扫掠曲面
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::SweptSheet* pSweptSheet(nullptr);
    if (pConstPathSketch)
    {
        wy3d::Sketch* pPathSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(pathId));
        if (!pPathSketch)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        if (!pProfileSketch)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != wy3d::SweptSheet::create(pTrans, pPathSketch, pProfileSketch, pSweptSheet) || !pSweptSheet)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else if (pConstPathCurve)
    {
        wy3d::Curve* pPathCurve = wy3d::Curve::cast(pTrans->getElementForWrite(pathId));
        if (!pPathCurve)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        if (!pProfileSketch)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != wy3d::SweptSheet::create(pTrans, pPathCurve, pProfileSketch, pSweptSheet) || !pSweptSheet)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    if (!pSweptSheet)
    {
        assert(false);
        goto ABORT_TRANS;
    }

    _pDb->getTransactionManager()->endTransaction();
    _pSweptSheet = pSweptSheet;
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pSweptSheet->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    if (_pSweptSheet)
    {
        _pSweptSheet = nullptr;
    }
    return false;
}
