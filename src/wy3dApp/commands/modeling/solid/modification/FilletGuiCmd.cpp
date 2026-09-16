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

#include "commands/modeling/solid/modification/FilletGuiCmd.h"
#include <QCoreApplication>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dSelectionType.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/FilletDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"


// 前置过滤器: 确保只能选择单一主体的面或边
class FilletGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    FilletGuiCmdPreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
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

FilletGuiCmd::FilletGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _radius(5.0),
    _hostId(wydb::ElementId::kNull), _hostIsSheet(false)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

FilletGuiCmd::~FilletGuiCmd()
{
}

wyap::CmdExecution::StartResult FilletGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) | static_cast<unsigned int>(ElementNodeType::Sheet);
    _pointPickOption.selType = wy3d::SelectionType::SolidEdge | wy3d::SelectionType::SolidFace;
    _pointPickOption.acceptElement = false;
    this->gotoStep(Step::SelectEdges);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void FilletGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

}
void FilletGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);

}

void FilletGuiCmd::reset()
{
    _step = Step::Undefined;
    _sels.clear();
    _radius = 0.0;

    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;

    // 宿主拓扑缓存
    _hostId = wydb::ElementId::kNull;
    _hostIsSheet = false;
    _hostFaces.Clear();
    _hostEdges.Clear();
    _hostEdgeFaces.Clear();
    Application::instance().setCursor(CursorType::Select);
}

bool FilletGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectEdges:
    {
        if (_sels.isEmpty())
        {
            assert(false);
            return false;
        }

        // next step
        this->gotoStep(Step::InputFilletRadius);
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

void FilletGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectEdges:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("FilletGuiCmd", "Select edges or faces; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        // 高亮
        _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    }
    break;

    case Step::InputFilletRadius:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("FilletGuiCmd", "Input fillet radius."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 倒角对话框
        FilletDialog dialog(5.0);
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset(); // 重置数据
            this->requestAbort(AbortCause::UserCancel);  // 退出
            return;
        }
        _radius = dialog.getRadius(); // 对话框逻辑中已经添加了校验数据的合理性

        // 执行圆角
        unsigned int errorCode(0);
        if (!this->createFillet(errorCode)) // 无论执行成功与否,后续逻辑都会退出命令
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
        }

        // 重置数据
        this->reset();

        // exit
        this->requestEnd();
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

void FilletGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectEdges:
    {
        // 点选预览: 不合格的边或面不预览
        this->updateHoverPreview(event.x, event.y);
    }
    break;
    }

    return;
}

void FilletGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::SelectEdges == _step)
    {
        if (_pPreview)
        {
            // 先拷贝: 下面会置空 _pPreview
            const wyap::Selection sel = _pPreview->getSelection();

            std::vector<wyap::Selection> sels;
            if (this->resolveFilletPick(sel, sels)) // 不可圆角的边或面: 不加入选择集
            {
                this->toggleSelections(sels);

                // 过滤器
                _pointPickOption.pSelPreFilter = std::make_shared<FilletGuiCmdPreSelFilter>(
                    _pSelSetHighlightor->getSelectionSet());
            }

            _pPreview = nullptr;
        }
    }

    return;
}

// 建立宿主拓扑缓存: 只在宿主变化时重建
bool FilletGuiCmd::ensureHostTopo(const wydb::ElementId& hostId)
{
    if (hostId == _hostId) return true;

    _hostId = hostId;
    _hostIsSheet = false;
    _hostFaces.Clear();
    _hostEdges.Clear();
    _hostEdgeFaces.Clear();

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    const wydb::Element* pElement = pDb->getElement(hostId);
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement);
    const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElement);
    if (!pSolid && !pSheet) return false;

    const TopoDS_Shape& shape = pSolid ? pSolid->getShape() : pSheet->getShape();
    if (shape.IsNull()) return false;

    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, _hostFaces);
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, _hostEdges);
    TopExp::MapShapesAndAncestors(shape, TopAbs_ShapeEnum::TopAbs_EDGE, TopAbs_ShapeEnum::TopAbs_FACE, _hostEdgeFaces);
    _hostIsSheet = (nullptr == pSolid);

    return true;
}

