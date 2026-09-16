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

#include "commands/modeling/solid/modification/DeleteFaceGuiCmd.h"
#include <cassert>
#include <cstdint>
#include <QCoreApplication>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dDeleteFace.h>
#include <wy3dSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/MessageBoxUtil.h"

namespace
{

// std::stoul throws on a malformed sub-path, a bad value only fails the pick
bool parseSubPathIndex(const std::string& subPath, unsigned int& index)
{
    if (subPath.empty()) return false;
    unsigned long long value(0);
    for (char c : subPath)
    {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<unsigned long long>(c - '0');
        if (value > 0xFFFFFFFFull) return false;
    }
    index = static_cast<unsigned int>(value);
    return true;
}

// 前置过滤器: 要删除的面必须来自同一个宿主
class DeleteFacePreSelFilter : public SelectPreFilterFunctor
{
public:
    DeleteFacePreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            _targetElemId = iter.current().getElementId();
            break;
        }
    }

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

} // namespace

DeleteFaceGuiCmd::DeleteFaceGuiCmd() : OsgGuiCommand(), _step(Step::Undefined)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult DeleteFaceGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _pSelSetHighlightor_Faces = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectFaces);

    return wyap::CmdExecution::StartResult::Succeeded;
}

// 基类在 onEnd()/onAbort() 中调用: 命令结束(含 Esc 中止)时清干净高亮与预览
void DeleteFaceGuiCmd::cleanup()
{
    _step = Step::Undefined;
    _faceSels.clear();

    _pPreview = nullptr;
    _pointPickOption.pSelPreFilter = nullptr;
    if (_pSelSetHighlightor_Faces) _pSelSetHighlightor_Faces->clearSelections();
    _pSelSetHighlightor_Faces = nullptr;
}

bool DeleteFaceGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectFaces:
    {
        if (_faceSels.isEmpty())
        {
            assert(false);
            return false;
        }

        unsigned int errorCode(0);
        if (!this->createDeleteFace(_faceSels, errorCode)) // 无论成败,后续逻辑都退出命令
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
        }

        // 重置数据
        this->cleanup();

        // 结束命令
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

void DeleteFaceGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectFaces:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 拾取配置: 只能是片体的面
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sheet);
        _pointPickOption.selType = wy3d::SelectionType::SolidFace;
        _pointPickOption.acceptElement = false;
        _pointPickOption.pSelFilter = nullptr;
        _pointPickOption.pSelPreFilter = std::make_shared<DeleteFacePreSelFilter>(
            _pSelSetHighlightor_Faces ? _pSelSetHighlightor_Faces->getSelectionSet() : wyap::SelectionSet());

        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DeleteFaceGuiCmd",
            "Select the faces to delete. Press Enter or Spacebar to confirm."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void DeleteFaceGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectFaces:
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

void DeleteFaceGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectFaces:
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            if (_pSelSetHighlightor_Faces->containsSelection(sel)) // 再点一次取消选中
            {
                _pSelSetHighlightor_Faces->removeSelection(sel);
            }
            else
            {
                _pSelSetHighlightor_Faces->addSelection(sel);
            }
            _faceSels = _pSelSetHighlightor_Faces->getSelectionSet();
            _pointPickOption.pSelPreFilter = std::make_shared<DeleteFacePreSelFilter>(_faceSels);
            _pPreview = nullptr;
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

void DeleteFaceGuiCmd::onEnterKey()
{
    switch (_step)
    {
    case Step::SelectFaces:
    {
        if (Step::SelectFaces == _step && _pSelSetHighlightor_Faces)
        {
            _faceSels = _pSelSetHighlightor_Faces->getSelectionSet();
            if (!_faceSels.isEmpty())
            {
                this->finishStep(_step);
            }
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void DeleteFaceGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool DeleteFaceGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectFaces == _step;
}

void DeleteFaceGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool DeleteFaceGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectFaces == _step;
}

void DeleteFaceGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectFaces == _step)
    {
        if (_pSelSetHighlightor_Faces) _pSelSetHighlightor_Faces->clearSelections();
        _faceSels.clear();
        _pointPickOption.pSelPreFilter = std::make_shared<DeleteFacePreSelFilter>(_faceSels);
    }
}

bool DeleteFaceGuiCmd::createDeleteFace(const wyap::SelectionSet& faceSels, unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    if (faceSels.isEmpty()) return false;

    // 要删除的面所属的宿主: 片体
    wydb::ElementId hostId = wydb::ElementId::kNull;
    for (auto iter = faceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        hostId = iter.current().getElementId();
        break;
    }
    if (hostId.isNull()) return false;
    const wydb::Element* pHostElem = pDb->getElement(hostId);
    if (!wy3d::Sheet::cast(pHostElem)) return false;

    // 根据选择集提取要删除的面的索引
    std::vector<std::uint32_t> faceIndices;
    faceIndices.reserve(faceSels.getCount());
    for (auto iter = faceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getElementId() != hostId) // 在选择过滤器中已经确保了只能选择同一宿主的
        {
            assert(false);
            return false;
        }
        if (static_cast<wy3d::SelectionType>(sel.getSelectionType()) != wy3d::SelectionType::SolidFace)
        {
            assert(false);
            return false;
        }

        unsigned int faceIndex(0);
        if (!parseSubPathIndex(sel.getSubPath(), faceIndex))
        {
            assert(false);
            return false;
        }
        faceIndices.emplace_back(static_cast<std::uint32_t>(faceIndex));
    }
    assert(!faceIndices.empty());

    // 开启事务创建删除面
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(hostId));
    if (!pSheet)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    wy3d::DeleteFace* pDeleteFace(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::DeleteFace::create(pTrans, pSheet, faceIndices, pDeleteFace))
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    pDb->getTransactionManager()->endTransaction();

    // 删除面已经创建成功但还需要查看有无错误码
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pDb->getTransactionManager()->getChainUpdateFeedback(pDeleteFace->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    return true;
}
