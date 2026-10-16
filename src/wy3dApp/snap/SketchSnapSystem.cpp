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

#include "SketchSnapSystem.h"
#include "select/PointPick.h"
#include <cassert>
#include <QToolTip>
#include <QApplication>
#include <wyVector2.h>
#include <utils/wy3dCurveIntersectionUtil.h>
#include <wydbDatabase.h>
#include <wyapApplication.h>
#include <wyapViewManager.h>
#include <wyapSceneManager.h>
#include <wyapView.h>
#include <wy3dImpl.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>

#include <wy3dApp/Application.h>
#include "SnapContext.h"
#include "SnapConsts.h"
#include "utils/MathUtils.h"
#include <wy3dApp/GuiCommand.h>
#include "utils/StlUtil.h"
#include "scene/nodes/ElementNodeType.h"

#include <iostream>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

typedef AngleSnapObjectSPtr AngleSPtr;
typedef ParallelSnapObjectSPtr ParlSPtr;
typedef PerpendicularSnapObjectSPtr PerpSPtr;
typedef AlignXSnapObjectSPtr AlignXSPtr;
typedef AlignYSnapObjectSPtr AlignYSPtr;

#define NULL_ID wydb::ElementId::kNull
#define MKSPTR std::make_shared
#define MKSPTR_HORZ   std::make_shared<HorizontalSnapObject>
#define MKSPTR_VERT   std::make_shared<VerticalSnapObject>
#define MKSPTR_AlignX std::make_shared<AlignXSnapObject>
#define MKSPTR_AlignY std::make_shared<AlignYSnapObject>
#define MKSPTR_Parl   std::make_shared<ParallelSnapObject>
#define MKSPTR_Perp   std::make_shared<PerpendicularSnapObject>
#define MKSPTR_Equal  std::make_shared<EqualSnapObject>
#define MOVE std::move

class SketchSnapPreSelFilter : public SelectPreFilterFunctor
{
public:
    SketchSnapPreSelFilter(
        wyrx::ClassInfo* classInfo,
        const std::set<wydb::ElementId>* pExcludeIds)
        : _classInfo(classInfo), _pExcludeIds(pExcludeIds) {}

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        assert(pDb);

        // 拾取到的不是Element比如Gizmo继续遍历;
        if (id.isNull())
        {
            return SelectFilterStatus::Continue;
        }

        // 如果是排除的元素继续遍历;
        if (_pExcludeIds && _pExcludeIds->find(id) != _pExcludeIds->cend())
        {
            return SelectFilterStatus::Continue;
        }

        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            return SelectFilterStatus::Continue;
        }
        if (pElem->isKindOf(_classInfo))
        {
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Continue;
        }
    }

    void setExcludeIds(const std::set<wydb::ElementId>* pExcludeIds)
    {
        _pExcludeIds = pExcludeIds;
    }

private:
    wyrx::ClassInfo* _classInfo;
    const std::set<wydb::ElementId>* _pExcludeIds;
};

static const wy3d::Plane& sketchPlaneOf(const wy3d::Sketch* pSketch)
{
    assert(pSketch);
    return pSketch->getPlane();
}

SketchSnapSystem::SketchSnapSystem(const wy3d::Sketch* pSketch)
    : Snap2dSystemImpl(sketchPlaneOf(pSketch)), _sketchId(wydb::ElementId::kNull)
{
    _sketchId = pSketch->getId();

    // 初始化
    this->init(pSketch);

    // 点选选项
    _pickOption.pickHalfSize = SnapConsts::PickSize;
    _pickOption.allowedElemTypes = ElementNodeType::SketchEntity;
    _pickOption.selType = wy3d::SelectionType::Element;
    _pickOption.pPreFilter = std::make_shared<SketchSnapPreSelFilter>(wy3d::SketchCurve::classInfo(), nullptr);
}

const wy3d::Sketch* SketchSnapSystem::getSketch() const
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    return wy3d::Sketch::cast(pDb->getElement(_sketchId));
}

