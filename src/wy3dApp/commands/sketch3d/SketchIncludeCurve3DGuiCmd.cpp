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

#include "SketchIncludeCurve3DGuiCmd.h"

#include <cassert>

#include <QCoreApplication>
#include <QMessageBox>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSolid.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/TopoShapeUtil.h"

// std::stoul throws on a malformed sub path, a bad value should only make the pick fail
static bool parseSubPathIndex(const std::string& subPath, unsigned int& index)
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

SketchIncludeCurve3DGuiCmd::SketchIncludeCurve3DGuiCmd()
    : OsgGuiCommand()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchIncludeCurve3DGuiCmd::~SketchIncludeCurve3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchIncludeCurve3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    if (!GuiCommandUtil::initSketch3DInfo(_sketch3DInfo))
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    if (_sketch3DInfo.sketch3dId.isNull())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    GuiCommandUtil::clearSelections();

    // Model edges and 2D/3D sketch curves share the sub path form: the element id is the
    // solid, sheet or sketch, and the sub path is the curve inside it. The entities of the
    // sketch being edited are Sketch3DEntity nodes, which only answer to acceptElement and
    // are therefore left out: including a curve into its own sketch would only stack a
    // coincident copy.
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) |
        static_cast<unsigned int>(ElementNodeType::Sheet) |
        static_cast<unsigned int>(ElementNodeType::Sketch) |
        static_cast<unsigned int>(ElementNodeType::Sketch3D);
    _pointPickOption.selType = wy3d::SelectionType::SolidEdge |
        wy3d::SelectionType::SketchCurve |
        wy3d::SelectionType::SketchCurve3D;
    _pointPickOption.acceptElement = false;

    _includedCurves.clear();

    Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
        "Click model edges or sketch curves to include them into the 3D sketch. Press Esc to exit."));

    Application::instance().setCursor(CursorType::SelectElements);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchIncludeCurve3DGuiCmd::onEnd()
{
    _pPreview = nullptr;
    GuiCommand::onEnd();
}

void SketchIncludeCurve3DGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    _pPreview = nullptr;
    GuiCommand::onAbort(cause);
}

void SketchIncludeCurve3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
}

void SketchIncludeCurve3DGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    if (sel.getElementId().isNull())
        return;

    switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
    {
    case wy3d::SelectionType::SolidEdge:
        this->includeModelEdge(sel);
        break;

    case wy3d::SelectionType::SketchCurve:
        this->includeSketchCurve(sel);
        break;

    case wy3d::SelectionType::SketchCurve3D:
        this->includeSketchEntity3D(sel);
        break;

    default:
        break;
    }
}

wydb::Transaction* SketchIncludeCurve3DGuiCmd::beginInclude(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return nullptr;
    }

    // The same curve is included only once; after an undo the entity is gone and the
    // curve may be included again.
    const std::pair<wydb::ElementId, std::string> key(sel.getElementId(), sel.getSubPath());
    const auto iter = _includedCurves.find(key);
    if (_includedCurves.end() != iter && pDb->getElement(iter->second))
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "The curve has already been included."));
        return nullptr;
    }
    _includedCurves.erase(key);

    // One transaction per pick: a single undo step removes one included curve
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    return pDb->getTransactionManager()->startTransaction("", option);
}

bool SketchIncludeCurve3DGuiCmd::finishInclude(const wyap::Selection& sel, wydb::Transaction* pTrans,
    wy3d::Sketch3DEdgeUtil::Result result, wy3d::SketchEntity3D* pEntity)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb || !pTrans)
    {
        assert(false);
        return false;
    }

    if (wy3d::Sketch3DEdgeUtil::Result::Ok == result && pEntity)
    {
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(_sketch3DInfo.sketch3dId));
        if (pSketch3D && wy::ErrorStatus::Ok == pSketch3D->addEntity(pEntity) &&
            wy::ErrorStatus::Ok == pDb->getTransactionManager()->endTransaction())
        {
            _includedCurves[std::make_pair(sel.getElementId(), sel.getSubPath())] = pEntity->getId();
            return true;
        }

        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    pDb->getTransactionManager()->abortTransaction();
    this->reportFailure(result);
    return false;
}

