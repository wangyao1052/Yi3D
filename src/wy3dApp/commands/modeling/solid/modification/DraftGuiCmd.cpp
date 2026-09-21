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

#include "commands/modeling/solid/modification/DraftGuiCmd.h"
#include <QCoreApplication>
#include <QToolTip>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/DoubleValueInputDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"
#include "scene/Colors.h"


// 前置过滤器: 确保只能选择单一主体的面或边
class DraftGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    DraftGuiCmdPreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            _targetElemId = iter.current().getElementId();
            break;
        }
    }

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        if (id.isNull()) return SelectFilterStatus::Continue;

        if (_targetElemId.isNull() || id == _targetElemId)
        {
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Continue;
        }
    }

private:
    wydb::ElementId _targetElemId;
};

// 中性面选择过滤器:不能选择拔模面
class DraftGuiCmdNeutralFaceSelFilter : public SelectFilterFunctor
{
public:
    DraftGuiCmdNeutralFaceSelFilter(const wyap::SelectionSet& ss) : _ss(ss) {}

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        if (_ss.contains(sel))
        {
            return SelectFilterStatus::Continue;
        }
        else
        {
            return SelectFilterStatus::Ok;
        }
    }

private:
    wyap::SelectionSet _ss;
};

DraftGuiCmd::DraftGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _angle(wy3d::degreesToRadians(1.0))
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult DraftGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
    _pointPickOption.selType = wy3d::SelectionType::Face;
    _pointPickOption.acceptElement = false;
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    _pSelSetHighlightor_NeutralFace = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet(), Colors::kPink);
    this->gotoStep(Step::SelectDraftFaces);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void DraftGuiCmd::reset()
{
    _step = Step::Undefined;
    _draftFacesSels.clear();
    _neutralFaceSels.clear();
    _angle = wy3d::degreesToRadians(1.0);

    _pPreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    if (_pSelSetHighlightor_NeutralFace) _pSelSetHighlightor_NeutralFace->clearSelections();
}

bool DraftGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectDraftFaces:
    {
        if (_draftFacesSels.isEmpty())
        {
            assert(false);
            return false;
        }

        // 下一步
        this->gotoStep(Step::SelectNeutralFace);
        return true;
    }
    break;

    case Step::SelectNeutralFace:
    {
        if (_neutralFaceSels.isEmpty() || _neutralFaceSels.getCount() != 1)
        {
            assert(false);
            return false;
        }

        // 下一步
        this->gotoStep(Step::InputAngle);
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

void DraftGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectDraftFaces:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DraftGuiCmd",
            "Select faces to draft. Press Enter or Spacebar to confirm. Press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
    }
    break;

    case Step::SelectNeutralFace:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DraftGuiCmd",
            "Select neutral face."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;

        // 中性面选择过滤器:不能选择拔模面
        assert(_pSelSetHighlightor);
        _pointPickOption.pSelFilter = std::make_shared<DraftGuiCmdNeutralFaceSelFilter>(
            _pSelSetHighlightor ? _pSelSetHighlightor->getSelectionSet() : wyap::SelectionSet());
    }
    break;

    case Step::InputAngle:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DraftGuiCmd", "Input draft angle."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 倒角对话框
        DoubleValueInputDialog::Options options;
        options.allowNegative = true;
        options.allowMax = wy3d::radiansToDegrees(wy3d::kMaxDraftAngle);
        options.allowMin = -options.allowMax;
        DoubleValueInputDialog dialog(1.0,
            QCoreApplication::translate("DraftGuiCmd", "Draft"),
            QCoreApplication::translate("DraftGuiCmd", "Angle"),
            options);
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset(); // 重置数据
            this->requestAbort(AbortCause::UserCancel);  // 结束命令
            return;
        }
        _angle = wy3d::degreesToRadians(dialog.getValue()); // 对话框逻辑中已经添加了校验数据的合理性

        // 执行拔模
        if (_neutralFaceSels.getCount() != 1)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return;
        }
        const wyap::Selection& neutralFaceSel = _neutralFaceSels.createIterator().current();
        unsigned int errorCode(0);
        if (!this->createDraft(_draftFacesSels, neutralFaceSel, _angle, errorCode)) // 无论执行成功与否,后续逻辑都会退出命令
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
        }

        // 重置数据
        this->reset();

        // 结束命令
        this->requestEnd();
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void DraftGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDraftFaces:
    {
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SelectNeutralFace:
    {
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
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

void DraftGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDraftFaces:
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            if (_pSelSetHighlightor->containsSelection(sel))
            {
                _pSelSetHighlightor->removeSelection(sel);
            }
            else
            {
                _pSelSetHighlightor->addSelection(sel);
            }
            _pointPickOption.pSelPreFilter = std::make_shared<DraftGuiCmdPreSelFilter>(
                _pSelSetHighlightor->getSelectionSet());
            _pPreview = nullptr;
            return;
        }
    }
    break;

    case Step::SelectNeutralFace:
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            if (_pSelSetHighlightor->containsSelection(sel)) // 中性面是要拔模的面
            {
                assert(false);
                return;
            }
            _pSelSetHighlightor_NeutralFace->addSelection(sel); // 高亮中性面

            _neutralFaceSels.clear();
            _neutralFaceSels.add(sel);

            _pPreview = nullptr;
            this->finishStep(_step);
            return;
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

