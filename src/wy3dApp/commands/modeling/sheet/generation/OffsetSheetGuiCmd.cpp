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

#include "OffsetSheetGuiCmd.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <QCoreApplication>
#include <QOpenGLWidget>

#include <BRep_Builder.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dSolid.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dOffsetSheet.h>
#include <utils/wy3dSheetOffsetUtil.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/Colors.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "commands/dialogs/OffsetSheetCmdPanel.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/featureTree/FeatureTreeWidget.h"

static constexpr double kDefaultOffset = 2.0;

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

// 一个宿主实体的一组要偏置的面: 面索引只在那个形体自己的面序里有效
struct OffsetFaceGroup
{
    wydb::ElementId hostId;
    std::vector<std::uint32_t> faceIndices;
};

// 已选面按宿主分组(实体面可以跨实体, 一张曲面装下它们全部)
void collectFaceGroups(const wyap::SelectionSet& faceSels, std::vector<OffsetFaceGroup>& groups)
{
    groups.clear();
    for (auto iter = faceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (static_cast<wy3d::SelectionType>(sel.getSelectionType()) != wy3d::SelectionType::SolidFace)
        {
            assert(false);
            continue;
        }
        unsigned int faceIndex(0);
        if (!parseSubPathIndex(sel.getSubPath(), faceIndex))
        {
            assert(false);
            continue;
        }

        const wydb::ElementId hostId = sel.getElementId();
        auto groupIter = std::find_if(groups.begin(), groups.end(),
            [&hostId](const OffsetFaceGroup& group) { return group.hostId == hostId; });
        if (groups.end() == groupIter)
        {
            groups.emplace_back(OffsetFaceGroup{ hostId, std::vector<std::uint32_t>() });
            groupIter = groups.end() - 1;
        }
        groupIter->faceIndices.emplace_back(static_cast<std::uint32_t>(faceIndex));
    }
}

// 片体模式只有一个宿主, 面索引就是一族
std::vector<std::uint32_t> collectFaceIndices(const wyap::SelectionSet& faceSels)
{
    std::vector<OffsetFaceGroup> groups;
    collectFaceGroups(faceSels, groups);
    assert(groups.size() <= 1);
    if (groups.empty()) return std::vector<std::uint32_t>();
    return groups.front().faceIndices;
}

// 形体上全部面的选择(0 基索引, 与点选子路径同序)
void collectAllFaceSelections(
    const TopoDS_Shape& shape, const wydb::ElementId& id, wyap::SelectionSet& sels)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    for (int index = 1; index <= faceMap.Extent(); ++index)
    {
        sels.add(wyap::Selection(
            static_cast<unsigned int>(wy3d::SelectionType::SolidFace),
            id, std::to_string(index - 1)));
    }
}

// 前置过滤器: 种类定下来之后, 片体模式只许那一个片体, 实体模式只许实体
class OffsetSheetPreSelFilter : public SelectPreFilterFunctor
{
public:
    OffsetSheetPreSelFilter(OffsetSheetGuiCmd::HostKind hostKind, const wydb::ElementId& hostId)
        : _hostKind(hostKind), _hostId(hostId) {}

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        if (id.isNull()) return SelectFilterStatus::Continue;

        if (OffsetSheetGuiCmd::HostKind::Undefined == _hostKind)
        {
            return SelectFilterStatus::Ok;
        }
        else if (OffsetSheetGuiCmd::HostKind::Sheet == _hostKind)
        {
            return id == _hostId ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;
        }
        else
        {
            return wy3d::Solid::cast(pDb->getElement(id))
                ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;
        }
    }

private:
    OffsetSheetGuiCmd::HostKind _hostKind;
    wydb::ElementId _hostId;
};

} // namespace

// ============================================================================
// OffsetSheetGuiCmd::MakeOffsetSheet
// ============================================================================

