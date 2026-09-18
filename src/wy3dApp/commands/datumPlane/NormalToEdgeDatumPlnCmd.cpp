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

#include "commands/datumPlane/NormalToEdgeDatumPlnCmd.h"

#include <gp_Pln.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomLib_Tool.hxx>
#include <BRepLib.hxx>

#include <TopoDS_Edge.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wy3dSolid.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine.h>
#include <wy3dCurve.h>
#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/Colors.h"
#include "snap/SnapConsts.h"
#include "utils/MathUtils.h"
#include "widgets/frame/MainWindow.h"
#include <QCoreApplication>
#include <QCursor>
#include <cmath>

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


class NormalToEdgeDatumPlnCmdSelFilter : public SelectFilterFunctor
{
public:
    inline virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        assert(pDb);
        wydb::ElementId id = sel.getElementId();
        if (id.isNull())
        {
            assert(false);
            return SelectFilterStatus::Continue;
        }

        bool isValid(false);
        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::SolidEdge:
        {
            isValid = this->isValid_SolidEdge(pDb, sel);
        }
        break;

        case wy3d::SelectionType::SketchCurve:
        {
            isValid = this->isValid_SketchCurve(pDb, sel);
        }
        break;

        case wy3d::SelectionType::SketchCurve3D:
        {
            isValid = this->isValid_SketchCurve3D(pDb, sel);
        }
        break;

        case wy3d::SelectionType::Element:
        {
            isValid = this->isValid_Curve(pDb, sel);
        }
        break;
        }

        return isValid ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;
    }

private:
    bool isValid_SketchCurve(const wydb::Database* pDb, const wyap::Selection& sel) const
    {
        assert(pDb);
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::SketchCurve)
        {
            assert(false);
            return false;
        }

        // 草图曲线ID
        if (sel.getSubPath().empty()) return false;
        unsigned int curveId = std::stoul(sel.getSubPath());
        if (0 == curveId) return false;

        // 草图曲线
        const wydb::Element* pElem = pDb->getElement(wydb::ElementId(curveId));
        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pElem);
        if (!pSketchCurve)
        {
            assert(false);
            return false;
        }

        return true;
    }

    bool isValid_SketchCurve3D(const wydb::Database* pDb, const wyap::Selection& sel) const
    {
        assert(pDb);
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::SketchCurve3D)
        {
            assert(false);
            return false;
        }

        if (sel.getSubPath().empty()) return false;
        unsigned int curveId = std::stoul(sel.getSubPath());
        if (0 == curveId) return false;

        const wydb::Element* pElem = pDb->getElement(wydb::ElementId(curveId));
        const wy3d::SketchCurve3D* pSketchCurve3D = wy3d::SketchCurve3D::cast(pElem);
        return nullptr != pSketchCurve3D;
    }

    bool isValid_SolidEdge(const wydb::Database* pDb, const wyap::Selection& sel) const
    {
        assert(pDb);
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::SolidEdge)
        {
            assert(false);
            return false;
        }

        // 边索引
        if (sel.getSubPath().empty()) return false;
        unsigned int edgeIndex = std::stoul(sel.getSubPath());
        if (edgeIndex == -1) return false;

        // 实体
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
        if (!pSolid) return false;
        if (!pSolid->getParent().isNull()) return false;
        TopoDS_Shape shape = pSolid->getShape();
        if (shape.IsNull()) return false;

        // 获取拓扑边
        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
        edgeIndex += 1; // OCC中以1为起始序号
        if (edgeMap.Size() < edgeIndex) return false;
        const TopoDS_Shape& edgeShape = edgeMap.FindKey(edgeIndex);

        // 曲线
        TopoDS_Edge edge = TopoDS::Edge(edgeShape);
        if (edge.IsNull()) return false;
        BRepLib::BuildCurve3d(edge, wy3d::TOL);
        double first(0.0), last(0.0);
        Handle(Geom_Curve) geomCurve = BRep_Tool::Curve(edge, first, last);
        if (geomCurve.IsNull()) return false;

        /*
        // 如果不是直线则直接过滤掉
        if (geomCurve->IsKind(STANDARD_TYPE(Geom_TrimmedCurve)))
        {
            Handle(Geom_TrimmedCurve) trimmedCurve = Handle(Geom_TrimmedCurve)::DownCast(geomCurve);
            Handle(Geom_Curve) basisCurve = trimmedCurve->BasisCurve();
            if (!basisCurve->IsKind(STANDARD_TYPE(Geom_Line)))
            {
                return false;
            }
        }
        else if (!geomCurve->IsKind(STANDARD_TYPE(Geom_Line)))
        {
            return false;
        }
        */

        return true;
    }

    bool isValid_Curve(const wydb::Database* pDb, const wyap::Selection& sel) const
    {
        assert(pDb);
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element)
        {
            assert(false);
            return false;
        }

        const wy3d::Curve* pCurve = wy3d::Curve::cast(pDb->getElement(sel.getElementId()));
        if (!pCurve) return false;        if (!pCurve)
        {
            assert(false);
            return false;
        }
        TopoDS_Edge edge = pCurve->getEdge();
        if (edge.IsNull()) return false;

        return true;
    }
};

