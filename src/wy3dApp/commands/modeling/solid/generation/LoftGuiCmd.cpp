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

#include "commands/modeling/solid/generation/LoftGuiCmd.h"
#include <QCoreApplication>
#include <QToolTip>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
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


LoftGuiCmd::LoftGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult LoftGuiCmd::onStart()
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

    // 初始化:步骤
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    std::vector<wydb::ElementId> profileIds;
    if (this->isValidSketchSelectionSet(ss, profileIds) && !profileIds.empty())
    {
        if (profileIds.size() == 1)
        {
            this->clearSelections();
            _profilesHighlightor.addSelection(wyap::Selection(profileIds.front()));
            this->gotoStep(Step::SelectProfiles);
        }
        else // 多个
        {
            this->clearSelections();
            for (const wydb::ElementId& profileId : profileIds)
            {
                _profilesHighlightor.addSelection(wyap::Selection(profileId));
            }
            this->finishStep(Step::SelectProfiles);
        }
    }
    else
    {
        this->clearSelections();
        this->gotoStep(Step::SelectProfiles);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void LoftGuiCmd::cleanup()
{
    _step = Step::Undefined;
    _profileIds.clear();

    _pProfilePreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _profilesHighlightor.clearSelections();

    _sketchId2ValidInfo.clear();

    _pMakeLoft = nullptr;
}

void LoftCutGuiCmd::cleanup()
{
    LoftGuiCmd::cleanup();
    _pSolidToCutPreview = nullptr;
}

bool LoftGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectProfiles:
    {
        // 获取选择的轮廓集合
        assert(_profileIds.empty());
        _profileIds = this->getSelectedProfiles(_profilesHighlightor);
        if (_profileIds.size() < 2)
        {
            assert(false);
            _profileIds.clear();
            return false;
        }

        // 创建放样体
        _pMakeLoft = std::make_shared<MakeLoft>(this, false); // isCut = false
        unsigned int errorCode(0);
        if (!_pMakeLoft->create(_profileIds, errorCode))
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            _pMakeLoft = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeLoft->commit();
        _pMakeLoft = nullptr;

        // 退出命令
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

bool LoftCutGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectProfiles:
    {
        // 获取选择的轮廓集合
        assert(_profileIds.empty());
        _profileIds = this->getSelectedProfiles(_profilesHighlightor);
        if (_profileIds.size() < 2)
        {
            assert(false);
            _profileIds.clear();
            return false;
        }

        // 创建放样体
        _pMakeLoft = std::make_shared<MakeLoft>(this, true); // isCut = true
        unsigned int errorCode(0);
        if (!_pMakeLoft->create(_profileIds, errorCode))
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            _pMakeLoft = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 自动切除实体
        if (const wy3d::Solid* pSolidToCut = GuiCommandUtil::autoGetSolidToCut(Application::instance().getActiveDatabase()))
        {
            unsigned int errorCode(0);
            if (_pMakeLoft->cutSolid(pSolidToCut, errorCode))
            {
                _pMakeLoft->commit();
                _pMakeLoft = nullptr;
                this->requestEnd();
                return true;
            }
            else
            {
                if (0 != errorCode)
                {
                    MessageBoxUtil::showError(errorCode);
                }
                _pMakeLoft = nullptr;
                this->requestAbort(AbortCause::ErrorTerminate);
                return false;
            }
        }

        // 下一步
        this->gotoStep(Step::SpecifySolidToCut);
        return true;
    }
    break;

    case Step::SpecifySolidToCut:
    {
        // 校验
        if (!_pMakeLoft)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 切除实体
        unsigned int errorCode(0);
        if (_pMakeLoft->cutSolid(this->getSolidToCut(), errorCode))
        {
            _pMakeLoft->commit();
            _pMakeLoft = nullptr;
            this->requestEnd();
            return true;
        }
        else
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
            _pMakeLoft = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
    }
    break;

    default:
    {
        return LoftGuiCmd::finishStep(step);
    }
    break;
    }

    return false;
}

void LoftGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectProfiles:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("LoftGuiCmd",
            "Select at least two profiles. Press Enter or Spacebar to confirm. Press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选配置项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
        _pointPickOption.selType = wy3d::SelectionType::Element;
    }
    break;

    default:
    {
        assert(false);
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
    }
    break;
    }
}

void LoftCutGuiCmd::gotoStep(Step step)
{
    if (Step::SpecifySolidToCut == step)
    {
        _step = step;

        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 清除高亮
        _profilesHighlightor.clearSelections();

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("LoftGuiCmd",
            "Select the solid to cut."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<SolidToCutSelectPreFilter>();
        _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Solid::classInfo());
    }
    else
    {
        return LoftGuiCmd::gotoStep(step);
    }
}

void LoftGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectProfiles:
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;

        preview(pickedSketchId);

        if (!pickedSketchId.isNull() && !_pProfilePreview)
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

        return;
    }
    break;

    default:
    {
    }
    break;
    }

    return;
}

void LoftCutGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (Step::SpecifySolidToCut == _step)
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pSolidToCutPreview);
        return;
    }
    else
    {
        return LoftGuiCmd::onMouseMove(event);
    }
}

void LoftGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectProfiles:
    {
        if (_pProfilePreview)
        {
            const wyap::Selection& sel = _pProfilePreview->getSelection();
            if (_profilesHighlightor.containsSelection(sel))
            {
                _profilesHighlightor.removeSelection(sel);
            }
            else
            {
                _profilesHighlightor.addSelection(sel);
            }
            _pProfilePreview = nullptr;
        }
    }
    break;

    default:
    {
    }
    break;
    }

    return;
}

void LoftCutGuiCmd::onLeftMouseUp(const MouseEvent& event)
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
        return LoftGuiCmd::onLeftMouseUp(event);
    }
}

void LoftGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectProfiles != _step) return;

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(id));
    if (!pSketch) return;
    if (!pSketch->getParent().isNull()) return;

    wyap::Selection sel(id);
    if (_profilesHighlightor.containsSelection(sel))
    {
        return;
    }

    QString error;
    if (this->isValidProfile(id, error))
    {
        _profilesHighlightor.addSelection(sel);
    }
    else
    {
        MessageBoxUtil::showWarning(error);
    }
}

void LoftGuiCmd::onEnterKey()
{
    if (Step::SelectProfiles == _step)
    {
        if (_profilesHighlightor.getSelectionSet().getCount() >= 2)
        {
            this->finishStep(_step);
        }
    }
}

void LoftGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool LoftGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectProfiles == _step;
}

void LoftGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool LoftGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectProfiles == _step;
}

void LoftGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectProfiles == _step)
    {
        _profilesHighlightor.clearSelections();
    }
}

std::vector<wydb::ElementId> LoftGuiCmd::getSelectedProfiles(const SelectionSetHighlightor& profilesHighlightor)
{
    std::set<wydb::ElementId> ids;
    const wyap::SelectionSet& sels = profilesHighlightor.getSelectionSet();
    for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId sketchId = iter.current().getElementId();
        if (sketchId.isNull())
        {
            assert(false);
            continue;
        }
        ids.insert(sketchId);
    }

    std::vector<wydb::ElementId> profileIds;
    profileIds.reserve(ids.size());
    profileIds.insert(profileIds.cend(), ids.cbegin(), ids.cend());
    return profileIds;
}

bool LoftGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, std::vector<wydb::ElementId>& profileIds)
{
    profileIds.clear();
    if (ss.isEmpty())
    {
        return false;
    }

    std::vector<wydb::ElementId> tempProfileIds;
    tempProfileIds.reserve(ss.getCount());
    QString error;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            return false;
        }
        wydb::ElementId id = sel.getElementId();
        if (this->isValidProfile(id, error))
        {
            tempProfileIds.emplace_back(id);
        }
        else
        {
            return false;
        }
    }

    if (tempProfileIds.empty()) return false;
    profileIds.swap(tempProfileIds);
    return true;
}