// 片体偏置的实时预览: 选定宿主即创建, 距离能实时改.
// 目标只在创建时定一次(OffsetSheet 没有改目标的接口), 所以拾取一变就擦旧建新
class OffsetSheetGuiCmd::MakeOffsetSheet : public GuiCmdMakeElement
{
public:
    MakeOffsetSheet(GuiCommand* pGuiCmd);
    ~MakeOffsetSheet();

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 选定宿主, 按整个片体或给定的面索引与距离创建
    bool init(const wydb::ElementId& sheetId, bool isWholeSheet,
        const std::vector<std::uint32_t>& faceIndices, double offset, unsigned int& errorCode);
    // 换成这一套目标: 老的擦掉再建(与无参曲面预览同法). 面模式而面索引为空时只擦不建
    bool rebuild(bool isWholeSheet, const std::vector<std::uint32_t>& faceIndices,
        double offset, unsigned int& errorCode);
    bool updateOffset(double offset);

    wy3d::OffsetSheet* getOffsetSheet() const { return _pOffsetSheet; }

private:
    wy3d::OffsetSheet* _pOffsetSheet;
    wydb::ElementId _sheetId;
};

OffsetSheetGuiCmd::MakeOffsetSheet::MakeOffsetSheet(GuiCommand* pGuiCmd)
    : GuiCmdMakeElement(pGuiCmd), _pOffsetSheet(nullptr), _sheetId(wydb::ElementId::kNull)
{
}

OffsetSheetGuiCmd::MakeOffsetSheet::~MakeOffsetSheet() {}

void OffsetSheetGuiCmd::MakeOffsetSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pOffsetSheet) idSet.insert(_pOffsetSheet->getId());
}

bool OffsetSheetGuiCmd::MakeOffsetSheet::init(const wydb::ElementId& sheetId, bool isWholeSheet,
    const std::vector<std::uint32_t>& faceIndices, double offset, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pOffsetSheet || _isFinished) return false;
    if (sheetId.isNull()) return false;
    if (!isWholeSheet && faceIndices.empty()) return false;
    if (std::fabs(offset) < wy3d::kMinValue ||
        std::fabs(offset) > wy3d::kMaxValue) return false;

    const wydb::Element* pElem = _pDb->getElement(sheetId);
    if (!pElem) return false;
    if (!wy3d::Sheet::cast(pElem)) return false;

    wy3d::OffsetSheet* pObj = nullptr;
    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
    if (!pSheet) { pTransMgr->abortTransaction(); return false; }

    const wy::ErrorStatus createStatus = isWholeSheet
        ? wy3d::OffsetSheet::create(pTrans, pSheet, offset, pObj)
        : wy3d::OffsetSheet::create(pTrans, pSheet, faceIndices, offset, pObj);
    if (wy::ErrorStatus::Ok != createStatus || !pObj)
    {
        pTransMgr->abortTransaction();
        return false;
    }
    if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
    {
        assert(false);
        return false;
    }

    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pTransMgr->getChainUpdateFeedback(pObj->getId()).get());
    if (0 != errorCode) return false;

    _pOffsetSheet = pObj;
    _sheetId = sheetId;
    return true;
}

// 目标换成这一套: 老的擦掉再建(与无参曲面预览同法). 面模式而面索引为空时只擦不建 ——
// 目标定下来之前库里不留特征
bool OffsetSheetGuiCmd::MakeOffsetSheet::rebuild(bool isWholeSheet, const std::vector<std::uint32_t>& faceIndices,
    double offset, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _isFinished) return false;
    if (_sheetId.isNull()) return false;

    const bool wantsFeature = isWholeSheet || !faceIndices.empty();
    if (wantsFeature &&
        (std::fabs(offset) < wy3d::kMinValue || std::fabs(offset) > wy3d::kMaxValue)) return false;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;

    const wydb::ElementId oldId = _pOffsetSheet ? _pOffsetSheet->getId() : wydb::ElementId::kNull;
    wy3d::OffsetSheet* pNew(nullptr);
    {
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(_sheetId));
        if (!pSheet)
        {
            assert(false);
            pTransMgr->abortTransaction();
            return false;
        }

        if (!oldId.isNull())
        {
            wydb::Element* pOldOffsetSheet = pTrans->getElementForWrite(oldId);
            if (pOldOffsetSheet) pOldOffsetSheet->erase();
        }

        if (wantsFeature)
        {
            const wy::ErrorStatus createStatus = isWholeSheet
                ? wy3d::OffsetSheet::create(pTrans, pSheet, offset, pNew)
                : wy3d::OffsetSheet::create(pTrans, pSheet, faceIndices, offset, pNew);
            if (wy::ErrorStatus::Ok != createStatus || !pNew)
            {
                assert(false);
                pTransMgr->abortTransaction();
                return false;
            }
        }
    }
    if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
    {
        assert(false);
        return false;
    }
    pTransMgr->mergeTransaction();

    // 事务成了才认这个预览: 回滚会让旧元素复活, 那时指针不能动
    _pOffsetSheet = pNew;
    if (_pOffsetSheet)
    {
        errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pTransMgr->getChainUpdateFeedback(_pOffsetSheet->getId()).get());
    }
    return true;
}

