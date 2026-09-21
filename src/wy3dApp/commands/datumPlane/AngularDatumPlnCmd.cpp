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

#include "commands/datumPlane/AngularDatumPlnCmd.h"
#include <Geom_Plane.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dImpl.h>
#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "select/SketchPlaneSelFilter.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/DatumPlaneElementNode.h"
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


class AngularDatumPlnCmdRotationAxisSelFilter : public SelectFilterFunctor
{
public:
    AngularDatumPlnCmdRotationAxisSelFilter(const wy3d::SketchPlane& plane) : _plane(plane) {}


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

        // 实体面
        if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::Face)
        {
            if (sel.getSubPath().empty()) return SelectFilterStatus::Continue;
            unsigned int faceIndex = std::stoul(sel.getSubPath());
            if (faceIndex == -1) return SelectFilterStatus::Continue;

            const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
            if (!pSolid) return SelectFilterStatus::Continue;
            TopoDS_Shape shape = pSolid->getShape();
            if (shape.IsNull()) return SelectFilterStatus::Continue;

            TopTools_IndexedMapOfShape faceMap;
            TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
            faceIndex += 1; // OCC中以1为起始序号
            if (faceMap.Size() < faceIndex) return SelectFilterStatus::Continue;
            const TopoDS_Shape& faceShape = faceMap.FindKey(faceIndex);

            TopoDS_Face face = TopoDS::Face(faceShape);
            if (face.IsNull()) return SelectFilterStatus::Continue;
            Handle(Geom_Plane) geomPlane = Handle(Geom_Plane)::DownCast(BRep_Tool::Surface(face));
            if (geomPlane.IsNull()) return SelectFilterStatus::Continue;

            // 平面平行
            gp_Dir normal = geomPlane->Position().Direction();
            double normalDotProduct = _plane.getNormal().dot(wy::Vector3(normal.X(), normal.Y(), normal.Z()));
            if (std::fabs(std::fabs(normalDotProduct) - 1.0) <= wy3d::EPS) // 法向量点积的绝对值接近于1则表明两面平行
            {
                return SelectFilterStatus::Continue;
            }

            return SelectFilterStatus::Ok;
        }
        // 实体边
        else if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::Edge)
        {
            if (sel.getSubPath().empty()) return SelectFilterStatus::Continue;
            unsigned int edgeIndex = std::stoul(sel.getSubPath());
            if (edgeIndex == -1) return SelectFilterStatus::Continue;

            const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
            if (!pSolid) return SelectFilterStatus::Continue;
            TopoDS_Shape shape = pSolid->getShape();
            if (shape.IsNull()) return SelectFilterStatus::Continue;

            TopTools_IndexedMapOfShape edgeMap;
            TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
            edgeIndex += 1; // OCC中以1为起始序号
            if (edgeMap.Size() < edgeIndex) return SelectFilterStatus::Continue;
            const TopoDS_Shape& edgeShape = edgeMap.FindKey(edgeIndex);

            // 曲线
            TopoDS_Edge edge = TopoDS::Edge(edgeShape);
            if (edge.IsNull()) return SelectFilterStatus::Continue;
            double first(0.0), last(0.0);
            Handle(Geom_Curve) geomCurve = BRep_Tool::Curve(edge, first, last);
            if (geomCurve.IsNull()) return SelectFilterStatus::Continue;

            // 如果不是直线则直接过滤掉
            if (geomCurve->IsKind(STANDARD_TYPE(Geom_TrimmedCurve)))
            {
                Handle(Geom_TrimmedCurve) trimmedCurve = Handle(Geom_TrimmedCurve)::DownCast(geomCurve);
                Handle(Geom_Curve) basisCurve = trimmedCurve->BasisCurve();
                if (!basisCurve->IsKind(STANDARD_TYPE(Geom_Line)))
                {
                    return SelectFilterStatus::Continue;
                }
            }
            else if (!geomCurve->IsKind(STANDARD_TYPE(Geom_Line)))
            {
                return SelectFilterStatus::Continue;
            }

            // 曲线的端点是否都在平面内
            gp_Pnt startOccPnt = geomCurve->Value(first);
            wy::Vector3 startPnt(startOccPnt.X(), startOccPnt.Y(), startOccPnt.Z());
            if (_plane.distanceTo(startPnt) > wy3d::EPS)
            {
                return SelectFilterStatus::Continue;
            }
            gp_Pnt endOccPnt = geomCurve->Value(last);
            wy::Vector3 endPnt(endOccPnt.X(), endOccPnt.Y(), endOccPnt.Z());
            if (_plane.distanceTo(endPnt) > wy3d::EPS)
            {
                return SelectFilterStatus::Continue;
            }
            
            return SelectFilterStatus::Ok;
        }
        // 基准面
        else if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::Element)
        {
            const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
            if (!pDatumPlane)
            {
                return SelectFilterStatus::Continue;
            }

            // 平面平行则直接continue
            const wy3d::SketchPlane& plane = pDatumPlane->getPlane();
            double normalDotProduct = _plane.getNormal().dot(plane.getNormal());
            if (std::fabs(std::fabs(normalDotProduct) - 1.0) <= wy3d::EPS) // 法向量点积的绝对值接近于1则表明两面平行
            {
                return SelectFilterStatus::Continue;
            }

            return SelectFilterStatus::Ok;
        }
        // 其它
        else
        {
            return SelectFilterStatus::Continue;
        }
    }