NormalToEdgeDatumPlnCmd::NormalToEdgeDatumPlnCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _curveInfo(), _plane()
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

NormalToEdgeDatumPlnCmd::~NormalToEdgeDatumPlnCmd()
{
}

wyap::CmdExecution::StartResult NormalToEdgeDatumPlnCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectEdge);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void NormalToEdgeDatumPlnCmd::cleanup()
{
    this->hidePopup();
    _step = Step::Undefined;
    _curveInfo = CurveInfo();
    _plane = wy3d::SketchPlane();
    _pPreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _pEdgeStartPntTransient = nullptr;
    _pMakeDatumPlane = nullptr;
    _hoverPopupState.resetValue();
}

void NormalToEdgeDatumPlnCmd::reset()
{
    this->cleanup();
}
void NormalToEdgeDatumPlnCmd::onEnd()
{
    GuiCommand::onEnd();

}
void NormalToEdgeDatumPlnCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

}

bool NormalToEdgeDatumPlnCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectEdge:
    {
        if (!_plane.isValid())
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 创建基准面
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(_plane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        assert(_snapExcludeIds.empty());
        _snapExcludeIds.insert(_pMakeDatumPlane->getId());

        // 参照面无效说明不需要第二步
        if (!_curveInfo.refPlane.isValid())
        {
            _pMakeDatumPlane->commit();
            _pMakeDatumPlane = nullptr;

            // 结束
            this->requestEnd();
            return true;
        }
        else
        {
            // 下一步
            this->gotoStep(Step::SpecifyDistance);
            return true;
        }
    }
    break;

    case Step::SpecifyDistance:
    {
        if (!_pMakeDatumPlane || !_curveInfo.geomCurve)
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 基准面
        wy3d::SketchPlane plane;
        if (!this->computePlane(_curveInfo.geomCurve, _curveInfo.param, plane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        if (!_pMakeDatumPlane->update(plane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeDatumPlane->commit();
        _pMakeDatumPlane = nullptr;

        // 结束
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

void NormalToEdgeDatumPlnCmd::gotoStep(Step step)
{
    this->hidePopup();
    _hoverPopupState.resetValue();
    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectEdge:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Select solid edge, sketch curve or 3D sketch curve."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(
            ElementNodeType::Solid | ElementNodeType::Sketch | ElementNodeType::Sketch3D |
            ElementNodeType::Curve);
        _pointPickOption.selType = wy3d::SelectionType::SolidEdge | wy3d::SelectionType::SketchCurve |
            wy3d::SelectionType::SketchCurve3D;
        _pointPickOption.pSelFilter = std::make_shared<NormalToEdgeDatumPlnCmdSelFilter>();
    }
    break;

    case Step::SpecifyDistance:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify through point or directly input the distance value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor)
        {
            assert(_pSelSetHighlightor->getSelectionSet().getCount() == 1);
        }

        // 进入第二步后先按当前鼠标位置计算一次，避免浮窗第一次弹出时还是初始值 0
        this->simulateMouseMoveFromPopup();
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void NormalToEdgeDatumPlnCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void NormalToEdgeDatumPlnCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::SelectEdge:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SpecifyDistance:
    {
        if (!_curveInfo.refPlane.isValid())
        {
            assert(false);
            return;
        }

        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _curveInfo.refPlane, _snapExcludeIds).first;
        GeomAPI_ProjectPointOnCurve projector(MathUtils::toPnt(pnt), _curveInfo.geomCurve);
        if (projector.NbPoints() > 0)
        {
            double param = projector.LowerDistanceParameter();
            double showParam = this->convertParamValueToGui(param, _curveInfo.paramType,
                _curveInfo.extraA, _curveInfo.extraB);
            _hoverPopupState.sign = param < 0.0 ? -1 : 1;
            _hoverPopupState.value = std::fabs(showParam);
            {
                _curveInfo.param = _hoverPopupState.sign < 0 ? -std::fabs(param) : std::fabs(param);
                wy3d::SketchPlane plane;
                if (this->computePlane(_curveInfo.geomCurve, _curveInfo.param, plane))
                {
                    if (_pMakeDatumPlane) _pMakeDatumPlane->update(plane);
                }
            }
            return;
        }
        else
        {
            return;
        }
    }
    break;
    }

    return;
}

void NormalToEdgeDatumPlnCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (Step::SpecifyDistance == _step)
    {
        if (!_curveInfo.refPlane.isValid())
        {
            assert(false);
            return;
        }

        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _curveInfo.refPlane, _snapExcludeIds).first;
        GeomAPI_ProjectPointOnCurve projector(MathUtils::toPnt(pnt), _curveInfo.geomCurve);
        if (projector.NbPoints() > 0)
        {
            double param = projector.LowerDistanceParameter();
            _curveInfo.param = _hoverPopupState.sign < 0 ? -std::fabs(param) : std::fabs(param);
            this->finishStep(_step);
            return;
        }
        else
        {
            this->finishStep(_step);
            return;
        }
    }

    return;
}

void NormalToEdgeDatumPlnCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::SelectEdge == _step)
    {
        if (!_pPreview || _pPreview->getSelection().getElementId().isNull())
        {
            return;
        }
        wyap::Selection sel = _pPreview->getSelection();
        _pPreview = nullptr;

        // 高亮选中
        _pSelSetHighlightor->clearSelections();
        _pSelSetHighlightor->addSelection(sel);

        if (!this->extractCurveInfo(sel, _curveInfo))
        {
            assert(false);
            _pSelSetHighlightor->clearSelections();
            return;
        }

        if (!this->computePlane(_curveInfo.geomCurve, _curveInfo.param, _plane))
        {
            assert(false);
            _pSelSetHighlightor->clearSelections();
            return;
        }

        // 显示起点
        _pEdgeStartPntTransient = std::make_shared<PointTransient>(_curveInfo.initPoint,
            Colors::kEdge_Preview, SnapConsts::PickSize);

        // 完成步骤
        this->finishStep(_step);
        return;
    }

    return;
}

void NormalToEdgeDatumPlnCmd::initializePopups()
{
    if (_pDistancePopup)
    {
        return;
    }

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    _pDistancePopup = std::make_unique<GuiCmdHoverInputPopup1>(
        QCoreApplication::translate("DatumPlnCmd", "Distance"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pDistancePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pDistancePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pDistancePopup->hide();
}

void NormalToEdgeDatumPlnCmd::showPopup()
{
    if (_step != Step::SpecifyDistance)
    {
        return;
    }
    if (!_pDistancePopup)
    {
        this->initializePopups();
    }
    if (!_pDistancePopup)
    {
        return;
    }

    _pDistancePopup->setValue(_hoverPopupState.value);
    _pDistancePopup->showAtGlobal(QCursor::pos());
}

void NormalToEdgeDatumPlnCmd::hidePopup()
{
    if (_pDistancePopup && _pDistancePopup->isVisible())
    {
        _pDistancePopup->hide();
    }
}

void NormalToEdgeDatumPlnCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyDistance)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pDistancePopup && _pDistancePopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void NormalToEdgeDatumPlnCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyDistance || !_pDistancePopup)
    {
        return;
    }

    double param(0.0);
    if (!parseDoubleText(_pDistancePopup->getRowText(), param))
    {
        return;
    }
    param = this->convertParamValueToKernel(param, _curveInfo.paramType,
        _curveInfo.extraA, _curveInfo.extraB);
    _curveInfo.param = _hoverPopupState.sign < 0 ? -std::fabs(param) : std::fabs(param);

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void NormalToEdgeDatumPlnCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void NormalToEdgeDatumPlnCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

double NormalToEdgeDatumPlnCmd::convertParamValueToKernel(
    double param, ParamType paramType,
    double extraA, double extraB) const
{
    switch (paramType)
    {
    case ParamType::Length:
        return param;
    
    case ParamType::Radian:
        return wy3d::degreesToRadians(param);
    
    case ParamType::ParametricRadian:
    {
        param = wy3d::degreesToRadians(param);
        return MathUtils::ellipseGeometricToParametricAngle(param, extraA, extraB);
    }
    
    default:
        return param;
    }
}