bool SketchIncludeCurve3DGuiCmd::includeModelEdge(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    const wydb::Element* pElement = pDb->getElement(sel.getElementId());
    if (!pElement) return false;

    TopoDS_Shape shape;
    if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement))
    {
        shape = pSolid->getShape();
    }
    else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElement))
    {
        shape = pSheet->getShape();
    }
    else
    {
        return false;
    }
    if (shape.IsNull()) return false;

    unsigned int edgeIndex(0);
    if (!parseSubPathIndex(sel.getSubPath(), edgeIndex)) return false;

    const std::pair<bool, TopoDS_Edge> edgeRet = TopoShapeUtil::getEdge(shape, edgeIndex);
    if (!edgeRet.first) return false;

    wydb::Transaction* pTrans = this->beginInclude(sel);
    if (!pTrans) return false;

    wy3d::SketchEntity3D* pEntity = nullptr;
    const wy3d::Sketch3DEdgeUtil::Result result = wy3d::Sketch3DEdgeUtil::convert(pTrans, edgeRet.second, pEntity);
    return this->finishInclude(sel, pTrans, result, pEntity);
}

bool SketchIncludeCurve3DGuiCmd::includeSketchCurve(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sel.getElementId()));
    if (!pSketch) return false;

    unsigned int curveId(0);
    if (!parseSubPathIndex(sel.getSubPath(), curveId)) return false;

    const wy3d::SketchCurve* pCurve =
        wy3d::SketchCurve::cast(pDb->getElement(wydb::ElementId(curveId)));
    if (!pCurve) return false;

    wydb::Transaction* pTrans = this->beginInclude(sel);
    if (!pTrans) return false;

    wy3d::SketchEntity3D* pEntity = nullptr;
    const wy3d::Sketch3DEdgeUtil::Result result =
        wy3d::Sketch3DEdgeUtil::convert(pTrans, pSketch, pCurve, pEntity);
    return this->finishInclude(sel, pTrans, result, pEntity);
}

bool SketchIncludeCurve3DGuiCmd::includeSketchEntity3D(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    // Another 3D sketch is picked through its element and a sub path, but accept both forms
    const wydb::Element* pElement = pDb->getElement(sel.getElementId());
    const wy3d::SketchEntity3D* pSourceEntity = wy3d::SketchEntity3D::cast(pElement);
    if (!pSourceEntity)
    {
        unsigned int entityId(0);
        if (!parseSubPathIndex(sel.getSubPath(), entityId)) return false;
        pSourceEntity = wy3d::SketchEntity3D::cast(pDb->getElement(wydb::ElementId(entityId)));
    }
    if (!pSourceEntity) return false;

    wydb::Transaction* pTrans = this->beginInclude(sel);
    if (!pTrans) return false;

    wy3d::SketchEntity3D* pEntity = nullptr;
    const wy3d::Sketch3DEdgeUtil::Result result =
        wy3d::Sketch3DEdgeUtil::convert(pTrans, pSourceEntity, pEntity);
    return this->finishInclude(sel, pTrans, result, pEntity);
}

void SketchIncludeCurve3DGuiCmd::reportFailure(wy3d::Sketch3DEdgeUtil::Result result) const
{
    QString message;
    switch (result)
    {
    case wy3d::Sketch3DEdgeUtil::Result::NullCurve:
        message = QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "Include failed: unable to retrieve curve geometry.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::InfiniteCurve:
        message = QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "Include failed: the curve has an infinite parameter range.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::Degenerate:
        message = QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "Include failed: the curve is degenerate.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::InvalidGeometry:
        message = QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "Include failed: the curve geometry is invalid.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::UnsupportedType:
        message = QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "Include failed: unsupported curve type.");
        break;
    default:
        message = QCoreApplication::translate("SketchIncludeCurve3DGuiCmd",
            "Include failed.");
        break;
    }

    QMessageBox::warning(nullptr,
        QCoreApplication::translate("SketchIncludeCurve3DGuiCmd", "Include Curve"),
        message);
}