bool FilletGuiCmd::resolveFilletPick(const wyap::Selection& sel,
    std::vector<wyap::Selection>& outSels)
{
    outSels.clear();

    const wydb::ElementId& hostId = sel.getElementId();
    if (hostId.isNull())
    {
        assert(false);
        outSels.emplace_back(sel);
        return true;
    }
    if (!this->ensureHostTopo(hostId)) // 不是实体或片体元素: 原样放行
    {
        outSels.emplace_back(sel);
        return true;
    }
    if (!_hostIsSheet) // 实体宿主: 与之前的逻辑完全一致
    {
        outSels.emplace_back(sel);
        return true;
    }

    switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
    {
    case wy3d::SelectionType::SolidEdge:
    {
        unsigned int edgeIndex(0);
        if (!ChamferFilletCmdCommon::parseSubPathIndex(sel.getSubPath(), edgeIndex))
        {
            assert(false);
            outSels.emplace_back(sel);
            return true;
        }
        if (edgeIndex >= static_cast<unsigned int>(_hostEdges.Extent()))
        {
            assert(false);
            outSels.emplace_back(sel);
            return true;
        }

        // 片体上圆角的边必须恰好两个相邻面
        const TopoDS_Shape& edge = _hostEdges(static_cast<int>(edgeIndex) + 1);
        if (!ChamferFilletCmdCommon::hasTwoAdjacentFaces(_hostEdgeFaces, edge)) return false;

        outSels.emplace_back(sel);
        return true;
    }

    case wy3d::SelectionType::SolidFace:
    {
        unsigned int faceIndex(0);
        if (!ChamferFilletCmdCommon::parseSubPathIndex(sel.getSubPath(), faceIndex))
        {
            assert(false);
            outSels.emplace_back(sel);
            return true;
        }
        if (faceIndex >= static_cast<unsigned int>(_hostFaces.Extent()))
        {
            assert(false);
            outSels.emplace_back(sel);
            return true;
        }

        // 面上没有任何能圆角的边: 这个面不能选
        const TopoDS_Face face = TopoDS::Face(_hostFaces(static_cast<int>(faceIndex) + 1));
        const std::vector<unsigned int> edgeIndices =
            ChamferFilletCmdCommon::collectChamferableEdgeIndices(face, _hostEdges, _hostEdgeFaces);
        if (edgeIndices.empty()) return false;

        // 面转换成该面上能圆角的边
        outSels.reserve(edgeIndices.size());
        for (unsigned int edgeIndex : edgeIndices)
        {
            outSels.emplace_back(wyap::Selection(
                static_cast<unsigned int>(wy3d::SelectionType::SolidEdge), hostId, std::to_string(edgeIndex)));
        }
        return true;
    }

    default:
    {
        assert(false);
        outSels.emplace_back(sel);
        return true;
    }
    }
}

void FilletGuiCmd::toggleSelections(const std::vector<wyap::Selection>& sels)
{
    if (sels.empty()) return;

    bool isAllSelected(true);
    for (const wyap::Selection& sel : sels)
    {
        if (!_pSelSetHighlightor->containsSelection(sel))
        {
            isAllSelected = false;
            break;
        }
    }

    for (const wyap::Selection& sel : sels)
    {
        if (isAllSelected)
        {
            _pSelSetHighlightor->removeSelection(sel);
        }
        else
        {
            _pSelSetHighlightor->addSelection(sel);
        }
    }
}

// 悬停预览: 不合格的边或面不高亮, 只把光标变成禁止 (照拉伸命令)
void FilletGuiCmd::updateHoverPreview(double x, double y)
{
    const wyap::Selection sel = this->pointPick(x, y, _pointPickOption);
    if (sel.getElementId().isNull())
    {
        _pPreview = nullptr;
        Application::instance().setCursor(CursorType::SelectElements);
        return;
    }

    std::vector<wyap::Selection> sels;
    if (!this->resolveFilletPick(sel, sels))
    {
        _pPreview = nullptr;
        Application::instance().setCursor(CursorType::Forbid);
        return;
    }

    // 同一个对象不重建预览
    if (!_pPreview || !_pPreview->isEqual(sel))
    {
        _pPreview = std::make_shared<SelectPreview>(sel);
    }
    Application::instance().setCursor(CursorType::SelectElements);
}

void FilletGuiCmd::onEnterKey()
{
    if (Step::SelectEdges == _step)
    {
        _sels = _pSelSetHighlightor->getSelectionSet();
        if (!_sels.isEmpty())
        {
            this->finishStep(_step);
        }
    }
}

void FilletGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool FilletGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectEdges == _step;
}

void FilletGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool FilletGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectEdges == _step;
}

void FilletGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectEdges == _step)
    {
        if (_pSelSetHighlightor)
        {
            _pSelSetHighlightor->clearSelections();
        }
    }
}

bool FilletGuiCmd::createFillet(unsigned int& errorCode)
{
    return ChamferFilletCmdCommon::createChamferOrFillet<wy3d::Fillet,
        wy3d::ErrorCode::FILLET_CreateFilletError>(_sels, _radius, errorCode);
}