bool SketchSnapSystem::init(const wy3d::Sketch* pSketch)
{
    if (!pSketch)
    {
        assert(false);
        return false;
    }
    wydb::Database* pDb = pSketch->getDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    // 创建所有的捕捉对象
    wydb::ElementId id(wydb::ElementId::kNull);
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        id = iter.current();
        this->appendSnapObjects(pDb, id);
    }

    return true;
}

bool SketchSnapSystem::refreshPlane(const wydb::Database* pDb)
{
    // added by wangyao 2025.06.05 {
    // 当草图ID为空时,表明当前捕捉系统应用于3D建模场景下的工作平面捕捉,不用局部更新;
    if (_sketchId.isNull())
    {
        return false;
    }
    //}

    // added by wangyao 2025.06.02 {
    // 重定位草图坐标系命令有可能更改草图平面的方位,所以每次局部刷新的时候也一并刷新草图平面信息.
    if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchId)))
    {
        _sketchPlane = pSketch->getPlane();
    }
    else
    {
        assert(false);
    }
    // }

    return true;
}

void SketchSnapSystem::rebuildAll()
{
    Snap2dSystemImpl::rebuildAll();  // reset + initDefaultSnapObjects（基类事务）
    this->init(this->getSketch());   // 草图元素（子类事务）
}

wydb::ElementId SketchSnapSystem::pickElement(
    const wydb::Database* pDb, osgViewer::View* pView,
    double x, double y, const std::set<wydb::ElementId>& excludeIds)
{
    SketchSnapPreSelFilter* pPreSelFilter = dynamic_cast<SketchSnapPreSelFilter*>(_pickOption.pPreFilter.get());
    if (pPreSelFilter) pPreSelFilter->setExcludeIds(&excludeIds);
    wyap::Selection sel = PointPick::pick(pDb, pView, x, y, _pickOption);
    return sel.getElementId();
}

Snap2dResultSPtr SketchSnapSystem::pickedDispatch(
    const SketchSnapContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    if (const SketchLocateContext* pLocateContext = dynamic_cast<const SketchLocateContext*>(pContext))
    {
        return this->snapImplPicked(pLocateContext, pDb, pickedId, pos, scaleScreenX, scaleScreenY);
    }
    else if (const SketchDrawLineContext* pDrawLineContext = dynamic_cast<const SketchDrawLineContext*>(pContext))
    {
        return this->snapImplPicked(pDrawLineContext, pDb, pickedId, pos, scaleScreenX, scaleScreenY);
    }
    else if (const SketchDrawCircleContext* pDrawCircleContext = dynamic_cast<const SketchDrawCircleContext*>(pContext))
    {
        return this->snapImplPicked(pDrawCircleContext, pDb, pickedId, pos, scaleScreenX, scaleScreenY);
    }
    else if (const SketchDrawArcContext* pDrawArcContext = dynamic_cast<const SketchDrawArcContext*>(pContext))
    {
        return this->snapImplPicked(pDrawArcContext, pDb, pickedId, pos, scaleScreenX, scaleScreenY);
    }
    // Curve end point. Deliberately no counterpart in the no-picked section below:
    // this context snaps only to the eligible curve end points, align snaps never appear.
    else if (const SketchCurveEndPointContext* pCurveEndPointContext = dynamic_cast<const SketchCurveEndPointContext*>(pContext))
    {
        return this->snapImplPicked(pCurveEndPointContext, pDb, pickedId, pos, scaleScreenX, scaleScreenY);
    }

    return nullptr;
}