private:
    wy3d::SketchPlane _plane;
};

AngularDatumPlnCmd::AngularDatumPlnCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _plane(), _rotateAnglePlane(), _angle(0.0)
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

AngularDatumPlnCmd::~AngularDatumPlnCmd()
{
}

wyap::CmdExecution::StartResult AngularDatumPlnCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    this->initStep2ndTip();

    // 初始化
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectDatumPlaneOrFace);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void AngularDatumPlnCmd::initStep2ndTip()
{
    _step2ndTip = QCoreApplication::translate("DatumPlnCmd",
        "Select solid edge or solid face or datum plane to determine the rotation axis.");
}
void AngularDatumPlnCmd::onEnd()
{
    GuiCommand::onEnd();

}
void AngularDatumPlnCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

}

void AngularDatumPlnCmd::cleanup()
{
    // 退出或重置时统一隐藏浮窗并清理悬停状态
    this->hidePopup();

    _step = Step::Undefined;
    _plane = wy3d::SketchPlane();
    _step2ndTip.clear();
    _intersectLineOrigin.set(0.0, 0.0);
    _intersectLineDir.set(0.0, 0.0);
    _rotateAxisLineStart.set(0.0, 0.0);
    _rotateAxisLineEnd.set(0.0, 0.0);
    _rotateAnglePlane = wy3d::SketchPlane();
    _angle = 0.0;
    _snapExcludeIds.clear();

    _pPreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _pMakeDatumPlane = nullptr;
    _pRotateAxisTransient = nullptr;
    _hoverPopupState.resetValue();
}

void AngularDatumPlnCmd::reset()
{
    this->cleanup();
}