bool OffsetSheetGuiCmd::MakeOffsetSheet::updateOffset(double offset)
{
    if (!_pDb || !_pTopTrans || !_pOffsetSheet || _isFinished) return false;
    if (std::fabs(offset) < wy3d::kMinValue ||
        std::fabs(offset) > wy3d::kMaxValue) return false;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;

    if (wy::ErrorStatus::Ok != _pOffsetSheet->upgradeForWrite() ||
        wy::ErrorStatus::Ok != _pOffsetSheet->setOffset(offset))
    {
        assert(false);
        pTransMgr->abortTransaction();
        return false;
    }
    if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
    {
        assert(false);
        return false;
    }
    pTransMgr->mergeTransaction();
    return true;
}

class OffsetSheetGuiCmd::MakeSolidFaceOffsetSheet : public GuiCmdMakeElement
{
public:
    MakeSolidFaceOffsetSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pSheet(nullptr) {}
    ~MakeSolidFaceOffsetSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override
    {
        if (_pSheet) idSet.insert(_pSheet->getId());
    }

    // 命令里拿它上预览色
    wy3d::NonParametricSheet* getSheet() const { return _pSheet; }

    // 按宿主分组重算预览; errorCode 非 0 表示有组没偏置出来(算出来的照样预览)
    bool update(const std::vector<OffsetFaceGroup>& groups, double offset, unsigned int& errorCode)
    {
        errorCode = 0;
        if (!_pDb || !_pTopTrans || _isFinished) return false;

        // 先算形体再改库: 几何原因失败只记错误码, 不打断别的组
        std::vector<TopoDS_Shape> regions;
        for (const OffsetFaceGroup& group : groups)
        {
            if (group.faceIndices.empty()) continue;

            const wy3d::Solid* pSolid = wy3d::Solid::cast(_pDb->getElement(group.hostId));
            if (!pSolid)
            {
                assert(false);
                continue;
            }

            TopoDS_Shape offsetShape;
            const wy3d::ErrorCode error = wy3d::SheetOffsetUtil::makeOffsetShape(
                pSolid->getShape(), group.faceIndices, offset, offsetShape);
            if (wy3d::ErrorCode::NoError != error)
            {
                if (0 == errorCode) errorCode = static_cast<unsigned int>(error);
                continue;
            }
            for (TopExp_Explorer ex(offsetShape, TopAbs_ShapeEnum::TopAbs_SHELL); ex.More(); ex.Next())
            {
                regions.emplace_back(ex.Current());
            }
        }

        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        wydb::Transaction* pTrans = pTransMgr->startTransaction();
        if (!pTrans) return false;

        // 预览元素常驻: 已经有一张就只换它的源形体, 算不出东西才把预览撤掉
        wy3d::NonParametricSheet* pNewSheet = _pSheet;
        {
            if (regions.empty())
            {
                if (_pSheet)
                {
                    wydb::Element* pOldSheet = pTrans->getElementForWrite(_pSheet->getId());
                    if (pOldSheet) pOldSheet->erase();
                }
                pNewSheet = nullptr;
            }
            else if (_pSheet)
            {
                if (wy::ErrorStatus::Ok != _pSheet->upgradeForWrite() ||
                    wy::ErrorStatus::Ok != _pSheet->setSourceShape(makeSheetShape(regions)))
                {
                    assert(false);
                    pTransMgr->abortTransaction();
                    return false;
                }
            }
            else
            {
                wy3d::NonParametricSheet* pCreatedSheet(nullptr);
                if (wy::ErrorStatus::Ok != wy3d::NonParametricSheet::create(
                    pTrans, makeSheetShape(regions), pCreatedSheet) || !pCreatedSheet)
                {
                    assert(false);
                    pTransMgr->abortTransaction();
                    return false;
                }
                pNewSheet = pCreatedSheet;
            }
        }
        if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
        {
            assert(false);
            return false;
        }
        pTransMgr->mergeTransaction();

        // 事务成了才认这张新预览: 回滚会把旧的「复活」, 那时不能再留着旧指针
        _pSheet = pNewSheet;
        if (_pSheet && 0 == errorCode)
        {
            errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
                pTransMgr->getChainUpdateFeedback(_pSheet->getId()).get());
        }
        return true;
    }