void SketchSnapSystem::appendSnapObjects(const wydb::Database* pDb, const wydb::ElementId& id)
{
    assert(pDb);
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
    if (!pSketchCurve) return;
    _ids.insert(id);

    // 直线段
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        if (pLine->getLength() <= wy3d::TOL) return;
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();

        // 平行
        this->appendAngleSnapObject(MKSPTR_Parl(id, startPnt, endPnt), _parlAndPerps);
        // 垂直
        this->appendAngleSnapObject(MKSPTR_Perp(id, startPnt, endPnt), _parlAndPerps);
        // 对齐X
        this->appendAlignSnapObject<AlignXSPtr>(
            _alignXMap, MKSPTR_AlignX(id, startPnt, SketchSnapObject::START));
        this->appendAlignSnapObject<AlignXSPtr>(
            _alignXMap, MKSPTR_AlignX(id, endPnt, SketchSnapObject::END));
        // 对齐Y
        this->appendAlignSnapObject<AlignYSPtr>(
            _alignYMap, MKSPTR_AlignY(id, startPnt, SketchSnapObject::START));
        this->appendAlignSnapObject<AlignYSPtr>(
            _alignYMap, MKSPTR_AlignY(id, endPnt, SketchSnapObject::END));
        // 相等
        double angle = SketchSnapObject::computeParallelAngle(startPnt, endPnt);
        assert(angle >= 0.0 && angle < wy3d::PI);
        this->appendEqualSnapObject(
            MKSPTR_Equal(id, pLine->getLength(), SketchSnapObject::LINE, angle));
    }
    // 中心线
    else if (const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        if (pCenterLine->getLength() <= wy3d::TOL) return;
        wy::Vector2 startPnt = pCenterLine->getStartPoint();
        wy::Vector2 endPnt = pCenterLine->getEndPoint();

        // 平行
        this->appendAngleSnapObject(MKSPTR_Parl(id, startPnt, endPnt), _parlAndPerps);
        // 垂直
        this->appendAngleSnapObject(MKSPTR_Perp(id, startPnt, endPnt), _parlAndPerps);
        // 对齐X
        this->appendAlignSnapObject<AlignXSPtr>(
            _alignXMap, MKSPTR_AlignX(id, startPnt, SketchSnapObject::START));
        this->appendAlignSnapObject<AlignXSPtr>(
            _alignXMap, MKSPTR_AlignX(id, endPnt, SketchSnapObject::END));
        // 对齐Y
        this->appendAlignSnapObject<AlignYSPtr>(
            _alignYMap, MKSPTR_AlignY(id, startPnt, SketchSnapObject::START));
        this->appendAlignSnapObject<AlignYSPtr>(
            _alignYMap, MKSPTR_AlignY(id, endPnt, SketchSnapObject::END));
        // 相等
        double angle = SketchSnapObject::computeParallelAngle(startPnt, endPnt);
        assert(angle >= 0.0 && angle < wy3d::PI);
        this->appendEqualSnapObject(
            MKSPTR_Equal(id, pCenterLine->getLength(), SketchSnapObject::LINE, angle));
    }
    // 圆
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        if (pCircle->getRadius() <= wy3d::TOL) return;
        wy::Vector2 center = pCircle->getCenter();

        // 对齐X
        this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(id, center));
        // 对齐Y
        this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(id, center));
        // 相等
        this->appendEqualSnapObject(
            MKSPTR_Equal(id, pCircle->getRadius(), SketchSnapObject::CIRCLE));
    }
    // 圆弧
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        if (pArc->getRadius() <= wy3d::TOL) return;
        if (pArc->getTotalAngle() <= wy3d::TOL) return;
        wy::Vector2 center = pArc->getCenter();

        // 对齐X
        this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(id, center));
        // 对齐Y
        this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(id, center));
        // 相等
        this->appendEqualSnapObject(
            MKSPTR_Equal(id, pArc->getRadius(), SketchSnapObject::CIRCLE));
    }
    // 椭圆
    else if (const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        if (pEllipse->getMajorRadius() <= wy3d::TOL) return;
        if (pEllipse->getMinorRadius() <= wy3d::TOL) return;
        wy::Vector2 center = pEllipse->getCenter();

        // 对齐X
        this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(id, center));
        // 对齐Y
        this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(id, center));
    }
    // 椭圆弧
    else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        if (pEllipseArc->getMajorRadius() <= wy3d::TOL) return;
        if (pEllipseArc->getMinorRadius() <= wy3d::TOL) return;
        if (pEllipseArc->getTotalAngle() <= wy3d::TOL) return;
        wy::Vector2 center = pEllipseArc->getCenter();

        // 对齐X
        this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(id, center));
        // 对齐Y
        this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(id, center));
    }
    // 样条曲线
    else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        const std::vector<wy::Vector2>& fitPoints = pSpline->getPoints();
        if (fitPoints.size() >= 2)
        {
            // 对齐X
            this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(id, fitPoints.front()));
            // 对齐Y
            this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(id, fitPoints.front()));
            // 对齐X
            this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(id, fitPoints.back()));
            // 对齐Y
            this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(id, fitPoints.back()));
        }
    }
    else
    {
        assert(false);
    }
}