bool AngularDatumPlnCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        if (!_plane.isValid())
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        this->gotoStep(Step::SpecifyRotationAxis);
        return true;
    }
    break;

    case Step::SpecifyRotationAxis:
    {
        // 创建基准面
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(_plane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 求出旋转轴直线段
        if (!computeRotateAxisLineSegment(_pMakeDatumPlane->getId(),
            _intersectLineOrigin, _intersectLineDir,
            _rotateAxisLineStart, _rotateAxisLineEnd))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 显示旋转轴直线段
        _pRotateAxisTransient = std::make_shared<SketchCurveTransient>(_plane, _rotateAxisLineStart, _rotateAxisLineEnd);

        // 调整基准面的中心点到旋转轴线的中点
        // 调整基准面的X轴和旋转轴线对齐
        wy::Vector3 axisStart3d = _plane.value(_rotateAxisLineStart);
        wy::Vector3 axisEnd3d = _plane.value(_rotateAxisLineEnd);
        wy::Vector3 axisDir = axisEnd3d - axisStart3d;
        axisDir.normalize();
        _plane = wy3d::SketchPlane((axisStart3d + axisEnd3d) / 2, _plane.getNormal(), axisDir);
        if (!_plane.isValid())
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        if (!_pMakeDatumPlane->update(_plane))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 刷新创建的基准面的显示
        if (Scene* pScene = Application::instance().getActiveScene())
        {
            pScene->updateDatumPlaneVisualSize(Application::instance().getActiveDatabase(), _pMakeDatumPlane->getId());
        }

        // 下一步
        _rotateAnglePlane = wy3d::SketchPlane(_plane.getOrigin(), _plane.getXDir(), _plane.getYDir());
        this->gotoStep(Step::SpecifyAngle);
    }
    break;

    case Step::SpecifyAngle:
    {
        if (!_pMakeDatumPlane)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 旋转基准面
        wy3d::SketchPlane rotatedPlane = this->rotatePlaneAroundXAxis(_angle);
        if (!_pMakeDatumPlane->update(rotatedPlane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeDatumPlane->commit();
        _pMakeDatumPlane = nullptr;

        // exit
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

void AngularDatumPlnCmd::gotoStep(Step step)
{
    // 切换步骤时先收起浮窗，避免残留在错误步骤上
    this->hidePopup();
    _hoverPopupState.resetValue();

    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Select datum plane or solid plane surface."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::DatumPlane);
        _pointPickOption.selType = wy3d::SelectionType::Face;
        _pointPickOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();
    }
    break;

    case Step::SpecifyRotationAxis:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(_step2ndTip);

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        if (_pSelSetHighlightor)
        {
            assert(_pSelSetHighlightor->getSelectionSet().getCount() == 1);
        }

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::DatumPlane);
        _pointPickOption.selType = wy3d::SelectionType::Face | wy3d::SelectionType::Edge;
        _pointPickOption.pSelFilter = std::make_shared<AngularDatumPlnCmdRotationAxisSelFilter>(_plane);
    }
    break;

    case Step::SpecifyAngle:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify the rotation angle."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 预览
        _pPreview = nullptr;

        // 捕捉排除项
        assert(_snapExcludeIds.empty());
        if (_pMakeDatumPlane && !_pMakeDatumPlane->getId().isNull())
        {
            _snapExcludeIds.insert(_pMakeDatumPlane->getId());
        }
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

void AngularDatumPlnCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void AngularDatumPlnCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        // 鼠标移动后重新计时，并关闭当前浮窗
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SpecifyRotationAxis:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SpecifyAngle:
    {
        double angle(0.0);
        if (this->computeRotationAngle(event.x, event.y, _rotateAnglePlane, wy::Vector2::kXAxis, _snapExcludeIds, angle))
        {
            // 记录绝对值和方向，供悬停浮窗提交时使用
            _hoverPopupState.angleSign = angle < 0.0 ? -1 : 1;
            _hoverPopupState.angle = std::fabs(angle);
            {
                if (_pMakeDatumPlane) _pMakeDatumPlane->update(this->rotatePlaneAroundXAxis(angle));
            }
            return;
        }
        else
        {
            assert(false);
            return;
        }
    }
    break;
    }

    return;
}

void AngularDatumPlnCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyAngle:
    {
        double angle(0.0);
        if (this->computeRotationAngle(event.x, event.y, _rotateAnglePlane, wy::Vector2::kXAxis, _snapExcludeIds, angle))
        {
            _angle = angle;
            this->finishStep(_step);
            return;
        }
        else
        {
            assert(false);
            return;
        }
    }
    break;
    }

    return;
}

void AngularDatumPlnCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
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

        // 获取平面
        if (!MakeDatumPlane::getSketchPlane(sel, _plane))
        {
            assert(false);
            _pSelSetHighlightor->clearSelections();
            return;
        }

        // finish step
        this->finishStep(_step);
        return;
    }
    break;

    case Step::SpecifyRotationAxis:
    {
        if (!_pPreview || _pPreview->getSelection().getElementId().isNull())
        {
            return;
        }
        wyap::Selection sel = _pPreview->getSelection();
        _pPreview = nullptr;

        unsigned int selType = sel.getSelectionType();
        if (selType == static_cast<unsigned int>(wy3d::SelectionType::Face)
            || selType == static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            // 面
            wy3d::SketchPlane plane;
            if (!MakeDatumPlane::getSketchPlane(sel, plane))
            {
                assert(false);
                return;
            }

            // 求两个平面的交线
            wy::Vector3 linePnt, lineDir;
            if (!MakeDatumPlane::intersect(_plane, plane, linePnt, lineDir))
            {
                assert(false);
                return;
            }

            // 交线
            _intersectLineOrigin = _plane.uv(linePnt);
            _intersectLineDir = _plane.uv(linePnt + lineDir) - _intersectLineOrigin;
            _intersectLineDir.normalize();
            if (_intersectLineDir.length() < 0.5)
            {
                assert(false);
                return;
            }
        }
        else if (selType == static_cast<unsigned int>(wy3d::SelectionType::Edge))
        {
            // 边
            wy::Vector3 startPnt, endPnt;
            if (!MakeDatumPlane::getSolidEdgeEndPoints(sel, startPnt, endPnt)
                || (endPnt - startPnt).length() <= wy3d::TOL)
            {
                assert(false);
                return;
            }

            // 交线
            _intersectLineOrigin = _plane.uv(startPnt);
            _intersectLineDir = _plane.uv(endPnt) - _intersectLineOrigin;
            _intersectLineDir.normalize();
            if (_intersectLineDir.length() < 0.5)
            {
                assert(false);
                return;
            }
        }
        else
        {
            assert(false);
            return;
        }

        // finish step
        this->finishStep(_step);
        return;
    }
    break;
    }

    return;
}

void AngularDatumPlnCmd::initializePopups()
{
    if (_pAnglePopup)
    {
        return;
    }

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    _pAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
        QCoreApplication::translate("DatumPlnCmd", "Angle"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pAnglePopup->hide();
}

void AngularDatumPlnCmd::showPopup()
{
    if (_step != Step::SpecifyAngle)
    {
        return;
    }
    if (!_pAnglePopup)
    {
        this->initializePopups();
    }
    if (!_pAnglePopup)
    {
        return;
    }

    _pAnglePopup->setValue(wy3d::radiansToDegrees(_hoverPopupState.angle));
    _pAnglePopup->showAtGlobal(QCursor::pos());
}

void AngularDatumPlnCmd::hidePopup()
{
    if (_pAnglePopup && _pAnglePopup->isVisible())
    {
        _pAnglePopup->hide();
    }
}

void AngularDatumPlnCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyAngle)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pAnglePopup && _pAnglePopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void AngularDatumPlnCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyAngle || !_pAnglePopup)
    {
        return;
    }

    double angle(0.0);
    if (!parseDoubleText(_pAnglePopup->getRowText(), angle))
    {
        return;
    }
    // 浮窗里输入的是角度数值，方向沿用当前鼠标指向
    _angle = _hoverPopupState.angleSign < 0 ? -wy3d::degreesToRadians(std::fabs(angle)) : wy3d::degreesToRadians(std::fabs(angle));

    this->finishStep(_step);
}

void AngularDatumPlnCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

bool AngularDatumPlnCmd::computeRotateAxisLineSegment(
    const wydb::ElementId& datumPlnId,
    const wy::Vector2& lineOrigin, const wy::Vector2& lineDir,
    wy::Vector2& axisLineStart, wy::Vector2& axisLineEnd)
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene)
    {
        assert(false);
        return false;
    }

    ElementNode* pElemNode = pScene->getElementNode(datumPlnId);
    DatumPlaneElementNode* pDatumPlnElemNode = dynamic_cast<DatumPlaneElementNode*>(pElemNode);
    if (!pDatumPlnElemNode)
    {
        assert(false);
        return false;
    }

    const wy::Vector2& minPnt = pDatumPlnElemNode->getMinPoint();
    const wy::Vector2& maxPnt = pDatumPlnElemNode->getMaxPoint();
    assert(maxPnt.x() > minPnt.x());
    assert(maxPnt.y() > minPnt.y());

    return MakeDatumPlane::intersect(minPnt, maxPnt, lineOrigin, lineDir, axisLineStart, axisLineEnd);
}