private:
    // 一张壳就是它, 多张壳拼成一个复合体(不嵌套复合体)
    static TopoDS_Shape makeSheetShape(const std::vector<TopoDS_Shape>& shells)
    {
        if (1 == shells.size()) return shells.front();

        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        for (const TopoDS_Shape& shell : shells)
        {
            builder.Add(compound, shell);
        }
        return compound;
    }

    wy3d::NonParametricSheet* _pSheet;
};

OffsetSheetGuiCmd::OffsetSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined),
    _hostKind(HostKind::Undefined),
    _hostId(wydb::ElementId::kNull),
    _targetMode(static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces)),
    _previewErrorCode(0),
    _pCmdPanel(nullptr),
    _offset(kDefaultOffset)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

OffsetSheetGuiCmd::~OffsetSheetGuiCmd() {}

wyap::CmdExecution::StartResult OffsetSheetGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // The tree hover preview repaints the whole element and would wipe the per-face colors
    Application::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setHoverPreviewEnabled(false);

    _pSelSetHighlightor_Faces = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    wydb::ElementId sheetId(wydb::ElementId::kNull);
    if (this->isValidSheetSelectionSet(ss, sheetId) && !sheetId.isNull())
    {
        // 预选了一个顶层片体: 整个片体偏置
        this->startWithWholeSheet(sheetId);
        return wyap::CmdExecution::StartResult::Succeeded;
    }

    // 其余情况一律直接选面: 预选的面当已选面用, 第一个面定宿主种类
    _targetMode = static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces);
    this->adoptPreSelectedFaces(ss);

    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    pSelMgr->beginChange();
    pSelMgr->clearSelections();
    pSelMgr->endChange();

    this->updatePickOption();
    if (!this->createCmdPanel())
    {
        this->requestAbort(AbortCause::ErrorTerminate);
        return wyap::CmdExecution::StartResult::Succeeded;
    }
    this->gotoStep(Step::SetParameters);

    if (!_faceSels.isEmpty())
    {
        unsigned int errorCode(0);
        wyap::Selection firstSel = _faceSels.createIterator().current();
        if (!this->resolveHostFromPickedFaces(firstSel, errorCode))
        {
            if (0 != errorCode) MessageBoxUtil::showError(errorCode);
            this->requestAbort(AbortCause::ErrorTerminate);
        }
        else
        {
            this->applyTarget();
        }
    }
    return wyap::CmdExecution::StartResult::Succeeded;
}