// 计算点到直线段的投影点
static inline bool projectPointOnLineseg(
    const wy::Vector2& point,
    const wy::Vector2& lineStart, const wy::Vector2& lineEnd,
    wy::Vector2& outPnt)
{
    wy::Vector2 lineVec = lineEnd - lineStart;
    double length = lineVec.length();
    if (length <= wy3d::EPS) // 直线段退化为点
    {
        return false;
    }

    double t = (point - lineStart).dot(lineVec) / length / length;
    if (t > 0.0 && t < 1.0)
    {
        outPnt = lineStart + t * lineVec;
        return true;
    }
    else
    {
        return false;
    }
}

GuiCmdTransientSPtr SketchSnapSystem::createHighlightTransient(
    const wydb::Database* pDb, const wydb::ElementId& id) const
{
    assert(pDb);
    if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id)))
    {
        return std::make_shared<SketchCurveTransient>(pSketchCurve);
    }
    return nullptr;
}

// 计算点到圆上的投影点
static inline bool projectPointOnCircle(
    const wy::Vector2& point,
    const wy::Vector2& center, double radius,
    wy::Vector2& outPnt)
{
    if (radius <= wy3d::EPS) // 圆退化为点
    {
        return false;
    }

    wy::Vector2 vec = point - center;
    if (vec.length2() <= wy3d::EPS * wy3d::EPS)
    {
        return false; // 点与圆心几乎重合
    }
    vec.normalize();
    vec *= radius;
    outPnt = center + vec;
    return true;
}

// 计算点到圆弧上的投影点
static inline bool projectPointOnArc(
    const wy::Vector2& point,
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    wy::Vector2& outPnt)
{
    if (radius <= wy3d::EPS) // 圆退化为点
    {
        return false;
    }

    wy::Vector2 vec = point - center;
    if (vec.length2() <= wy3d::EPS * wy3d::EPS)
    {
        return false; // 点与圆心几乎重合
    }

    startAngle = wy3d::normalizeRadian(startAngle); // [0, 2PI)
    endAngle = wy3d::normalizeRadian(endAngle); // [0, 2PI)
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;
    if (totalAngle <= wy3d::EPS)
    {
        return false; // 圆弧退化为点
    }

    double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, vec); // [0, 2PI)
    if (angle < startAngle) angle += wy3d::TWO_PI;
    double delta = angle - startAngle;
    if (delta > 0.0 && delta < totalAngle)
    {
        vec.normalize();
        vec *= radius;
        outPnt = center + vec;
        return true;
    }
    else
    {
        return false;
    }
}

