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

#include "commands/modeling/solid/modification/SplitFaceGuiCmd.h"
#include <QCoreApplication>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSplitFace.h>
#include <wy3dSheet.h>
#include <wy3dSketch3D.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"

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

// 前置过滤器: 要分割的面必须来自同一个宿主
class SplitFacePreSelFilter : public SelectPreFilterFunctor
{
public:
    SplitFacePreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
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

SplitFaceGuiCmd::SplitFaceGuiCmd() : OsgGuiCommand(), _step(Step::Undefined)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult SplitFaceGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _pSelSetHighlightor_Faces = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectFaces);

    return wyap::CmdExecution::StartResult::Succeeded;
}

// 基类在 onEnd()/onAbort() 中调用: 命令结束(含 Esc 中止)时清干净高亮与预览
void SplitFaceGuiCmd::cleanup()
{
    _step = Step::Undefined;
    _faceSels.clear();
    _sketchId = wydb::ElementId::kNull;

    _pPreview = nullptr;
    _pointPickOption.pSelPreFilter = nullptr;
    _pointPickOption.pSelFilter = nullptr;
    if (_pSelSetHighlightor_Faces) _pSelSetHighlightor_Faces->clearSelections();
    _pSelSetHighlightor_Faces = nullptr;
}

bool SplitFaceGuiCmd::finishStep(Step step)
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

        // 下一步
        this->gotoStep(Step::SelectSketch);
        return true;
    }
    break;

    case Step::SelectSketch:
    {
        if (_sketchId.isNull())
        {
            assert(false);
            return false;
        }

        unsigned int errorCode(0);
        if (!this->createSplitFace(_faceSels, _sketchId, errorCode)) // 无论成败,后续逻辑都退出命令
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

void SplitFaceGuiCmd::gotoStep(Step step)
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

        // 拾取配置: 实体或片体的面
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) | static_cast<unsigned int>(ElementNodeType::Sheet);
        _pointPickOption.selType = wy3d::SelectionType::Face;
        _pointPickOption.acceptElement = false;
        _pointPickOption.pSelFilter = nullptr;
        _pointPickOption.pSelPreFilter = std::make_shared<SplitFacePreSelFilter>(
            _pSelSetHighlightor_Faces ? _pSelSetHighlightor_Faces->getSelectionSet() : wyap::SelectionSet());

        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SplitFaceGuiCmd",
            "Select the face to split. Press Enter or Spacebar to confirm."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
    }
    break;

    case Step::SelectSketch:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 拾取配置: 整张3D草图(点草图里的任意一条曲线选中的就是它所属的草图)
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch3D);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.acceptElement = true;
        _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Sketch3D::classInfo());
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::Sketch3D::classInfo());

        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SplitFaceGuiCmd",
            "Select the 3D sketch to split the face with."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
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

void SplitFaceGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectFaces:
    case Step::SelectSketch:
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

void SplitFaceGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectFaces:
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            if (_pSelSetHighlightor_Faces->containsSelection(sel))
            {
                _pSelSetHighlightor_Faces->removeSelection(sel);
            }
            else
            {
                _pSelSetHighlightor_Faces->addSelection(sel);
            }
            _faceSels = _pSelSetHighlightor_Faces->getSelectionSet();
            _pointPickOption.pSelPreFilter = std::make_shared<SplitFacePreSelFilter>(_faceSels);
            _pPreview = nullptr;
            return;
        }
    }
    break;

    case Step::SelectSketch:
    {
        if (_pPreview)
        {
            const bool picked = this->pickSketch(_pPreview->getSelection());
            _pPreview = nullptr;
            // 草图就是这一步的全部输入, 点中即执行(同 FilledSheet 草图那一步)
            if (picked) this->finishStep(_step);
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

bool SplitFaceGuiCmd::pickSketch(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    if (!wy3d::Sketch3D::cast(pDb->getElement(sel.getElementId())))
    {
        assert(false);
        return false;
    }

    _sketchId = sel.getElementId();
    return true;
}

void SplitFaceGuiCmd::onEnterKey()
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

    case Step::SelectSketch:
        // 这一步点中草图就已经执行完了, 没有要确认的东西
        break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void SplitFaceGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool SplitFaceGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectFaces == _step;
}

void SplitFaceGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool SplitFaceGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectFaces == _step;
}

void SplitFaceGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectFaces == _step)
    {
        if (_pSelSetHighlightor_Faces) _pSelSetHighlightor_Faces->clearSelections();
        _faceSels.clear();
        _pointPickOption.pSelPreFilter = std::make_shared<SplitFacePreSelFilter>(_faceSels);
    }
}

bool SplitFaceGuiCmd::createSplitFace(
    const wyap::SelectionSet& faceSels,
    const wydb::ElementId& sketchId,
    unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    if (faceSels.isEmpty()) return false;
    if (sketchId.isNull()) return false;

    // 要分割的面所属的宿主: 实体或片体
    wydb::ElementId hostId = wydb::ElementId::kNull;
    for (auto iter = faceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        hostId = iter.current().getElementId();
        break;
    }
    if (hostId.isNull()) return false;
    const wydb::Element* pHostElem = pDb->getElement(hostId);
    if (!wy3d::Solid::cast(pHostElem) && !wy3d::Sheet::cast(pHostElem)) return false;

    // 根据选择集提取要分割的面的索引
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
        if (static_cast<wy3d::SelectionType>(sel.getSelectionType()) != wy3d::SelectionType::Face)
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

    // 开启事务创建分割面
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Feature* pHost = wy3d::Feature::cast(pTrans->getElementForWrite(hostId));
    wy3d::Solid* pSolidHost = pHost ? wy3d::Solid::cast(pHost) : nullptr;
    wy3d::Sheet* pSheetHost = (pHost && !pSolidHost) ? wy3d::Sheet::cast(pHost) : nullptr;
    // 工具草图要收到本特征名下, 所以拿的是本次事务的写副本
    wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
    if ((!pSolidHost && !pSheetHost) || !pSketch3D)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    // 宿主是实体还是片体决定了走哪个重载
    wy3d::SplitFace* pSplitFace(nullptr);
    const wy::ErrorStatus createStatus = pSolidHost
        ? wy3d::SplitFace::create(pTrans, pSolidHost, faceIndices, pSketch3D, pSplitFace)
        : wy3d::SplitFace::create(pTrans, pSheetHost, faceIndices, pSketch3D, pSplitFace);
    if (wy::ErrorStatus::Ok != createStatus)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    pDb->getTransactionManager()->endTransaction();

    // 分割面已经创建成功但还需要查看有无错误码
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pDb->getTransactionManager()->getChainUpdateFeedback(pSplitFace->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    return true;
}