void OffsetSheetGuiCmd::cleanup()
{
    _step = Step::Undefined;
    _hostKind = HostKind::Undefined;
    _hostId = wydb::ElementId::kNull;
    _targetMode = static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces);
    _previewErrorCode = 0;
    _offset = kDefaultOffset;
    _faceSels.clear();

    _pMakeOffsetSheet = nullptr;
    _pMakeSolidSheet = nullptr;
    _pPreview = nullptr;
    _pointPickOption.pSelPreFilter = nullptr;
    _pointPickOption.pSelFilter = nullptr;
    if (_pSelSetHighlightor_Faces) _pSelSetHighlightor_Faces->clearSelections();
    _pSelSetHighlightor_Faces = nullptr;
    _pSelSetHighlightor_TargetFaces = nullptr;
    _pSelSetHighlightor_NewFaces = nullptr;
    this->destroyCmdPanel();

    Application::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setHoverPreviewEnabled(true);
}

void OffsetSheetGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SetParameters:
    {
        this->updatePrompt();
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void OffsetSheetGuiCmd::updatePrompt()
{
    if (static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces) == _targetMode)
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("OffsetSheetGuiCmd",
            "Select the faces to offset, then OK to apply."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    else
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("OffsetSheetGuiCmd",
            "Specify the offset distance, then OK to apply."));
        Application::instance().setCursor(CursorType::Select);
    }
}

void OffsetSheetGuiCmd::updatePickOption()
{
    if (static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet) == _targetMode)
    {
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sheet);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.acceptElement = true;
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::Sheet::classInfo());
        _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(
            wy3d::Sheet::classInfo());
    }
    else
    {
        // 种类没定之前实体与片体的面都收, 定下来就只收那一类
        unsigned int pickMask = static_cast<unsigned int>(ElementNodeType::Solid) |
            static_cast<unsigned int>(ElementNodeType::Sheet);
        if (HostKind::Sheet == _hostKind)
            pickMask = static_cast<unsigned int>(ElementNodeType::Sheet);
        else if (HostKind::Solid == _hostKind)
            pickMask = static_cast<unsigned int>(ElementNodeType::Solid);

        _pointPickOption.pickMask = pickMask;
        _pointPickOption.selType = wy3d::SelectionType::SolidFace;
        _pointPickOption.acceptElement = false;
        _pointPickOption.pSelPreFilter = std::make_shared<OffsetSheetPreSelFilter>(_hostKind, _hostId);
        _pointPickOption.pSelFilter = nullptr;
    }
    _pPreview = nullptr;
}

void OffsetSheetGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SetParameters:
    {
        if (static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces) != _targetMode) break;

        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        Application::instance().setCursor(
            _pPreview ? CursorType::SelectElements : CursorType::Forbid);
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void OffsetSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SetParameters:
    {
        if (static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces) != _targetMode) break;
        if (!_pPreview || !_pSelSetHighlightor_Faces) break;

        const wyap::Selection sel = _pPreview->getSelection();
        if (_pSelSetHighlightor_Faces->containsSelection(sel)) // 再点一次取消选中
        {
            _pSelSetHighlightor_Faces->removeSelection(sel);
        }
        else
        {
            _pSelSetHighlightor_Faces->addSelection(sel);
        }
        _faceSels = _pSelSetHighlightor_Faces->getSelectionSet();
        _pPreview = nullptr;

        if (HostKind::Undefined == _hostKind && !_faceSels.isEmpty())
        {
            // 第一个面定种类, 同时把预览建起来
            unsigned int errorCode(0);
            if (!this->resolveHostFromPickedFaces(sel, errorCode))
            {
                if (0 != errorCode) MessageBoxUtil::showError(errorCode);
                this->requestAbort(AbortCause::ErrorTerminate);
                break;
            }
            this->updatePickOption();
        }
        this->applyTarget();
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

bool OffsetSheetGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SetParameters == _step &&
        static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces) == _targetMode;
}

void OffsetSheetGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SetParameters != _step) return;
    if (static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces) != _targetMode) return;

    this->clearFaceSelections();
    this->applyTarget();
}