// 定位点+捕捉到对象
Snap2dResultSPtr SketchSnapSystem::snapImplPicked(
    const SketchLocateContext* pLocateContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    assert(pLocateContext);
    assert(_pSnapContext);
    assert(pLocateContext == _pSnapContext.get());
    assert(pDb);
    assert(!pickedId.isNull());

    const wydb::Element* pPickedElem = pDb->getElement(pickedId);
    assert(pPickedElem);
    // 捕捉到直线
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pPickedElem))
    {
        // 捕捉点在直线段上
        wy::Vector2 projPnt;
        if (projectPointOnLineseg(pos, pLine->getStartPoint(), pLine->getEndPoint(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::LINE);
            return std::make_shared<Snap2dResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    // 捕捉到圆
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pPickedElem))
    {
        // 捕捉点在圆上
        wy::Vector2 projPnt;
        if (projectPointOnCircle(pos, pCircle->getCenter(), pCircle->getRadius(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::CIRCLE);
            return std::make_shared<Snap2dResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pPickedElem))
    {
        // 捕捉点在圆弧上
        wy::Vector2 projPnt;
        if (projectPointOnArc(pos, pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::ARC);
            return std::make_shared<Snap2dResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    else
    {
        return nullptr;
    }

    return nullptr;
}

// 绘制直线+捕捉到对象
Snap2dResultSPtr SketchSnapSystem::snapImplPicked(
    const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId, const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    assert(pDrawLineContext);
    assert(_pSnapContext);
    assert(pDrawLineContext == _pSnapContext.get());
    assert(pDb);
    assert(!pickedId.isNull());
    wy::Vector2 drawStartPnt = pDrawLineContext->getStartPoint();
    wy::Vector2 vec = pos - drawStartPnt;
    double length = vec.length();
    if (length <= wy3d::TOL)
    {
        return nullptr; // 绘制的直线段太短不捕捉
    }

    const wydb::Element* pPickedElem = pDb->getElement(pickedId);
    assert(pPickedElem);
    // 捕捉到直线段
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pPickedElem))
    {
        // 计算垂足
        if (_dynSnapObjs.id2PointSnapObjects.find(pickedId) == _dynSnapObjs.id2PointSnapObjects.cend())
        {
            // 垂足
            wy::Vector2 foot;
            if (MathUtils::perpendicularFootOnLineseg(drawStartPnt, pLine->getStartPoint(), pLine->getEndPoint(), foot))
            {
                PointSnapObjectSPtr pSnapObj = std::make_shared<PerpendicularPointSnapObject>(pickedId, foot, 0);
                _dynSnapObjs.id2PointSnapObjects[pickedId].push_back(pSnapObj);
            }
        }

        // 捕捉垂足
        PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[pickedId], pos, 10, scaleScreenX, scaleScreenY);
        if (pPointSnapRet)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        // 捕捉点在直线段上
        wy::Vector2 projPnt;
        if (projectPointOnLineseg(pos, pLine->getStartPoint(), pLine->getEndPoint(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurveSnapObject = std::make_shared<PointOnCurveSnapObject>(
                pLine->getId(), SketchSnapObject::LINE);
            return std::make_shared<Snap2dResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurveSnapObject);
        }
    }
    // 捕捉到圆
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pPickedElem))
    {
        // 计算切点&垂足
        if (_dynSnapObjs.id2PointSnapObjects.find(pickedId) == _dynSnapObjs.id2PointSnapObjects.cend())
        {
            std::list<PointSnapObjectSPtr>& pointSnapObjs = _dynSnapObjs.id2PointSnapObjects[pickedId];
            // 切点
            wy::Vector2 tangentPnt1, tangentPnt2;
            if (MathUtils::tangentPointsToCircle(drawStartPnt, pCircle->getCenter(), pCircle->getRadius(), tangentPnt1, tangentPnt2))
            {
                PointSnapObjectSPtr pSnapObj1 = std::make_shared<TangentPointSnapObject>(pickedId, tangentPnt1, 0);
                pointSnapObjs.push_back(pSnapObj1);
                PointSnapObjectSPtr pSnapObj2 = std::make_shared<TangentPointSnapObject>(pickedId, tangentPnt2, 1);
                pointSnapObjs.push_back(pSnapObj2);
            }
            // 垂足
            wy::Vector2 foot1, foot2;
            if (MathUtils::perpendicularFootsOnCircle(drawStartPnt, pCircle->getCenter(), pCircle->getRadius(), foot1, foot2))
            {
                PointSnapObjectSPtr pSnapObj1 = std::make_shared<PerpendicularPointSnapObject>(pickedId, foot1, 0);
                pointSnapObjs.push_back(pSnapObj1);
                PointSnapObjectSPtr pSnapObj2 = std::make_shared<PerpendicularPointSnapObject>(pickedId, foot2, 1);
                pointSnapObjs.push_back(pSnapObj2);
            }
        }

        // 捕捉切点垂足
        PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[pickedId], pos, 10, scaleScreenX, scaleScreenY);
        if (pPointSnapRet)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        // 捕捉点在圆上
        wy::Vector2 projPnt;
        if (projectPointOnCircle(pos, pCircle->getCenter(), pCircle->getRadius(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::CIRCLE);
            return std::make_shared<Snap2dResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    // 捕捉到圆弧
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pPickedElem))
    {
        // 计算切点&垂足
        if (_dynSnapObjs.id2PointSnapObjects.find(pickedId) == _dynSnapObjs.id2PointSnapObjects.cend())
        {
            std::list<PointSnapObjectSPtr>& pointSnapObjs = _dynSnapObjs.id2PointSnapObjects[pickedId];
            // 切点
            std::vector<wy::Vector2> tangentPnts;
            if (MathUtils::tangentPointsToArc(drawStartPnt, pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(), tangentPnts))
            {
                short extra(-1);
                for (const wy::Vector2& tangentPnt : tangentPnts)
                {
                    ++extra;
                    PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(pickedId, tangentPnt, extra);
                    pointSnapObjs.push_back(pSnapObj);
                }
            }
            // 垂足
            std::vector<wy::Vector2> foots;
            if (MathUtils::perpendicularFootsOnArc(drawStartPnt, pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(), foots))
            {
                short extra(-1);
                for (const wy::Vector2& foot : foots)
                {
                    ++extra;
                    PointSnapObjectSPtr pSnapObj = std::make_shared<PerpendicularPointSnapObject>(pickedId, foot, extra);
                    pointSnapObjs.push_back(pSnapObj);
                }
            }
        }

        // 捕捉切点垂足
        PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[pickedId], pos, 10, scaleScreenX, scaleScreenY);
        if (pPointSnapRet)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        // 捕捉点在圆弧上
        wy::Vector2 projPnt;
        if (projectPointOnArc(pos, pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::ARC);
            return std::make_shared<Snap2dResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    else
    {
        return nullptr;
    }

    return nullptr;
}

