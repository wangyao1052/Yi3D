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
#include <TopExp.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSolid.h>
#include <wy3dSolid.h>
#include <wy3dSketch.h>
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
#include "commands/dialogs/ChamferDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"


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
    _options.pointSelect = false;
    _options.boxSelect = false;
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
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
    _pointPickOption.selType = wy3d::SelectionType::SolidEdge | wy3d::SelectionType::SolidFace;
    _pointPickOption.acceptElement = false;

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 禁用输入
    // 提示信息
    Application::instance().getStatusBar()->setTips("");

    // 鼠标样式
    Application::instance().setCursor(CursorType::SelectElements);

    // 预览
    _pPreview = nullptr;
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
    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;
}

void TopoNameGuiCmd::showTopoName(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
    if (!pSolid) return;
    const wy3d::TopoNaming* pTopoNaming = pSolid->getTopoNaming();
    if (!pTopoNaming) return;
    TopoDS_Shape shape = pSolid->getShape();

    switch (static_cast<wy3d::SelectionType>(sel.getSelectionType()))
    {
    case wy3d::SelectionType::SolidEdge:
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
        Application::instance().getStatusBar()->setTips(strTopoName.c_str());
        QToolTip::showText(QCursor::pos(), strTopoName.c_str(), nullptr, QRect(), 30000); // 30s
    }
    break;

    case wy3d::SelectionType::SolidFace:
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
        Application::instance().getStatusBar()->setTips(strTopoName.c_str());
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
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
    Application::instance().getStatusBar()->setTips("");
    if (_pPreview)
    {
        wyap::Selection sel = _pPreview->getSelection();
        this->showTopoName(sel);
    }

    return;
}

GuiCmdMenu* TopoNameGuiCmd::initContextMenu()
{
    return new TopoNameGuiCmdMenu(this);
}

bool TopoNameGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return false;
}

void TopoNameGuiCmd::onContextMenuAction_CompleteSelection()
{
    return;
}

bool TopoNameGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return true;
}

void TopoNameGuiCmd::onContextMenuAction_ClearSelection()
{
    // 选择边
    if (_pSelSetHighlightor)
    {
        _pSelSetHighlightor->clearSelections();
    }
}

bool TopoNameGuiCmdMenu::initCustomHeaderActions(QMenu* menu)
{
    // 查找边
    QAction* pActionFindEdgeByTopoName = new QAction("Find Edges", menu);
    menu->addAction(pActionFindEdgeByTopoName);
    this->connect(pActionFindEdgeByTopoName, &QAction::triggered, this, &TopoNameGuiCmdMenu::onFindEdgesByTopoName);

    // 查找面
    QAction* pActionFindFacesByTopoName = new QAction("Find Faces", menu);
    menu->addAction(pActionFindFacesByTopoName);
    this->connect(pActionFindFacesByTopoName, &QAction::triggered, this, &TopoNameGuiCmdMenu::onFindFacesByTopoName);

    return true;
}

void TopoNameGuiCmdMenu::addSelection(TopAbs_ShapeEnum shapeType, const std::string& topoName)
{
    assert(_pCmd);
    TopoNameGuiCmd* pCmd = dynamic_cast<TopoNameGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return;
    }

    wy3d::SelectionType selectionType(wy3d::SelectionType::Element);
    if (TopAbs_ShapeEnum::TopAbs_EDGE == shapeType)
    {
        selectionType = wy3d::SelectionType::SolidEdge;
    }
    else if (TopAbs_ShapeEnum::TopAbs_FACE == shapeType)
    {
        selectionType = wy3d::SelectionType::SolidFace;
    }
    else
    {
        assert(false);
        return;
    }
    unsigned int selType = static_cast<unsigned int>(selectionType);

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    std::vector<const wy3d::Solid*> solids;
    solids.reserve(10);
    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem) continue;
        if (!pElem->getParent().isNull()) continue;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        if (!pSolid) continue;
        
        solids.emplace_back(pSolid);
    }

    for (const wy3d::Solid* pSolid : solids)
    {
        TopoDS_Shape shape = pSolid->getShape();
        const wy3d::TopoNaming* pTopoNaming = pSolid->getTopoNaming();
        assert(pTopoNaming);

        TopTools_IndexedMapOfShape subShapes;
        TopExp::MapShapes(shape, shapeType, subShapes);
        for (int i = 1; i <= subShapes.Extent(); ++i)
        {
            TopoDS_Shape subShape = subShapes(i);
            if (topoName == pTopoNaming->getTopoName(subShape))
            {
                wyap::Selection sel(selType, pSolid->getId(), std::to_string(i - 1));
                pCmd->_pSelSetHighlightor->addSelection(sel);
            }
        }
    }
}

void TopoNameGuiCmdMenu::onFindEdgesByTopoName()
{
    assert(_pCmd);
    TopoNameGuiCmd* pCmd = dynamic_cast<TopoNameGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return;
    }
    if (pCmd->_pSelSetHighlightor)
    {
        pCmd->_pSelSetHighlightor->clearSelections();
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

void TopoNameGuiCmdMenu::onFindFacesByTopoName()
{
    assert(_pCmd);
    TopoNameGuiCmd* pCmd = dynamic_cast<TopoNameGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return;
    }
    if (pCmd->_pSelSetHighlightor)
    {
        pCmd->_pSelSetHighlightor->clearSelections();
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
