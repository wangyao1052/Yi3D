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

#include "SketchProjectGuiCmd.h"

#include <cassert>
#include <string>

#include <QCoreApplication>
#include <QMessageBox>

#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>

#include <wy3dSolid.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "snap/SketchSnapSystem.h"
#include "select/SelectPreview.h"
#include "utils/GuiCommandUtil.h"
#include "utils/SketchProjectUtil.h"


SketchProjectGuiCmd::SketchProjectGuiCmd()
    : OsgGuiCommand()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchProjectGuiCmd::~SketchProjectGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchProjectGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys)
        _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 校验
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    if (_sketchInfo.sketchId.isNull())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    // 点选选项：选择实体边
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
    _pointPickOption.selType = wy3d::SelectionType::SolidEdge;
    _pointPickOption.acceptElement = false;

    // 提示信息
    Application::instance().getStatusBar()->setTips(
        QCoreApplication::translate("SketchProject", "Click on solid edges to project them onto the sketch plane. Press Esc to exit."));

    // 鼠标样式
    Application::instance().setCursor(CursorType::SelectElements);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchProjectGuiCmd::onEnd()
{
    _pPreview = nullptr;
    GuiCommand::onEnd();
}

void SketchProjectGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    _pPreview = nullptr;
    GuiCommand::onAbort(cause);
}

void SketchProjectGuiCmd::onMouseMove(const MouseEvent& event)
{
    // 悬停高亮预览
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
}

void SketchProjectGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    // 点选边
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    if (sel.getElementId().isNull())
        return;

    // 必须是SolidEdge类型
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::SolidEdge)
        return;

    // 立即投影
    this->projectEdge(sel);
}

bool SketchProjectGuiCmd::projectEdge(const wyap::Selection& sel)
{
    const std::string& subPath = sel.getSubPath();
    if (subPath.empty())
        return false;

    unsigned int edgeIndex = std::stoul(subPath);

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    // 获取Solid的TopoDS_Shape
    const wy3d::Solid* pConstSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
    if (!pConstSolid) return false;

    const TopoDS_Shape& shape = pConstSolid->getShape();
    if (shape.IsNull()) return false;

    // 按索引提取边
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    unsigned int occIndex = edgeIndex + 1; // OCCT以1为起始
    if (occIndex > edgeMap.Size()) return false;

    const TopoDS_Edge& edge = TopoDS::Edge(edgeMap.FindKey(occIndex));
    if (edge.IsNull()) return false;

    // 开启事务
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;

    // 投影
    wy3d::SketchEntity* pEntity = nullptr;
    SketchProjectUtil::ProjectResult result = SketchProjectUtil::projectEdge(
        pTrans, edge, _sketchInfo.sketchPlane, pEntity);

    if (result == SketchProjectUtil::ProjectResult::Ok && pEntity)
    {
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchInfo.sketchId));
        if (!pSketch)
        {
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        pSketch->addEntity(pEntity);
        pDb->getTransactionManager()->endTransaction();
        return true;
    }

    // 失败处理
    pDb->getTransactionManager()->abortTransaction();

    switch (result)
    {
    case SketchProjectUtil::ProjectResult::Degenerate:
        QMessageBox::warning(nullptr,
            QCoreApplication::translate("SketchProject", "Project"),
            QCoreApplication::translate("SketchProject",
                "The edge is perpendicular to the sketch plane, projection degenerates to a point."));
        break;
    case SketchProjectUtil::ProjectResult::NullCurve:
        QMessageBox::warning(nullptr,
            QCoreApplication::translate("SketchProject", "Project"),
            QCoreApplication::translate("SketchProject",
                "Projection failed: unable to retrieve curve geometry from the edge."));
        break;
    case SketchProjectUtil::ProjectResult::ProjectFailed:
        QMessageBox::warning(nullptr,
            QCoreApplication::translate("SketchProject", "Project"),
            QCoreApplication::translate("SketchProject",
                "Projection failed."));
        break;
    case SketchProjectUtil::ProjectResult::UnsupportedType:
        QMessageBox::warning(nullptr,
            QCoreApplication::translate("SketchProject", "Project"),
            QCoreApplication::translate("SketchProject",
                "Unsupported curve type for projection."));
        break;
    default:
        break;
    }

    return false;
}