bool OffsetSheetGuiCmd::isValidSheetSelectionSet(
    const wyap::SelectionSet& ss, wydb::ElementId& sheetId)
{
    sheetId = wydb::ElementId::kNull;
    if (ss.getCount() != 1) return false;
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(
            wy3d::SelectionType::Element)) return false;
    wydb::ElementId id = sel.getElementId();
    if (id.isNull() || !isValidSheet(id)) return false;
    sheetId = id;
    return true;
}

bool OffsetSheetGuiCmd::isValidSheet(const wydb::ElementId& sheetId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    if (!pSheet || !pSheet->getParent().isNull()) return false;
    return true;
}

bool OffsetSheetGuiCmd::startWithWholeSheet(const wydb::ElementId& sheetId)
{
    _hostKind = HostKind::Sheet;
    _hostId = sheetId;
    _targetMode = static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet);

    // 目标改由面级高亮标出, 不用选择集(元素级高亮会把边一起染绿)
    this->clearHostSelection();

    this->updatePickOption();

    // create element with the current distance
    _pMakeOffsetSheet = std::make_shared<MakeOffsetSheet>(this);
    unsigned int errorCode(0);
    if (!_pMakeOffsetSheet->init(_hostId, true,
            std::vector<std::uint32_t>(), _offset, errorCode))
    {
        _pMakeOffsetSheet = nullptr;
        this->clearHostSelection();
        if (0 != errorCode) MessageBoxUtil::showError(errorCode);
        this->requestAbort(AbortCause::ErrorTerminate);
        return false;
    }

    this->refreshFaceColors();

    // show dialog
    if (!this->createCmdPanel())
    {
        _pMakeOffsetSheet = nullptr;
        this->requestAbort(AbortCause::ErrorTerminate);
        return false;
    }

    this->gotoStep(Step::SetParameters);
    return true;
}

void OffsetSheetGuiCmd::adoptPreSelectedFaces(const wyap::SelectionSet& ss)
{
    std::vector<wyap::Selection> adopted;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (static_cast<wy3d::SelectionType>(sel.getSelectionType()) != wy3d::SelectionType::SolidFace)
            continue;

        wydb::ElementId hostId(wydb::ElementId::kNull);
        const HostKind hostKind = this->hostKindOf(sel, hostId);
        if (HostKind::Undefined == hostKind) continue;

        if (HostKind::Undefined == _hostKind)
        {
            _hostKind = hostKind;
            _hostId = hostId;
        }
        else if (hostKind != _hostKind) continue;
        else if (HostKind::Sheet == _hostKind && hostId != _hostId) continue;

        adopted.emplace_back(sel);
    }

    for (const wyap::Selection& sel : adopted)
    {
        _pSelSetHighlightor_Faces->addSelection(sel);
    }
    _faceSels = _pSelSetHighlightor_Faces->getSelectionSet();
}

OffsetSheetGuiCmd::HostKind OffsetSheetGuiCmd::hostKindOf(
    const wyap::Selection& sel, wydb::ElementId& hostId)
{
    hostId = wydb::ElementId::kNull;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return HostKind::Undefined;

    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    if (!pElem) return HostKind::Undefined;

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (pSolid)
    {
        return HostKind::Solid;
    }
    if (wy3d::Sheet::cast(pElem))
    {
        hostId = sel.getElementId();
        return HostKind::Sheet;
    }
    return HostKind::Undefined;
}

bool OffsetSheetGuiCmd::resolveHostFromPickedFaces(
    const wyap::Selection& firstSel, unsigned int& errorCode)
{
    errorCode = 0;
    if (_faceSels.isEmpty()) return false;

    if (HostKind::Undefined == _hostKind)
    {
        wydb::ElementId hostId(wydb::ElementId::kNull);
        _hostKind = this->hostKindOf(firstSel, hostId);
        _hostId = hostId;
    }
    if (HostKind::Undefined == _hostKind) return false;

    // 宿主定下来了: 实体面入口不校验小值(小到阈值以下就是复制面), 片体入口仍要求真偏
    if (_pCmdPanel) _pCmdPanel->setSmallValueAllowed(HostKind::Solid == _hostKind);

    if (HostKind::Solid == _hostKind)
    {
        // 一张无参曲面装下所有实体面的偏置: 预览持有者管擦旧建新
        _pMakeSolidSheet = std::make_shared<MakeSolidFaceOffsetSheet>(this);
        return true;
    }

    _pMakeOffsetSheet = std::make_shared<MakeOffsetSheet>(this);
    if (!_pMakeOffsetSheet->init(_hostId, false,
            collectFaceIndices(_faceSels), _offset, errorCode))
    {
        _pMakeOffsetSheet = nullptr;
        return false;
    }
    return true;
}