bool LoftGuiCmd::isValidProfile(const wydb::ElementId& profileId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(profileId));
    if (!pSketch) return false;
    if (!pSketch->getParent().isNull()) return false;

    return SketchUtil::isValidLoftProfile(*pSketch, error);
}

void LoftGuiCmd::preview(wydb::ElementId sketchId)
{
    if (wydb::ElementId::kNull == sketchId)
    {
        _pProfilePreview = nullptr;
        return;
    }
    if (_pProfilePreview && _pProfilePreview->getSelection().getElementId() == sketchId)
    {
        return;
    }

    _pProfilePreview = nullptr;

    auto iter = _sketchId2ValidInfo.find(sketchId);
    if (iter != _sketchId2ValidInfo.cend())
    {
        if (iter->second.valid)
        {
            _pProfilePreview = std::make_shared<SelectPreview>(wyap::Selection(sketchId));
        }
    }
    else
    {
        QString error;
        SketchValidInfo info;
        if (this->isValidProfile(sketchId, error))
        {
            _pProfilePreview = std::make_shared<SelectPreview>(wyap::Selection(sketchId));
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

const wy3d::Solid* LoftCutGuiCmd::getSolidToCut() const
{
    if (!_pSolidToCutPreview) return nullptr;
    wydb::ElementId id = _pSolidToCutPreview->getSelection().getElementId();
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wy3d::Solid* pSolidToCut = wy3d::Solid::cast(pDb->getElement(id));
    return pSolidToCut;
}

void MakeLoft::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pLoft) idSet.insert(_pLoft->getId());
}

bool MakeLoft::create(const std::vector<wydb::ElementId>& profileIds, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pLoft || _isFinished)
    {
        return false;
    }
    if (profileIds.size() < 2)
    {
        return false;
    }

    // 校验轮廓草图
    for (const wydb::ElementId& profileId : profileIds)
    {
        const wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(_pDb->getElement(profileId));
        if (!pProfileSketch)
        {
            assert(false);
            return false;
        }
        if (!pProfileSketch->getParent().isNull())
        {
            assert(false);
            return false;
        }
    }

    // 创建放样体
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Loft* pLoft = nullptr;
    std::vector<wy3d::Sketch*> profiles;
    profiles.reserve(profileIds.size());
    for (const wydb::ElementId& profileId : profileIds)
    {
        wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(profileId));
        if (!pProfileSketch)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        profiles.emplace_back(pProfileSketch);
    }
    if (_isCut)
    {
        if (wy::ErrorStatus::Ok != wy3d::Loft::createCut(pTrans, profiles, nullptr, pLoft) || !pLoft)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else
    {
        if (wy::ErrorStatus::Ok != wy3d::Loft::create(pTrans, profiles, pLoft) || !pLoft)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    _pLoft = pLoft;
    _pDb->getTransactionManager()->endTransaction();
    // added by wangyao 2025.04.16 {
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pLoft->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    // }
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pLoft = nullptr;
    return false;
}

bool MakeLoft::cutSolid(const wy3d::Solid* pConstSolidToCut, unsigned int& errorCode)
{
    if (!_pDb || !_pTopTrans || !_pLoft || _isFinished)
    {
        return false;
    }
    if (!pConstSolidToCut)
    {
        return false;
    }
    if (pConstSolidToCut->getId() == _pLoft->getId())
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
    if (wy::ErrorStatus::Ok != _pLoft->upgradeForWrite())
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    
    if (wy::ErrorStatus::Ok != pSolidToCut->addModification(_pLoft))
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();

    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(_pLoft->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }

    return true;
}