double NormalToEdgeDatumPlnCmd::convertParamValueToGui(
    double param, ParamType paramType,
    double extraA, double extraB) const
{
    switch (paramType)
    {
    case ParamType::Length:
        return param;
    
    case ParamType::Radian:
        return wy3d::radiansToDegrees(param);
    
    case ParamType::ParametricRadian:
    {
        param = MathUtils::ellipseParametricToGeometricAngle(param, extraA, extraB);
        return wy3d::radiansToDegrees(param);
    }
    
    default:
        return param;
    }
}

QString NormalToEdgeDatumPlnCmd::getParamTypeLabelStr(ParamType paramType) const
{
    switch (paramType)
    {
    case ParamType::Length:
        return QCoreApplication::translate("DatumPlnCmd", "Distance");

    case ParamType::Radian:
    case ParamType::ParametricRadian:
        return QCoreApplication::translate("DatumPlnCmd", "Angle");

    default:
        return QCoreApplication::translate("DatumPlnCmd", "Distance");
    }
}

bool NormalToEdgeDatumPlnCmd::extractCurveInfo(const wyap::Selection& sel, CurveInfo& curveInfo)
{
    try
    {
        return this->extractCurveInfoImpl(sel, curveInfo);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

bool NormalToEdgeDatumPlnCmd::extractCurveInfoImpl(const wyap::Selection& sel, CurveInfo& curveInfo)
{
    Handle(Geom_Curve) geomCurve(nullptr);
    switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
    {
    case wy3d::SelectionType::SolidEdge:
    {
        geomCurve = MakeDatumPlane::getSolidEdgeGeomCurve(sel);
    }
    break;

    case wy3d::SelectionType::SketchCurve:
    {
        geomCurve = MakeDatumPlane::getSketchCurveGeomCurve(sel);
    }
    break;

    case wy3d::SelectionType::SketchCurve3D:
    {
        geomCurve = MakeDatumPlane::getSketchCurve3DGeomCurve(sel);
    }
    break;

    case wy3d::SelectionType::Element:
    {
        geomCurve = MakeDatumPlane::getCurveGeomCurve(sel);
    }
    break;

    default:
    {
        assert(false);
        return false;
    }
    break;
    }

    if (geomCurve.IsNull())
    {
        assert(false);
        return false;
    }

    Handle(Geom_Curve) basisCurve = geomCurve;
    if (geomCurve->IsKind(STANDARD_TYPE(Geom_TrimmedCurve)))
    {
        Handle(Geom_TrimmedCurve) trimmedCurve = Handle(Geom_TrimmedCurve)::DownCast(geomCurve);
        if (!trimmedCurve.IsNull())
        {
            basisCurve = trimmedCurve->BasisCurve();
        }
    }
    if (basisCurve.IsNull())
    {
        assert(false);
        return false;
    }

    const Handle(Standard_Type)& basisCurveType = basisCurve->DynamicType();
    if (basisCurveType == STANDARD_TYPE(Geom_Line))
    {
        curveInfo.curveType = CurveType::Line;
        curveInfo.paramType = ParamType::Length;

        Handle(Geom_Line) geomLine = Handle(Geom_Line)::DownCast(basisCurve);
        if (geomLine.IsNull())
        {
            assert(false);
            return false;
        }

        const gp_Ax1& ax1 = geomLine->Position();
        wy::Vector3 origin = MathUtils::toVector3(ax1.Location());
        wy::Vector3 xDir = MathUtils::toVector3(ax1.Direction());
        wy::Vector3 yDir = wy::Vector3::kZAxis.cross(xDir);
        if (yDir.length() < 0.5)
        {
            yDir = wy::Vector3::kXAxis.cross(xDir);
        }
        wy::Vector3 zDir = xDir.cross(yDir);
        curveInfo.refPlane = wy3d::SketchPlane(origin, zDir, xDir);
    }
    else if (basisCurveType == STANDARD_TYPE(Geom_Circle))
    {
        curveInfo.curveType = CurveType::Circle;
        curveInfo.paramType = ParamType::Radian;

        Handle(Geom_Circle) geomCircle = Handle(Geom_Circle)::DownCast(basisCurve);
        if (geomCircle.IsNull())
        {
            assert(false);
            return false;
        }

        const gp_Ax2& ax2 = geomCircle->Position();
        curveInfo.refPlane = wy3d::SketchPlane(
            MathUtils::toVector3(ax2.Location()),
            MathUtils::toVector3(ax2.Direction()),
            MathUtils::toVector3(ax2.XDirection()));
    }
    else if (basisCurveType == STANDARD_TYPE(Geom_Ellipse))
    {
        curveInfo.curveType = CurveType::Ellipse;
        curveInfo.paramType = ParamType::ParametricRadian;

        Handle(Geom_Ellipse) geomEllipse = Handle(Geom_Ellipse)::DownCast(basisCurve);
        if (geomEllipse.IsNull())
        {
            assert(false);
            return false;
        }

        const gp_Ax2& ax2 = geomEllipse->Position();
        curveInfo.refPlane = wy3d::SketchPlane(
            MathUtils::toVector3(ax2.Location()),
            MathUtils::toVector3(ax2.Direction()),
            MathUtils::toVector3(ax2.XDirection()));

        curveInfo.extraA = geomEllipse->MajorRadius();
        curveInfo.extraB = geomEllipse->MinorRadius();
    }
    else if (basisCurveType == STANDARD_TYPE(Geom_BSplineCurve))
    {
        curveInfo.curveType = CurveType::Spline;
        curveInfo.paramType = ParamType::Undefined;
        curveInfo.refPlane = wy3d::SketchPlane::kInvalid;
    }
    else
    {
        assert(false);
        curveInfo.curveType = CurveType::Undefined;
        curveInfo.paramType = ParamType::Undefined;
        curveInfo.refPlane = wy3d::SketchPlane::kInvalid;
    }

    // 曲线
    assert(geomCurve);
    curveInfo.geomCurve = geomCurve;

    // 根据拾取点确定是起点还是终点
    gp_Pnt gpStartPnt, gpEndPnt;
    curveInfo.geomCurve->D0(geomCurve->FirstParameter(), gpStartPnt);
    curveInfo.geomCurve->D0(geomCurve->LastParameter(), gpEndPnt);
    wy::Vector3 startPnt = MathUtils::toVector3(gpStartPnt);
    wy::Vector3 endPnt = MathUtils::toVector3(gpEndPnt);
    wy::Vector3 pickPos = sel.getPickPosition();
    if ((pickPos - startPnt).length() <= (pickPos - endPnt).length())
    {
        curveInfo.param = geomCurve->FirstParameter();
        curveInfo.initPoint = startPnt;
    }
    else
    {
        curveInfo.param = geomCurve->LastParameter();
        curveInfo.initPoint = endPnt;
    }

    return true;
}

bool NormalToEdgeDatumPlnCmd::computePlane(Handle(Geom_Curve) geomCurve, double param, wy3d::SketchPlane& plane)
{
    if (geomCurve.IsNull())
    {
        return false;
    }

    try
    {
        double firstParam = geomCurve->FirstParameter();
        double lastParam = geomCurve->LastParameter();
        if (param < firstParam) param = firstParam;
        else if (param > lastParam) param = lastParam;

        gp_Pnt point;
        gp_Vec dir;
        geomCurve->D1(param, point, dir);
        if (!geomCurve->IsClosed())
        {
            if (std::fabs(param - firstParam) <= std::fabs(param - lastParam))
            {
                dir = -dir;
            }
        }

        wy::Vector3 plnNormal(dir.X(), dir.Y(), dir.Z());
        plnNormal.normalize();
        if (plnNormal.length() < 0.5)
        {
            assert(false);
            return false;
        }

        wy::Vector3 yDir = plnNormal.cross(wy::Vector3::kXAxis);
        if (yDir.length() < 0.5)
        {
            yDir = plnNormal.cross(wy::Vector3::kYAxis);
        }
        yDir.normalize();
        if (yDir.length() < 0.5)
        {
            assert(false);
            return false;
        }
        wy::Vector3 xDir = yDir.cross(plnNormal);
        assert(xDir.length() > 0.5);

        plane = wy3d::SketchPlane(MathUtils::toVector3(point), plnNormal, xDir);
        if (!plane.isValid())
        {
            assert(false);
            return false;
        }

        return true;
    }
    catch (const Standard_Failure& e)
    {
        // 处理异常(如参数无效、曲线不可导等)
        assert(false);
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