wy3d::SketchPlane AngularDatumPlnCmd::rotatePlaneAroundXAxis(double angle) const
{
    wy::Vector2 uv(-std::sin(angle), std::cos(angle)); // (0,1)旋转angle角度
    wy::Vector3 newNormal = _rotateAnglePlane.value(uv) - _rotateAnglePlane.getOrigin();
    return wy3d::SketchPlane(_plane.getOrigin(), newNormal, _plane.getXDir());
}

void PerpendicularDatumPlnCmd::initStep2ndTip()
{
    _step2ndTip = QCoreApplication::translate("DatumPlnCmd",
        "Select solid edge or solid face or datum plane to determine the rotation axis.");
}

bool PerpendicularDatumPlnCmd::finishStep(Step step)
{
    if (Step::SpecifyRotationAxis == _step)
    {
        // 创建基准面
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(_plane))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 求出旋转轴直线段
        if (!computeRotateAxisLineSegment(_pMakeDatumPlane->getId(),
            _intersectLineOrigin, _intersectLineDir,
            _rotateAxisLineStart, _rotateAxisLineEnd))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 调整基准面的中心点到旋转轴线的中点
        // 调整基准面的X轴和旋转轴线对齐
        wy::Vector3 axisStart3d = _plane.value(_rotateAxisLineStart);
        wy::Vector3 axisEnd3d = _plane.value(_rotateAxisLineEnd);
        wy::Vector3 axisDir = axisEnd3d - axisStart3d;
        axisDir.normalize();
        _plane = wy3d::SketchPlane((axisStart3d + axisEnd3d) / 2, _plane.getNormal(), axisDir);
        if (!_plane.isValid())
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 求出旋转角度平面
        _rotateAnglePlane = wy3d::SketchPlane(_plane.getOrigin(), _plane.getXDir(), _plane.getYDir());

        // 旋转90度
        wy3d::SketchPlane rotatedPlane = this->rotatePlaneAroundXAxis(wy3d::PI_2);
        if (!_pMakeDatumPlane->update(rotatedPlane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeDatumPlane->commit();
        _pMakeDatumPlane = nullptr;

        // 下一步
        this->requestEnd();
        return true;
    }
    else
    {
        return AngularDatumPlnCmd::finishStep(step);
    }
}

void ThroughAxisDatumPlnCmd::initStep2ndTip()
{
    _step2ndTip = QCoreApplication::translate("DatumPlnCmd",
        "Select solid edge or solid face or datum plane to determine the X axis.");
}

bool ThroughAxisDatumPlnCmd::finishStep(Step step)
{
    if (Step::SpecifyRotationAxis == _step)
    {
        // 创建基准面
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(_plane))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 求出旋转轴直线段
        if (!computeRotateAxisLineSegment(_pMakeDatumPlane->getId(),
            _intersectLineOrigin, _intersectLineDir,
            _rotateAxisLineStart, _rotateAxisLineEnd))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 调整基准面的中心点到旋转轴线的中点
        // 调整基准面的X轴和旋转轴线对齐
        wy::Vector3 axisStart3d = _plane.value(_rotateAxisLineStart);
        wy::Vector3 axisEnd3d = _plane.value(_rotateAxisLineEnd);
        wy::Vector3 axisDir = axisEnd3d - axisStart3d;
        axisDir.normalize();
        _plane = wy3d::SketchPlane((axisStart3d + axisEnd3d) / 2, _plane.getNormal(), axisDir);
        if (!_plane.isValid())
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        if (!_pMakeDatumPlane->update(_plane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeDatumPlane->commit();
        _pMakeDatumPlane = nullptr;

        // 下一步
        this->requestEnd();
        return true;
    }
    else
    {
        return AngularDatumPlnCmd::finishStep(step);
    }
}