// 绘制圆+捕捉到对象
Snap2dResultSPtr SketchSnapSystem::snapImplPicked(
    const SketchDrawCircleContext* pDrawCircleContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    assert(pDrawCircleContext);
    assert(_pSnapContext);
    assert(pDrawCircleContext == _pSnapContext.get());
    assert(pDb);
    assert(!pickedId.isNull());
    wy::Vector2 drawCenterPnt = pDrawCircleContext->getCenter();
    wy::Vector2 drawVec = pos - drawCenterPnt;
    double drawRadius = drawVec.length();
    if (drawRadius <= wy3d::TOL)
    {
        return nullptr; // 绘制的圆太小则不捕捉
    }

    const wydb::Element* pPickedElem = pDb->getElement(pickedId);
    assert(pPickedElem);
    // 捕捉到直线
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pPickedElem))
    {
        // 计算切点
        if (_dynSnapObjs.id2PointSnapObjects.find(pickedId) == _dynSnapObjs.id2PointSnapObjects.cend())
        {
            // 垂足实际上就是切点
            wy::Vector2 foot;
            if (MathUtils::perpendicularFootOnLineseg(drawCenterPnt, pLine->getStartPoint(), pLine->getEndPoint(), foot))
            {
                PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(pickedId, foot, 0);
                _dynSnapObjs.id2PointSnapObjects[pickedId].push_back(pSnapObj);
            }
        }

        // 捕捉切点
        PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[pickedId], pos, 100, scaleScreenX, scaleScreenY);
        if (pPointSnapRet)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }
        else
        {
            return nullptr;
        }
    }
    // 捕捉到圆
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pPickedElem))
    {
        // 计算切点
        if (_dynSnapObjs.id2PointSnapObjects.find(pickedId) == _dynSnapObjs.id2PointSnapObjects.cend())
        {
            std::list<PointSnapObjectSPtr>& pointSnapObjs = _dynSnapObjs.id2PointSnapObjects[pickedId];
            wy::Vector2 foot1, foot2;
            if (MathUtils::perpendicularFootsOnCircle(drawCenterPnt, pCircle->getCenter(), pCircle->getRadius(), foot1, foot2))
            {
                PointSnapObjectSPtr pSnapObj1 = std::make_shared<TangentPointSnapObject>(pickedId, foot1, 0);
                pointSnapObjs.push_back(pSnapObj1);
                PointSnapObjectSPtr pSnapObj2 = std::make_shared<TangentPointSnapObject>(pickedId, foot2, 1);
                pointSnapObjs.push_back(pSnapObj2);
            }
        }

        // 捕捉切点
        PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject_Nearest(_dynSnapObjs.id2PointSnapObjects[pickedId], pos, 100, scaleScreenX, scaleScreenY);
        if (pPointSnapRet)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }
        else
        {
            return nullptr;
        }
    }

    return nullptr;
}