bool OffsetSheetGuiCmd::applyTarget()
{
    _previewErrorCode = 0;

    bool result(false);
    if (static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet) == _targetMode)
    {
        result = _pMakeOffsetSheet &&
            _pMakeOffsetSheet->rebuild(true, std::vector<std::uint32_t>(), _offset, _previewErrorCode);
    }
    else if (HostKind::Sheet == _hostKind)
    {
        result = _pMakeOffsetSheet &&
            _pMakeOffsetSheet->rebuild(false, collectFaceIndices(_faceSels), _offset, _previewErrorCode);
    }
    else if (_pMakeSolidSheet)
    {
        std::vector<OffsetFaceGroup> groups;
        collectFaceGroups(_faceSels, groups);
        result = _pMakeSolidSheet->update(groups, _offset, _previewErrorCode);
    }

    // 形体一改宿主节点就重生, 面上的高亮与新面预览色都会没, 改完要重挂
    this->refreshFaceColors();
    return result;
}

void OffsetSheetGuiCmd::clearFaceSelections()
{
    if (_pSelSetHighlightor_Faces) _pSelSetHighlightor_Faces->clearSelections();
    _faceSels.clear();
    _pPreview = nullptr;
}

void OffsetSheetGuiCmd::refreshFaceColors()
{
    // 目标面高亮(整片体入口): 元素级高亮会把边一起染绿, 也会盖掉面上的颜色, 所以标在面上
    _pSelSetHighlightor_TargetFaces = nullptr;
    if (static_cast<int>(wy3d::OffsetSheet::Target::WholeSheet) == _targetMode)
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        const wy3d::Sheet* pHostSheet = pDb
            ? wy3d::Sheet::cast(pDb->getElement(_hostId)) : nullptr;
        if (pHostSheet)
        {
            wyap::SelectionSet targetFaceSels;
            collectAllFaceSelections(pHostSheet->getShape(), _hostId, targetFaceSels);
            if (!targetFaceSels.isEmpty())
            {
                _pSelSetHighlightor_TargetFaces =
                    std::make_shared<SelectionSetHighlightor>(targetFaceSels);
            }
        }
    }

    // 新面预览色: 每次整套重建(高亮器析构即撤色), 照 DraftGuiCmd 的中性面
    _pSelSetHighlightor_NewFaces = nullptr;

    wyap::SelectionSet newFaceSels;
    // 片体面: 宿主上追加的那几片新面
    wy3d::OffsetSheet* pOffsetSheet = _pMakeOffsetSheet
        ? _pMakeOffsetSheet->getOffsetSheet() : nullptr;
    if (pOffsetSheet)
    {
        for (std::uint32_t faceIndex : pOffsetSheet->getNewFaceIndices())
        {
            newFaceSels.add(wyap::Selection(
                static_cast<unsigned int>(wy3d::SelectionType::SolidFace),
                _hostId, std::to_string(faceIndex)));
        }
    }
    // 实体面: 预览的那张无参曲面整片都是命令生成的
    else if (_pMakeSolidSheet)
    {
        const wy3d::NonParametricSheet* pPreviewSheet = _pMakeSolidSheet->getSheet();
        if (pPreviewSheet)
        {
            collectAllFaceSelections(
                pPreviewSheet->getShape(), pPreviewSheet->getId(), newFaceSels);
        }
    }
    if (!newFaceSels.isEmpty())
    {
        _pSelSetHighlightor_NewFaces =
            std::make_shared<SelectionSetHighlightor>(newFaceSels, Colors::kSheetFace_New);
    }

    if (!_pSelSetHighlightor_Faces) return;

    // 拾取高亮后挂, 同一个面两种状态时以拾取为准; 高亮器不重复挂已在集合里的项, 只能先全撤再重挂
    wyap::SelectionSet faceSels;
    faceSels = _faceSels;
    _pSelSetHighlightor_Faces->clearSelections();
    for (auto iter = faceSels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        _pSelSetHighlightor_Faces->addSelection(iter.current());
    }
    _faceSels = _pSelSetHighlightor_Faces->getSelectionSet();
}

