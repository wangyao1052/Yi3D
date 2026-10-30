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

#include "TopoNameGuiCmd.h"

#include <string>
#include <sstream>
#include <QCoreApplication>
#include <QToolTip>
#include <QInputDialog>
#include <QMenu>
#include <TopExp.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSolid.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/ElementNodeType.h>
#include "scene/nodes/SolidElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/CommonSelFilters.h>
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/ChamferDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 前置过滤器: 确保只能选择单一主体的面或边
class TopoNameGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    TopoNameGuiCmdPreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
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

TopoNameGuiCmd::TopoNameGuiCmd() : OsgGuiCommand()
{
}

TopoNameGuiCmd::~TopoNameGuiCmd()
{
}

wyap::CmdExecution::StartResult TopoNameGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.allowedElemTypes = ElementNodeType::Solid | ElementNodeType::Sheet;
    _pointPickOption.selType = wy3d::SelectionType::Edge | wy3d::SelectionType::Face;

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    // 禁用输入
    // 提示信息
    ApplicationImpl::instance().getStatusBar()->setTips("");

    // 鼠标样式
    ApplicationImpl::instance().setCursor(CursorType::SelectElements);

    // 预览
    _preview.clear();
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());

    return wyap::CmdExecution::StartResult::Succeeded;
}
void TopoNameGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

}
void TopoNameGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);

}

void TopoNameGuiCmd::reset()
{
    _preview.clear();
    _pSelSetHighlightor = nullptr;
}

void TopoNameGuiCmd::showTopoName(const wyap::Selection& sel)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElem);
    if (!pSolid && !pSheet) return;
    const wy3d::TopoNaming* pTopoNaming = pSolid
        ? pSolid->getTopoNaming() : pSheet->getTopoNaming();
    if (!pTopoNaming) return;
    TopoDS_Shape shape = pSolid ? pSolid->getShape() : pSheet->getShape();

    switch (static_cast<wy3d::SelectionType>(sel.getSelectionType()))
    {
    case wy3d::SelectionType::Edge:
    {
        const std::string& subPath = sel.getSubPath();
        if (subPath.empty())
        {
            assert(false);
            return;
        }
        unsigned int edgeIndex = std::stoul(subPath);
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        if ((edgeIndex + 1) > edges.Extent())
        {
            assert(false);
            return;
        }
        TopoDS_Shape egde = edges(edgeIndex + 1);
        std::string strTopoName = pTopoNaming->getTopoName(egde);
        ApplicationImpl::instance().getStatusBar()->setTips(strTopoName.c_str());
        QToolTip::showText(QCursor::pos(), strTopoName.c_str(), nullptr, QRect(), 30000); // 30s
    }
    break;

    case wy3d::SelectionType::Face:
    {
        const std::string& subPath = sel.getSubPath();
        if (subPath.empty())
        {
            assert(false);
            return;
        }
        unsigned int faceIndex = std::stoul(subPath);
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        if ((faceIndex + 1) > faces.Extent())
        {
            assert(false);
            return;
        }
        TopoDS_Shape face = faces(faceIndex + 1);
        std::string strTopoName = pTopoNaming->getTopoName(face);
        ApplicationImpl::instance().getStatusBar()->setTips(strTopoName.c_str());
        QToolTip::showText(QCursor::pos(), strTopoName.c_str(), nullptr, QRect(), 5000); // 5s
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }
}

void TopoNameGuiCmd::onMouseMove(const MouseEvent& event)
{
    // 点选预览
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    _preview.update(sel);
    ApplicationImpl::instance().getStatusBar()->setTips("");
    if (!_preview.isNull())
    {
        wyap::Selection sel = _preview.getSelection();
        this->showTopoName(sel);
    }

    return;
}

void TopoNameGuiCmd::configureContextMenu(ContextMenu& menu)
{
    menu.addAction("Find Edges", [this]() { this->onFindEdgesByTopoName(); });
    menu.addAction("Find Faces", [this]() { this->onFindFacesByTopoName(); });
    menu.setClearSelectionVisible(true);
}

void TopoNameGuiCmd::onCompleteSelectionMenuItem()
{
    return;
}

void TopoNameGuiCmd::onClearSelectionMenuItem()
{
    // 选择边
    if (_pSelSetHighlightor)
    {
        _pSelSetHighlightor->clearSelections();
    }
}

void TopoNameGuiCmd::addSelection(TopAbs_ShapeEnum shapeType, const std::string& topoName)
{
    wy3d::SelectionType selectionType(wy3d::SelectionType::Element);
    if (TopAbs_ShapeEnum::TopAbs_EDGE == shapeType)
    {
        selectionType = wy3d::SelectionType::Edge;
    }
    else if (TopAbs_ShapeEnum::TopAbs_FACE == shapeType)
    {
        selectionType = wy3d::SelectionType::Face;
    }
    else
    {
        assert(false);
        return;
    }
    unsigned int selType = static_cast<unsigned int>(selectionType);

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    auto processElements = [&](const wydb::Element* pElem)
    {
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElem);
        if (!pSolid && !pSheet) return;

        TopoDS_Shape shape = pSolid ? pSolid->getShape() : pSheet->getShape();
        const wy3d::TopoNaming* pTopoNaming = pSolid
            ? pSolid->getTopoNaming() : pSheet->getTopoNaming();
        assert(pTopoNaming);

        TopTools_IndexedMapOfShape subShapes;
        TopExp::MapShapes(shape, shapeType, subShapes);
        for (int i = 1; i <= subShapes.Extent(); ++i)
        {
            TopoDS_Shape subShape = subShapes(i);
            if (topoName == pTopoNaming->getTopoName(subShape))
            {
                wyap::Selection sel(selType, pElem->getId(), std::to_string(i - 1));
                _pSelSetHighlightor->addSelection(sel);
            }
        }
    };

    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current());
        if (!pElem) continue;
        if (!pElem->getParent().isNull()) continue;
        processElements(pElem);
    }
}

void TopoNameGuiCmd::onFindEdgesByTopoName()
{
    if (_pSelSetHighlightor)
    {
        _pSelSetHighlightor->clearSelections();
    }

    QInputDialog dialog;
    dialog.setWindowTitle("Find Edges");
    dialog.setLabelText("Edge Topo Name");
    dialog.setTextValue("");
    dialog.setInputMode(QInputDialog::TextInput);
    if (dialog.exec() != QDialog::Accepted) return;
    QString text = dialog.textValue();
    if (text.isEmpty()) return;

    std::string edgeTopoName = text.trimmed().toStdString();
    if (edgeTopoName.empty()) return;
    addSelection(TopAbs_ShapeEnum::TopAbs_EDGE, edgeTopoName);
}

void TopoNameGuiCmd::onFindFacesByTopoName()
{
    if (_pSelSetHighlightor)
    {
        _pSelSetHighlightor->clearSelections();
    }

    QInputDialog dialog;
    dialog.setWindowTitle("Find Faces");
    dialog.setLabelText("Face Topo Name");
    dialog.setTextValue("");
    dialog.setInputMode(QInputDialog::TextInput);
    if (dialog.exec() != QDialog::Accepted) return;
    QString text = dialog.textValue();
    if (text.isEmpty()) return;

    std::string faceTopoName = text.trimmed().toStdString();
    if (faceTopoName.empty()) return;
    addSelection(TopAbs_ShapeEnum::TopAbs_FACE, faceTopoName);
}

} // namespace wy3dApp