// 绘制圆弧+捕捉到对象
Snap2dResultSPtr SketchSnapSystem::snapImplPicked(
    const SketchDrawArcContext* pDrawArcContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    assert(pDrawArcContext);
    assert(_pSnapContext);
    assert(pDrawArcContext == _pSnapContext.get());
    assert(pDb);
    assert(!pickedId.isNull());
    wy::Vector2 drawCenterPnt = pDrawArcContext->getCenter();
    double drawRadius = pDrawArcContext->getRadius();

    const wydb::Element* pPickedElem = pDb->getElement(pickedId);
    assert(pPickedElem);
    // 捕捉到直线
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pPickedElem))
    {
        // 计算交点
        if (_dynSnapObjs.id2PointSnapObjects.find(pickedId) == _dynSnapObjs.id2PointSnapObjects.cend())
        {
            std::list<PointSnapObjectSPtr>& pointSnapObjs = _dynSnapObjs.id2PointSnapObjects[pickedId];

            wy::Vector2 intPnt1, intPnt2;
            unsigned int numIntPnts = wy3d::intersectLinesegCircle(pLine->getStartPoint(), pLine->getEndPoint(),
                drawCenterPnt, drawRadius, intPnt1, intPnt2);
            if (numIntPnts == 1)
            {
                PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(pickedId, intPnt1, 0);
                pointSnapObjs.push_back(pSnapObj);
            }
            else if (numIntPnts == 2)
            {
                PointSnapObjectSPtr pSnapObj1 = std::make_shared<IntersectionPointSnapObject>(pickedId, intPnt1, 0);
                pointSnapObjs.push_back(pSnapObj1);
                PointSnapObjectSPtr pSnapObj2 = std::make_shared<IntersectionPointSnapObject>(pickedId, intPnt2, 1);
                pointSnapObjs.push_back(pSnapObj2);
            }
        }

        // 捕捉垂足
        PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[pickedId], pos, 10, scaleScreenX, scaleScreenY);
        if (pPointSnapRet)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        return nullptr;
    }
    // 捕捉到圆
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pPickedElem))
    {
        return nullptr;
    }

    return nullptr;
}

// 曲线端点
Snap2dResultSPtr SketchSnapSystem::snapImplPicked(
    const SketchCurveEndPointContext* pCurveEndPointContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    assert(pCurveEndPointContext);
    assert(_pSnapContext);
    assert(pCurveEndPointContext == _pSnapContext.get());
    assert(pDb);
    assert(!pickedId.isNull());

    // Only lines, centerlines, arcs, ellipse arcs and open splines of this sketch qualify
    const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(pickedId));
    if (!pCurve || pCurve->isClosed() || pCurve->getParent() != _sketchId)
    {
        return nullptr;
    }

    std::list<PointSnapObjectSPtr> pointSnapObjs;
    pointSnapObjs.push_back(std::make_shared<EndPointSnapObject>(
        pickedId, pCurve->getStartPoint(), SketchSnapObject::START));
    pointSnapObjs.push_back(std::make_shared<EndPointSnapObject>(
        pickedId, pCurve->getEndPoint(), SketchSnapObject::END));

    PointSnapObjectSPtr pSnapObj = this->snapPointSnapObject_Nearest(
        pointSnapObjs, pos, 10, scaleScreenX, scaleScreenY);
    if (pSnapObj)
    {
        return std::make_shared<Snap2dResult>(_sketchPlane, pSnapObj->getValue(), _pSnapContext, pSnapObj);
    }

    return nullptr;
}

} // namespace wy3dApp