void OffsetSheetGuiCmd::clearHostSelection()
{
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
}

bool OffsetSheetGuiCmd::createCmdPanel()
{
    QOpenGLWidget* pParentWidget = nullptr;
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (pMainWindow)
        pParentWidget = pMainWindow->findChild<QOpenGLWidget*>();
    if (!pParentWidget) return false;

    _pCmdPanel = new OffsetSheetCmdPanel(pParentWidget);
    _pCmdPanel->setOffsetValue(_offset);
    _pCmdPanel->setSmallValueAllowed(HostKind::Solid == _hostKind);

    QObject::connect(_pCmdPanel, &OffsetSheetCmdPanel::offsetChanged,
        _pCmdPanel, [this](double v) { this->onDialogOffsetChanged(v); });
    QObject::connect(_pCmdPanel, &OffsetSheetCmdPanel::accepted,
        _pCmdPanel, [this]() { this->onDialogAccepted(); });
    QObject::connect(_pCmdPanel, &OffsetSheetCmdPanel::canceled,
        _pCmdPanel, [this]() { this->onDialogCanceled(); });
    _pCmdPanel->show();
    return true;
}

void OffsetSheetGuiCmd::destroyCmdPanel()
{
    if (_pCmdPanel)
    {
        _pCmdPanel->hide();
        delete _pCmdPanel;
        _pCmdPanel = nullptr;
    }
}

void OffsetSheetGuiCmd::onDialogOffsetChanged(double value)
{
    _offset = value;
    if (_pMakeOffsetSheet)
    {
        _pMakeOffsetSheet->updateOffset(_offset);
        this->refreshFaceColors();
    }
    else if (_pMakeSolidSheet)
        this->applyTarget(); // 距离不是无参曲面上的字段, 只能按新距离重算
}

void OffsetSheetGuiCmd::onDialogAccepted()
{
    if (Step::SetParameters != _step) return;

    if (static_cast<int>(wy3d::OffsetSheet::Target::SelectedFaces) == _targetMode && _faceSels.isEmpty())
    {
        MessageBoxUtil::showError(
            static_cast<unsigned int>(wy3d::ErrorCode::OFFSETSHEET_NoFaceSelected));
        return;
    }

    if (_pMakeOffsetSheet)
    {
        // 预览可能因为几何原因失败(偏置距离过大等), 确认前先看链上的错误码
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        wy3d::OffsetSheet* pOffsetSheet = _pMakeOffsetSheet->getOffsetSheet();
        if (pDb && pOffsetSheet)
        {
            const unsigned int errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
                pDb->getTransactionManager()->getChainUpdateFeedback(
                    pOffsetSheet->getId()).get());
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
                return;
            }
        }
        _pMakeOffsetSheet->commit();
        _pMakeOffsetSheet = nullptr;
    }
    else if (_pMakeSolidSheet)
    {
        if (0 != _previewErrorCode)
        {
            MessageBoxUtil::showError(_previewErrorCode);
            return;
        }
        _pMakeSolidSheet->commit();
        _pMakeSolidSheet = nullptr;
    }
    this->requestEnd();
}

void OffsetSheetGuiCmd::onDialogCanceled()
{
    this->clearFaceSelections();
    this->clearHostSelection();
    _pMakeOffsetSheet = nullptr;
    _pMakeSolidSheet = nullptr;
    this->requestAbort(AbortCause::UserCancel);
}