void DraftGuiCmd::onEnterKey()
{
    if (Step::SelectDraftFaces == _step && _pSelSetHighlightor)
    {
        _draftFacesSels = _pSelSetHighlightor->getSelectionSet();
        if (!_draftFacesSels.isEmpty())
        {
            this->finishStep(_step);
        }
    }
}

void DraftGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool DraftGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectDraftFaces == _step;
}

void DraftGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool DraftGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectDraftFaces == _step;
}

void DraftGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectDraftFaces == _step)
    {
        if (_pSelSetHighlightor)
        {
            _pSelSetHighlightor->clearSelections();
        }
    }
}

bool DraftGuiCmd::createDraft(
    const wyap::SelectionSet& draftFaceSels,
    const wyap::Selection& neutralFaceSel,
    double angle, unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    if (draftFaceSels.isEmpty()) return false;
    if (neutralFaceSel.getElementId().isNull()) return false;

    // 执行拔模的实体
    const wy3d::Solid* pConstSolid(nullptr);
    for (auto iter = draftFaceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        pConstSolid = wy3d::Solid::cast(pDb->getElement(iter.current().getElementId()));
        break;
    }
    if (!pConstSolid) return false;
    wydb::ElementId solidId = pConstSolid->getId();

    // 根据选择集提取拔模面的索引
    std::vector<unsigned int> draftFaceIndices;
    draftFaceIndices.reserve(10);
    for (auto iter = draftFaceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getElementId() != solidId) // 在选择过滤器中已经确保了只能选择单一主体的面或边
        {
            assert(false);
            return false;
        }
        if (static_cast<wy3d::SelectionType>(sel.getSelectionType()) != wy3d::SelectionType::Face)
        {
            assert(false);
            return false;
        }

        const std::string& subPath = sel.getSubPath();
        if (subPath.empty())
        {
            assert(false);
            return false;
        }
        unsigned int faceIndex = std::stoul(subPath);
        draftFaceIndices.emplace_back(faceIndex);
    }
    assert(!draftFaceIndices.empty());

    // 中性面的索引
    if (static_cast<wy3d::SelectionType>(neutralFaceSel.getSelectionType()) != wy3d::SelectionType::Face)
    {
        assert(false);
        return false;
    }
    const std::string& subPath = neutralFaceSel.getSubPath();
    if (subPath.empty())
    {
        assert(false);
        return false;
    }
    unsigned int neutralFaceIndex = std::stoul(subPath);

    // 开启事务创建拔模
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(solidId));
    if (!pSolid)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    wy3d::Draft* pDraft(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Draft::create(pTrans, pSolid, neutralFaceIndex, draftFaceIndices, angle, pDraft))
    {
        errorCode = static_cast<unsigned int>(wy3d::ErrorCode::DRAFT_CreateDraftError);
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    pDb->getTransactionManager()->endTransaction();

    // 拔模已经创建成功但还需要查看有无错误码
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pDb->getTransactionManager()->getChainUpdateFeedback(pDraft->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    return true;
}
