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

#include "Snap2dSystemImpl.h"
#include "Snap2dContextInternal.h"
#include "Snap2dResultFactory.h"
#include "Snap2dResultDisplay.h"
#include "SnapContext.h"
#include "SnapObject.h"
#include "SnapResult.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <wyVector2.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "utils/MathUtils.h"

namespace wy3dApp {

#define NULL_ID wydb::ElementId::kNull
#define MKSPTR std::make_shared
#define MKSPTR_HORZ   std::make_shared<Snap2dHorizontal>
#define MKSPTR_VERT   std::make_shared<Snap2dVertical>
#define MOVE std::move

Snap2dSystem::Snap2dSystem()
{
}

Snap2dSystem::~Snap2dSystem()
{
}

Snap2dRule::Snap2dRule()
{
}

Snap2dRule::~Snap2dRule()
{
}

Snap2dSystemSPtr Snap2dSystem::create(const wy3d::Plane& workPlane)
{
    return std::make_shared<Snap2dSystemImpl>(workPlane);
}

// Built-in rule: both faces delegate to the snap implementations for the rule's
// own context class. The static_cast in the bodies is safe because the rule table
// keys by the exact context class.
template <typename TContext>
class Snap2dSystemImpl::Rule : public Snap2dRuleHandler
{
public:
    explicit Rule(Snap2dSystemImpl& sys) : _sys(sys) {}

    Snap2dResultSPtr snapPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY,
        const std::set<wydb::ElementId>&) override
    {
        return _sys.snapImplPicked(
            static_cast<const TContext*>(pContext),
            pDb, pickedId, pos, valuePerPixelX, valuePerPixelY);
    }

    Snap2dResultSPtr snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY,
        const std::set<wydb::ElementId>& excludeIds) override
    {
        return _sys.snapImplNoPicked(
            static_cast<const TContext*>(pContext),
            pDb, pos, valuePerPixelX, valuePerPixelY, excludeIds);
    }

private:
    Snap2dSystemImpl& _sys;
};

// Adapter wrapping an external rule into the internal protocol: the reported
// position and hits become a Snap2dResult over the system's plane.
class Snap2dSystemImpl::ExternalRuleAdapter : public Snap2dRuleHandler
{
public:
    ExternalRuleAdapter(Snap2dSystemImpl& sys, Snap2dRuleSPtr pRule)
        : _sys(sys), _pRule(pRule)
    {
        assert(_pRule);
    }

    Snap2dResultSPtr snapPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY,
        const std::set<wydb::ElementId>& excludeIds) override
    {
        wy::Vector2 retPosition;
        std::vector<Snap2dHit> hits;
        if (!_pRule->snapPicked(pContext, pDb, pickedId, pos,
            valuePerPixelX, valuePerPixelY, excludeIds, retPosition, hits))
        {
            return nullptr;
        }

        return this->makeResult(retPosition, hits);
    }

    Snap2dResultSPtr snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY,
        const std::set<wydb::ElementId>& excludeIds) override
    {
        wy::Vector2 retPosition;
        std::vector<Snap2dHit> hits;
        if (!_pRule->snapNoPicked(pContext, pDb, pos,
            valuePerPixelX, valuePerPixelY, excludeIds, retPosition, hits))
        {
            return nullptr;
        }

        return this->makeResult(retPosition, hits);
    }

private:
    Snap2dResultSPtr makeResult(
        const wy::Vector2& retPosition, const std::vector<Snap2dHit>& hits) const
    {
        return makeSnap2dResult(_sys._sketchPlane, retPosition, hits);
    }

private:
    Snap2dSystemImpl& _sys;
    Snap2dRuleSPtr _pRule;
};

Snap2dSystemImpl::Snap2dSystemImpl(const wy3d::Plane& workPlane)
    : _sketchPlane(workPlane),
      _pResultDisplay(std::make_unique<Snap2dResultDisplay>(
          [this](const wydb::ElementId& id) { return this->createHighlightTransient(id); }))
{
    // 初始化默认捕捉对象
    this->initDefaultSnapObjects();

    // Register the built-in rules: one rule per context class, both faces. The
    // picked face lands on the virtual hooks (null for plane systems, overridden
    // by the sketch system).
    _rules.emplace(Locate2dContext::classInfo()->className(),
        std::make_shared<Rule<Locate2dContext>>(*this));
    _rules.emplace(DrawLine2dContext::classInfo()->className(),
        std::make_shared<Rule<DrawLine2dContext>>(*this));
    _rules.emplace(DrawCircle2dContext::classInfo()->className(),
        std::make_shared<Rule<DrawCircle2dContext>>(*this));
    _rules.emplace(DrawArc2dContext::classInfo()->className(),
        std::make_shared<Rule<DrawArc2dContext>>(*this));
    _rules.emplace(DrawLineTangent2dContext::classInfo()->className(),
        std::make_shared<Rule<DrawLineTangent2dContext>>(*this));
    assert(5 == _rules.size());
}

Snap2dSystemImpl::~Snap2dSystemImpl()
{
}

wy::ErrorStatus Snap2dSystemImpl::addRule(
    const wyrx::ClassInfo* pContextClass, Snap2dRuleSPtr pRule)
{
    if (!pRule || !pContextClass) return wy::ErrorStatus::InvalidInput;

    const std::string& contextClassName = pContextClass->className();
    if (contextClassName.empty()) return wy::ErrorStatus::InvalidInput;

    if (_rules.cend() != _rules.find(contextClassName)) return wy::ErrorStatus::DuplicateKey;

    _rules.emplace(contextClassName, std::make_shared<ExternalRuleAdapter>(*this, pRule));

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Snap2dSystemImpl::removeRule(const wyrx::ClassInfo* pContextClass)
{
    if (!pContextClass) return wy::ErrorStatus::InvalidInput;

    auto iter = _rules.find(pContextClass->className());
    if (_rules.cend() == iter) return wy::ErrorStatus::KeyNotFound;

    // Built-in rules are owned by the system.
    if (!dynamic_cast<ExternalRuleAdapter*>(iter->second.get()))
    {
        return wy::ErrorStatus::InvalidInput;
    }

    _rules.erase(iter);

    return wy::ErrorStatus::Ok;
}

wydb::ElementId Snap2dSystemImpl::pickElement(
    const wydb::Database* pDb, osgViewer::View* pView,
    double x, double y, const std::set<wydb::ElementId>& excludeIds)
{
    return wydb::ElementId::kNull;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const Locate2dContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const DrawLine2dContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const DrawCircle2dContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const DrawArc2dContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY)
{
    return nullptr;
}

bool Snap2dSystemImpl::refreshPlane(const wydb::Database* pDb)
{
    return false;
}

void Snap2dSystemImpl::initDefaultSnapObjects()
{
    // 水平和竖直
    _defaultAngleSnapObjects.reserve(5);
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_HORZ()));
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_VERT()));

    // 坐标原点对齐(常驻,不占动态对齐点名额)
    _originAlignRef.pAlignX = std::make_shared<Snap2dAlignX>(NULL_ID, wy::Vector2::kZero);
    _originAlignRef.pAlignY = std::make_shared<Snap2dAlignY>(NULL_ID, wy::Vector2::kZero);
}

bool Snap2dSystemImpl::partiallyUpdate(const wydb::Database* pDb)
{
    assert(pDb);
    if (!pDb)
    {
        return false;
    }

    _isPartiallyUpdatePending = false;

    // 刷新平面。工作平面型系统没有可刷新的平面,返回false属正常,
    // 不能据此早退,否则脏集合会一直累积且每帧重试。
    this->refreshPlane(pDb);

    // 对齐点: 元素被修改或删除都会使参照位置陈旧
    this->pruneAlignRefPoints(_modifiedIds);
    this->pruneAlignRefPoints(_erasedIds);

    // 参考图元池只存id,几何实时读取; 仅删除需要剔除
    this->pruneSnapRefElements(_erasedIds);

    // 跨帧瞬态数据
    _pSnapResult = nullptr;
    _dynSnapObjs.clear();

    // 无条件清空,避免早退路径导致集合无限累积
    _modifiedIds.clear();
    _erasedIds.clear();
    return true;
}

void Snap2dSystemImpl::beginSnapContext(const Snap2dContextSPtr& pContext)
{
    if (_pSnapContext == pContext) return;

    _pSnapContext = pContext;

    // 上下文(命令/步骤)切换时清空动态捕捉数据
    _dynSnapObjs.clear();
    _alignRefPoints.clear();
    _snapRefElements.clear();
}

void Snap2dSystemImpl::appendAlignRefPoint(
    const wydb::ElementId& id, const wy::Vector2& point)
{
    // 待删除的元素不再记录
    if (!id.isNull() && _erasedIds.find(id) != _erasedIds.cend()) return;

    // 按位置去重: 同一坐标只保留一条, 重复捕捉时刷新为最新
    auto iter = std::find_if(_alignRefPoints.begin(), _alignRefPoints.end(),
        [&point](const AlignRefPoint& ref)
        {
            assert(ref.pAlignX);
            return (ref.pAlignX->getRefPoint() - point).length2()
                <= wy3d::TOL * wy3d::TOL;
        });
    if (iter != _alignRefPoints.end())
    {
        if (iter->pAlignX->getId() == id)
        {
            AlignRefPoint existing = *iter; // id 相同: 复用对象, 只刷新顺序
            _alignRefPoints.erase(iter);
            _alignRefPoints.emplace_back(MOVE(existing));
            return; // 刷新为最新不触发淘汰
        }

        // id 变化: 同一位置来自另一个元素, 重建对象以关联最新元素
        _alignRefPoints.erase(iter);
    }

    AlignRefPoint ref;
    ref.pAlignX = std::make_shared<Snap2dAlignX>(id, point);
    ref.pAlignY = std::make_shared<Snap2dAlignY>(id, point);
    _alignRefPoints.emplace_back(MOVE(ref));

    // 超出上限则淘汰最早加入的点
    while (_alignRefPoints.size() > kMaxAlignRefPointCount)
    {
        _alignRefPoints.pop_front();
    }
}

void Snap2dSystemImpl::appendAlignRefPoints(const Snap2dResultSPtr& pSnapResult)
{
    if (!pSnapResult) return;

    for (const Snap2dHit& hit : pSnapResult->getHits())
    {
        if (this->isAlignRefPointType(hit.type))
        {
            this->appendAlignRefPoint(hit.element, hit.point);
        }
    }
}

bool Snap2dSystemImpl::isAlignRefPointType(Snap2dType type)
{
    switch (type)
    {
    case Snap2dType::Origin:
    case Snap2dType::GenericPoint:
    case Snap2dType::EndPoint:
    case Snap2dType::MiddlePoint:
    case Snap2dType::CenterPoint:
    case Snap2dType::QuadrantPoint:
        return true;

    default:
        return false;
    }
}

void Snap2dSystemImpl::pruneAlignRefPoints(
    const std::set<wydb::ElementId>& invalidIds)
{
    if (invalidIds.empty() || _alignRefPoints.empty()) return;

    for (auto iter = _alignRefPoints.begin(); iter != _alignRefPoints.end(); )
    {
        assert(iter->pAlignX);
        const wydb::ElementId id = iter->pAlignX->getId();
        if (!id.isNull() && invalidIds.find(id) != invalidIds.cend())
        {
            iter = _alignRefPoints.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

const Snap2dSystemImpl::AlignRefPoint* Snap2dSystemImpl::findAlignRefPoint(
    double value, bool isX, double tolPx, double valuePerPixel,
    const std::set<wydb::ElementId>& excludeIds) const
{
    assert(tolPx > 0.0);
    assert(valuePerPixel > 0.0);

    const double tol = std::fabs(tolPx * valuePerPixel);
    const AlignRefPoint* pRet = nullptr;
    double minDelta = DBL_MAX;

    auto consider = [&](const AlignRefPoint& ref)
    {
        assert(ref.pAlignX);
        const wydb::ElementId& id = ref.pAlignX->getId();
        if (excludeIds.find(id) != excludeIds.cend()) return;
        if (_erasedIds.find(id) != _erasedIds.cend()) return;

        const wy::Vector2& pnt = ref.pAlignX->getRefPoint();
        const double delta = std::fabs((isX ? pnt.x() : pnt.y()) - value);
        if (delta <= tol && delta < minDelta)
        {
            minDelta = delta;
            pRet = &ref;
        }
    };

    // 动态点优先于原点(并列时先扫到的优先)
    for (auto iter = _alignRefPoints.crbegin(); iter != _alignRefPoints.crend(); ++iter)
    {
        consider(*iter);
    }
    consider(_originAlignRef);

    return pRet;
}

Snap2dAlignXSPtr Snap2dSystemImpl::snapAlignRefX(
    double value, double tolPx, double valuePerPixel,
    const std::set<wydb::ElementId>& excludeIds) const
{
    const AlignRefPoint* pRef =
        this->findAlignRefPoint(value, true, tolPx, valuePerPixel, excludeIds);
    return pRef ? pRef->pAlignX : nullptr;
}

Snap2dAlignYSPtr Snap2dSystemImpl::snapAlignRefY(
    double value, double tolPx, double valuePerPixel,
    const std::set<wydb::ElementId>& excludeIds) const
{
    const AlignRefPoint* pRef =
        this->findAlignRefPoint(value, false, tolPx, valuePerPixel, excludeIds);
    return pRef ? pRef->pAlignY : nullptr;
}

// 模PI角距[0, PI/2], 处理0/PI边界
static double angleDistanceModPi(double angle1, double angle2)
{
    double delta = std::fabs(angle1 - angle2);
    if (delta > wy3d::PI_2) delta = wy3d::PI - delta;
    return delta;
}

// 可作为参考的草图曲线类型
static bool isSupportedSnapRefCurve(const wy3d::SketchCurve& curve)
{
    return wy3d::SketchLine::cast(&curve)
        || wy3d::SketchCenterLine::cast(&curve)
        || wy3d::SketchCircle::cast(&curve)
        || wy3d::SketchArc::cast(&curve);
}

const wy3d::SketchCurve* Snap2dSystemImpl::getSnapRefCurve(
    const wydb::Database* pDb, const wydb::ElementId& id) const
{
    if (!pDb || id.isNull()) return nullptr;
    if (_erasedIds.find(id) != _erasedIds.cend()) return nullptr;

    const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
    if (!pCurve) return nullptr;
    if (!isSupportedSnapRefCurve(*pCurve)) return nullptr;
    if (!this->isSnapRefElementAllowed(*pCurve)) return nullptr;
    return pCurve;
}

void Snap2dSystemImpl::appendSnapRefElement(
    const wydb::ElementId& id, const wydb::Database* pDb,
    const std::set<wydb::ElementId>& excludeIds)
{
    if (id.isNull()) return;

    // 已在池中: 只刷新MRU顺序, 不做数据库校验(重复悬停零DB开销)
    auto iter = std::find(_snapRefElements.begin(), _snapRefElements.end(), id);
    if (iter != _snapRefElements.end())
    {
        _snapRefElements.erase(iter);
        _snapRefElements.push_back(id);
        return;
    }

    if (excludeIds.find(id) != excludeIds.cend()) return;
    if (!this->getSnapRefCurve(pDb, id)) return;

    _snapRefElements.push_back(id);
    while (_snapRefElements.size() > kMaxSnapRefElementCount)
    {
        _snapRefElements.pop_front();
    }
}

void Snap2dSystemImpl::appendSnapRefElements(
    const Snap2dResultSPtr& pSnapResult, const wydb::Database* pDb,
    const std::set<wydb::ElementId>& excludeIds)
{
    if (!pSnapResult || !pDb) return;

    for (const Snap2dHit& hit : pSnapResult->getHits())
    {
        this->appendSnapRefElement(hit.element, pDb, excludeIds);
    }
}

void Snap2dSystemImpl::pruneSnapRefElements(
    const std::set<wydb::ElementId>& invalidIds)
{
    if (invalidIds.empty() || _snapRefElements.empty()) return;

    _snapRefElements.erase(
        std::remove_if(_snapRefElements.begin(), _snapRefElements.end(),
            [&invalidIds](const wydb::ElementId& id)
            {
                return invalidIds.find(id) != invalidIds.cend();
            }),
        _snapRefElements.end());
}

Snap2dAngleSPtr Snap2dSystemImpl::snapParallelOrPerpendicular(
    const wydb::Database* pDb, double drawAngle, double angleTol,
    const std::set<wydb::ElementId>& excludeIds) const
{
    Snap2dType bestType = Snap2dType::Undefined;
    wydb::ElementId bestId = wydb::ElementId::kNull;
    wy::Vector2 bestStartPnt, bestEndPnt;
    double minDelta = DBL_MAX;

    // 新->旧; 严格<, 并列取最新悬停的参考图元
    for (auto iter = _snapRefElements.crbegin(); iter != _snapRefElements.crend(); ++iter)
    {
        const wydb::ElementId& id = *iter;
        if (excludeIds.find(id) != excludeIds.cend()) continue;

        const wy3d::SketchCurve* pCurve = this->getSnapRefCurve(pDb, id);
        if (!pCurve) continue;

        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve);
        const wy3d::SketchCenterLine* pCenterLine =
            pLine ? nullptr : wy3d::SketchCenterLine::cast(pCurve);
        if (!pLine && !pCenterLine) continue;

        const wy::Vector2 startPnt =
            pLine ? pLine->getStartPoint() : pCenterLine->getStartPoint();
        const wy::Vector2 endPnt =
            pLine ? pLine->getEndPoint() : pCenterLine->getEndPoint();
        if ((endPnt - startPnt).length() <= wy3d::TOL) continue;

        const double parallelDelta = angleDistanceModPi(
            Snap2dObject::computeParallelAngle(startPnt, endPnt), drawAngle);
        if (parallelDelta <= angleTol && parallelDelta < minDelta)
        {
            minDelta = parallelDelta;
            bestType = Snap2dType::Parallel;
            bestId = id;
            bestStartPnt = startPnt;
            bestEndPnt = endPnt;
        }

        const double perpDelta = angleDistanceModPi(
            Snap2dObject::computePerpendicularAngle(startPnt, endPnt), drawAngle);
        if (perpDelta <= angleTol && perpDelta < minDelta)
        {
            minDelta = perpDelta;
            bestType = Snap2dType::Perpendicular;
            bestId = id;
            bestStartPnt = startPnt;
            bestEndPnt = endPnt;
        }
    }

    if (bestType == Snap2dType::Parallel)
    {
        return std::make_shared<Snap2dParallel>(bestId, bestStartPnt, bestEndPnt);
    }
    if (bestType == Snap2dType::Perpendicular)
    {
        return std::make_shared<Snap2dPerpendicular>(bestId, bestStartPnt, bestEndPnt);
    }
    return nullptr;
}

Snap2dEqualLengthSPtr Snap2dSystemImpl::snapEqualLengthInRefElements(
    const wydb::Database* pDb, double length, double tolPx,
    double valuePerPixel, double angle,
    const std::set<wydb::ElementId>& excludeIds) const
{
    if (length <= wy3d::TOL) return nullptr;

    const double tol = std::fabs(tolPx * valuePerPixel);
    Snap2dEqualLengthSPtr pRet(nullptr);
    double minDelta = DBL_MAX;

    for (auto iter = _snapRefElements.crbegin(); iter != _snapRefElements.crend(); ++iter)
    {
        const wydb::ElementId& id = *iter;
        if (excludeIds.find(id) != excludeIds.cend()) continue;

        const wy3d::SketchCurve* pCurve = this->getSnapRefCurve(pDb, id);
        if (!pCurve) continue;

        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve);
        const wy3d::SketchCenterLine* pCenterLine =
            pLine ? nullptr : wy3d::SketchCenterLine::cast(pCurve);
        if (!pLine && !pCenterLine) continue;

        const wy::Vector2 startPnt =
            pLine ? pLine->getStartPoint() : pCenterLine->getStartPoint();
        const wy::Vector2 endPnt =
            pLine ? pLine->getEndPoint() : pCenterLine->getEndPoint();
        const double refLength = pLine ? pLine->getLength() : pCenterLine->getLength();
        if (refLength <= wy3d::TOL) continue;

        const double refAngle = Snap2dObject::computeParallelAngle(startPnt, endPnt);
        const double angleDelta = angleDistanceModPi(refAngle, angle);
        // 参考线需与绘制方向平行或垂直
        if (angleDelta > wy3d::EPS && std::fabs(angleDelta - wy3d::PI_2) > wy3d::EPS)
        {
            continue;
        }

        const double lengthDelta = std::fabs(refLength - length);
        if (lengthDelta <= tol && lengthDelta < minDelta)
        {
            minDelta = lengthDelta;
            pRet = std::make_shared<Snap2dEqualLength>(id, refLength, refAngle);
        }
    }

    return pRet;
}

Snap2dEqualRadiusSPtr Snap2dSystemImpl::snapEqualRadiusInRefElements(
    const wydb::Database* pDb, double radius, double tolPx,
    double valuePerPixel, const std::set<wydb::ElementId>& excludeIds) const
{
    if (radius <= wy3d::TOL) return nullptr;

    const double tol = std::fabs(tolPx * valuePerPixel);
    Snap2dEqualRadiusSPtr pRet(nullptr);
    double minDelta = DBL_MAX;

    for (auto iter = _snapRefElements.crbegin(); iter != _snapRefElements.crend(); ++iter)
    {
        const wydb::ElementId& id = *iter;
        if (excludeIds.find(id) != excludeIds.cend()) continue;

        const wy3d::SketchCurve* pCurve = this->getSnapRefCurve(pDb, id);
        if (!pCurve) continue;

        double refRadius = 0.0;
        if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve))
        {
            refRadius = pCircle->getRadius();
            if (refRadius <= wy3d::TOL) continue;
        }
        else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve))
        {
            refRadius = pArc->getRadius();
            if (refRadius <= wy3d::TOL) continue;
            if (pArc->getTotalAngle() <= wy3d::TOL) continue;
        }
        else
        {
            continue;
        }

        const double radiusDelta = std::fabs(refRadius - radius);
        if (radiusDelta <= tol && radiusDelta < minDelta)
        {
            minDelta = radiusDelta;
            pRet = std::make_shared<Snap2dEqualRadius>(id, refRadius);
        }
    }

    return pRet;
}

static Snap2dResultSPtr convertSnapResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& retPos,
    const wyap::SnapResultSPtr& pSnapRet3d)
{
    assert(pSnapRet3d);

    const SnapResultPoint* pSnapRetPoint = dynamic_cast<const SnapResultPoint*>(pSnapRet3d.get());
    if (!pSnapRetPoint) return nullptr;
    wyap::SnapObjectSPtr pSnapObj = pSnapRetPoint->getSnapObject();
    if (!pSnapObj) return nullptr;
    SnapPoint* pSnapPoint = dynamic_cast<SnapPoint*>(pSnapObj.get());
    if (!pSnapPoint)
    {
        assert(false);
        return nullptr;
    }
    Snap2dObjectSPtr pSketchSnapObj(nullptr);
    switch (pSnapPoint->getType())
    {
    case SnapPoint::Type::Origin:
        pSketchSnapObj = std::make_shared<Snap2dOrigin>();
        break;

    case SnapPoint::Type::End:
        pSketchSnapObj = std::make_shared<Snap2dEndPoint>(pSnapObj->getId(), retPos);
        break;

    case SnapPoint::Type::Middle:
        pSketchSnapObj = std::make_shared<Snap2dMiddlePoint>(pSnapObj->getId(), retPos);
        break;

    case SnapPoint::Type::Center:
        pSketchSnapObj = std::make_shared<Snap2dCenterPoint>(pSnapObj->getId(), retPos);
        break;

    case SnapPoint::Type::Quadrant:
        pSketchSnapObj = std::make_shared<Snap2dQuadrantPoint>(pSnapObj->getId(), retPos);
        break;

    case SnapPoint::Type::Point:
        pSketchSnapObj = std::make_shared<Snap2dGenericPoint>(retPos);
        break;
    }

    if (pSketchSnapObj)
    {
        return makeSnap2dResult(sketchPlane, retPos, pSketchSnapObj);
    }

    // A hit always yields a result; unknown kinds yield a display-less one.
    return makeSnap2dResult(sketchPlane, retPos);
}

std::pair<Snap2dResultSPtr, wy::Vector2> Snap2dSystemImpl::snap(
    double x, double y, osgViewer::View* pView,
    const std::set<wydb::ElementId>& excludeIds,
    Snap2dContextSPtr pContext)
{
    assert(pContext);

    // 上下文(命令/步骤)切换时清理动态对齐点(必须在全局点捕捉之前)
    this->beginSnapContext(pContext);

    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();

    if (_isSnap3dEnabled)
    {
        wyap::SnapContextSPtr pSnapContext = std::make_shared<LocateContext>();
        wyap::SnapResultSPtr pSnapResult = ApplicationImpl::instance().getSnapSystem()->snap(
            x, y, pSnapContext, excludeIds, false);
        if (pSnapResult)
        {
            const wy::Vector2 snapPos = _sketchPlane.uv(pSnapResult->getPosition());
            const wy::Vector3 planePnt = _sketchPlane.value(snapPos);
            const wy::Vector2 retPos = _sketchPlane.uv(planePnt);
            Snap2dResultSPtr pSketchSnapRet = convertSnapResult(_sketchPlane, retPos, pSnapResult);
            this->appendAlignRefPoints(pSketchSnapRet);
            this->appendSnapRefElements(pSketchSnapRet, pDb, excludeIds);
            this->setSnapResult(pSketchSnapRet);
            return std::make_pair(pSketchSnapRet, retPos);
        }
    }

    assert(pView);
    const wy::Vector2 rawPos = _sketchPlane.uv(MathUtils::projectToPlane(pView, x, y, _sketchPlane));
    Snap2dResultSPtr pSnapResult = this->snapImpl(x, y, pView, rawPos, excludeIds, pContext);
    this->appendAlignRefPoints(pSnapResult);
    this->appendSnapRefElements(pSnapResult, pDb, excludeIds);
    this->setSnapResult(pSnapResult);
    return std::make_pair(pSnapResult, rawPos);
}

Snap2dResultSPtr Snap2dSystemImpl::snapIndexOnly(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
    Snap2dContextSPtr pContext)
{
    assert(pContext);
    this->beginSnapContext(pContext);
    Snap2dResultSPtr pSnapResult = this->snapImpl(x, y, pView, pos, excludeIds, pContext);
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    this->appendAlignRefPoints(pSnapResult);
    this->appendSnapRefElements(pSnapResult, pDb, excludeIds);
    this->setSnapResult(pSnapResult);
    return pSnapResult;
}

void Snap2dSystemImpl::setSnapResult(Snap2dResultSPtr pSnapResult)
{
    _pSnapResult = std::move(pSnapResult);
    _pResultDisplay->apply(_pSnapResult.get());
}

Snap2dResultSPtr Snap2dSystemImpl::snapImpl(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
    std::shared_ptr<Snap2dContext> pContext)
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) { assert(false); return nullptr; }

    // Apply pending database changes first: a full rebuild resets the snap context.
    if (_isPartiallyUpdatePending) this->partiallyUpdate(pDb);

    // Ensure the context is established (no-op when unchanged).
    this->beginSnapContext(pContext);

    osg::Camera* camera = pView->getCamera();
    assert(camera);
    osg::Matrix VPW = camera->getViewMatrix() 
        * camera->getProjectionMatrix()
        * camera->getViewport()->computeWindowMatrix();
    osg::Vec3d screenPos = MathUtils::toVec3d(
        _sketchPlane.value(pos)) * VPW;
    osg::Vec3d screenPosX = MathUtils::toVec3d(
        _sketchPlane.value(wy::Vector2(pos.x() + 1, pos.y()))) * VPW;
    osg::Vec3d screenPosY = MathUtils::toVec3d(
        _sketchPlane.value(wy::Vector2(pos.x(), pos.y() + 1))) * VPW;
    double xPerPixel(1.0), yPerPixel(1.0); // 屏幕上的1个像素对应草图中的多少
    {
        double pixelsPerSketchUnitX = (screenPosX - screenPos).length();
        if (pixelsPerSketchUnitX > 1e-5) xPerPixel = 1.0 / pixelsPerSketchUnitX;
        else
        {
            assert(false);
            return nullptr;
        }

        double pixelsPerSketchUnitY = (screenPosY - screenPos).length();
        if (pixelsPerSketchUnitY > 1e-5) yPerPixel = 1.0 / pixelsPerSketchUnitY;
        else
        {
            assert(false);
            return nullptr;
        }
    }

    // 点选
    wydb::ElementId pickedId = this->pickElement(pDb, pView, x, y, excludeIds);

    // Rule table dispatch, equivalent to the former two dynamic_cast chains: on an
    // exact context-class match, the picked face runs first (sketch-side rules),
    // then the no-picked face (base rules); the first non-null result wins.
    Snap2dResultSPtr pSnapRet(nullptr);
    if (pContext)
    {
        auto iter = _rules.find(pContext->getClassInfo()->className());
        if (iter != _rules.cend())
        {
            // Picked face
            if (!pickedId.isNull())
            {
                pSnapRet = iter->second->snapPicked(
                    pContext.get(), pDb, pickedId, pos, xPerPixel, yPerPixel, excludeIds);
            }

            // No-picked face
            if (!pSnapRet)
            {
                pSnapRet = iter->second->snapNoPicked(
                    pContext.get(), pDb, pos, xPerPixel, yPerPixel, excludeIds);
            }
        }
    }

    // 悬停拾取到的曲线也入参考池: 覆盖 picked 面无命中(如画圆时切点超出范围)
    // 或结果 hit 不带 id 的情况。放在结果计算之后, 只影响下一帧。
    this->appendSnapRefElement(pickedId, pDb, excludeIds);
    return pSnapRet;
}

std::shared_ptr<Snap2dAngle> Snap2dSystemImpl::snapAngleSnapObject(
    const std::vector<Snap2dAngleSPtr>& angleSnapObjs,
    double angle, double tol)
{
    assert(angle >= 0.0 && angle < wy3d::PI);
    assert(tol > 0.0);
    double delta(0.0);
    for (const std::shared_ptr<Snap2dAngle>& pAngleSnapObj : angleSnapObjs)
    {
        assert(pAngleSnapObj);
        delta = std::fabs(pAngleSnapObj->getValue() - angle);
        assert(pAngleSnapObj->getValue() >= 0.0 && pAngleSnapObj->getValue() < wy3d::PI);
        if (delta <= tol || std::fabs(delta - wy3d::PI) <= tol)
        {
            return pAngleSnapObj;
        }
    }

    return nullptr;
}


// 定位点
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const Locate2dContext* pLocateContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds)
{
    assert(pLocateContext);
    assert(_pSnapContext);
    assert(pLocateContext == _pSnapContext.get());
    assert(pDb);

    // 对齐X
    Snap2dAlignXSPtr pAlignXSnapObj = this->snapAlignRefX(pos.x(), 10, valuePerPixelX, excludeIds);
    // 对齐Y
    Snap2dAlignYSPtr pAlignYSnapObj = this->snapAlignRefY(pos.y(), 10, valuePerPixelY, excludeIds);
    // 结果
    if (pAlignXSnapObj && pAlignYSnapObj)
    {
        return makeSnap2dResult(_sketchPlane,
            wy::Vector2(pAlignXSnapObj->getValue(), pAlignYSnapObj->getValue()), pAlignXSnapObj, pAlignYSnapObj);
    }
    else if (pAlignXSnapObj)
    {
        return makeSnap2dResult(_sketchPlane,
            wy::Vector2(pAlignXSnapObj->getValue(), pos.y()), pAlignXSnapObj);
    }
    else if (pAlignYSnapObj)
    {
        return makeSnap2dResult(_sketchPlane,
            wy::Vector2(pos.x(), pAlignYSnapObj->getValue()), pAlignYSnapObj);
    }

    return nullptr;
}

// 绘制直线
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const DrawLine2dContext* pDrawLineContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds)
{
    assert(pDrawLineContext);
    assert(_pSnapContext);
    assert(pDrawLineContext == _pSnapContext.get());
    assert(pDb);

    wy::Vector2 drawStartPnt = pDrawLineContext->getStartPoint();
    wy::Vector2 drawVec = pos - drawStartPnt;
    double drawLength = drawVec.length();
    if (drawLength <= wy3d::TOL)
    {
        return nullptr; // 绘制的直线段太短不捕捉
    }

    // 算出捕捉角度
    static double radian10 = wy3d::degreesToRadians(10.0);
    double angleTol = std::atan2(15 * (valuePerPixelX + valuePerPixelY) / 2, drawLength); // 15像素
    if (angleTol > radian10)
    {
        angleTol = radian10;
    }

    // 角度[0, PI)
    double drawAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, drawVec); // [0, 2PI)
    assert(drawAngle >= 0.0 && drawAngle < wy3d::TWO_PI);
    if (drawAngle >= wy3d::PI) drawAngle -= wy3d::PI;
    assert(drawAngle >= 0.0 && drawAngle < wy3d::PI);

    // 捕捉到的角度对象(包含相切)
    struct SnappedAngleObject
    {
        Snap2dObjectSPtr pSnapObject;
        double angle;
        wy::Vector2 dir;
        wy::Vector2 retPos;
        SnappedAngleObject() : pSnapObject(nullptr), angle(0.0), dir(1.0, 0.0), retPos(0.0, 0.0) {}
    };
    std::shared_ptr<SnappedAngleObject> pSnappedAngleObject(nullptr);

    // 捕捉起点切线
    const DrawLineTangent2dContext* pTangentContext =
        dynamic_cast<const DrawLineTangent2dContext*>(pDrawLineContext);
    const SnapStartTangent* pSnapStartTangent =
        pTangentContext ? &pTangentContext->getStartTangent() : nullptr;
    if (pSnapStartTangent && !pSnapStartTangent->id.isNull() && pSnapStartTangent->direction.length() > 0.5)
    {
        // id
        wydb::ElementId startElemId = pSnapStartTangent->id;

        // 切线角度
        const wy::Vector2& startTangentDir = pSnapStartTangent->direction;
        double startTangentAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, startTangentDir);
        assert(startTangentAngle >= 0.0 && startTangentAngle < wy3d::TWO_PI);
        if (startTangentAngle >= wy3d::PI) startTangentAngle -= wy3d::PI;
        assert(startTangentAngle >= 0.0 && startTangentAngle < wy3d::PI);

        // 捕捉到切线
        double delta = std::fabs(startTangentAngle - drawAngle);
        if (delta <= angleTol || std::fabs(delta - wy3d::PI) <= angleTol)
        {
            // 捕捉位置
            double projectLen = drawVec.dot(startTangentDir);
            wy::Vector2 retPos = drawStartPnt + projectLen * startTangentDir;

            // 切线
            Snap2dTangentPointSPtr pTangentSnapObj = std::make_shared<Snap2dTangentPoint>(
                startElemId, drawStartPnt);
            pSnappedAngleObject = std::make_shared<SnappedAngleObject>();
            pSnappedAngleObject->pSnapObject = pTangentSnapObj;
            pSnappedAngleObject->angle = startTangentAngle;
            pSnappedAngleObject->dir = projectLen < 0.0 ? -startTangentDir : startTangentDir;
            pSnappedAngleObject->retPos = retPos;
        }
    }

    // 捕捉角度
    if (!pSnappedAngleObject)
    {
        // 1.水平竖直
        Snap2dAngleSPtr pAngleSnapObj = this->snapAngleSnapObject(_defaultAngleSnapObjects, drawAngle, angleTol);
        // 2.平行垂直(参考动态池)
        if (!pAngleSnapObj) pAngleSnapObj = this->snapParallelOrPerpendicular(pDb, drawAngle, angleTol, excludeIds);
        // 捕捉到角度
        if (pAngleSnapObj)
        {
            // 捕捉位置
            double angle = pAngleSnapObj->getValue();
            assert(angle >= 0.0 && angle < wy3d::PI);
            // added by wangyao 2025.04.10 {
            // 水平和竖直时直接设置dir为(0,1)or(1,0)而不是通过角度计算是为了避免误差
            wy::Vector2 dir;
            if (pAngleSnapObj->getType() == Snap2dType::Horizontal)
                dir.set(1.0, 0.0);
            else if (pAngleSnapObj->getType() == Snap2dType::Vertical)
                dir.set(0.0, 1.0);
            else
                dir.set(std::cos(angle), std::sin(angle));
            // }
            double projectLen = drawVec.dot(dir);
            if (projectLen < 0.0)
            {
                projectLen = -projectLen;
                dir = -dir;
            }
            wy::Vector2 retPos = drawStartPnt + projectLen * dir;

            pSnappedAngleObject = std::make_shared<SnappedAngleObject>();
            pSnappedAngleObject->pSnapObject = pAngleSnapObj;
            pSnappedAngleObject->angle = angle;
            pSnappedAngleObject->dir = dir;
            pSnappedAngleObject->retPos = retPos;
        }
    }

    // 捕捉角度
    if (pSnappedAngleObject && pSnappedAngleObject->pSnapObject)
    {
        const double& angle = pSnappedAngleObject->angle;
        const wy::Vector2& dir = pSnappedAngleObject->dir;
        wy::Vector2& retPos = pSnappedAngleObject->retPos;
        const Snap2dObjectSPtr& pAngleSnapObj = pSnappedAngleObject->pSnapObject;

        // 水平
        if (pAngleSnapObj->getType() == Snap2dType::Horizontal || std::fabs(angle) <= wy3d::EPS)
        {
            // 相等
            if (Snap2dEqualLengthSPtr pEqualSnapObj = this->snapEqualLengthInRefElements(
                pDb, drawLength, 10, std::max(valuePerPixelX, valuePerPixelY), 0.0, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getLength() * dir;
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (Snap2dAlignXSPtr pAlignXSnapObj = this->snapAlignRefX(
                pos.x(), 10, valuePerPixelX, excludeIds))
            {
                retPos.setX(pAlignXSnapObj->getValue());
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pAlignXSnapObj);
            }
        }
        // 竖直
        else if (pAngleSnapObj->getType() == Snap2dType::Vertical || std::fabs(angle - wy3d::PI_2) <= wy3d::EPS)
        {
            // 相等
            if (Snap2dEqualLengthSPtr pEqualSnapObj = this->snapEqualLengthInRefElements(
                pDb, drawLength, 10, std::max(valuePerPixelX, valuePerPixelY), 0.0, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getLength() * dir;
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐Y
            else if (Snap2dAlignYSPtr pAlignYSnapObj = this->snapAlignRefY(
                pos.y(), 10, valuePerPixelY, excludeIds))
            {
                retPos.setY(pAlignYSnapObj->getValue());
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pAlignYSnapObj);
            }
        }
        // 其它角度
        else
        {
            // 相等
            if (Snap2dEqualLengthSPtr pEqualSnapObj = this->snapEqualLengthInRefElements(
                pDb, drawLength, 10, std::max(valuePerPixelX, valuePerPixelY), angle, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getLength() * dir;
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (Snap2dAlignXSPtr pAlignXSnapObj = this->snapAlignRefX(
                pos.x(), 10, valuePerPixelX, excludeIds))
            {
                if (std::fabs(std::cos(angle)) > wy3d::EPS)
                {
                    // (y - startY) / (x - startX) = sin(t) / cos(t) 
                    // ==> y = (x - startX) * sin(t) / cos(t) + startY
                    double x = pAlignXSnapObj->getValue();
                    double y = (x - drawStartPnt.x()) * std::sin(angle) / std::cos(angle) + drawStartPnt.y();
                    retPos.set(x, y);
                    return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pAlignXSnapObj);
                }
                else
                {
                    assert(false);
                }
            }
            // 对齐Y
            else if (Snap2dAlignYSPtr pAlignYSnapObj = this->snapAlignRefY(
                pos.y(), 10, valuePerPixelY, excludeIds))
            {
                if (std::fabs(std::sin(angle)) > wy3d::EPS)
                {
                    // (y - startY) / (x - startX) = sin(t) / cos(t)
                    // ==> x = (y - startY) * cos(t) / sin(t) + startX
                    double y = pAlignYSnapObj->getValue();
                    double x = (y - drawStartPnt.y()) * std::cos(angle) / std::sin(angle) + drawStartPnt.x();
                    retPos.set(x, y);
                    return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pAlignYSnapObj);
                }
                else
                {
                    assert(false);
                }
            }
        }

        return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj);
    }
    // 没有捕捉到角度
    else
    {
        // 对齐X
        Snap2dAlignXSPtr pAlignX = this->snapAlignRefX(
            pos.x(), 10, valuePerPixelX, excludeIds);
        // 对齐Y
        Snap2dAlignYSPtr pAlignY = this->snapAlignRefY(
            pos.y(), 10, valuePerPixelY, excludeIds);
        // 结果
        if (pAlignX && pAlignY)
        {
            return makeSnap2dResult(_sketchPlane,
                wy::Vector2(pAlignX->getValue(), pAlignY->getValue()), pAlignX, pAlignY);
        }
        else if (pAlignX)
        {
            return makeSnap2dResult(_sketchPlane,
                wy::Vector2(pAlignX->getValue(), pos.y()), pAlignX);
        }
        else if (pAlignY)
        {
            return makeSnap2dResult(_sketchPlane,
                wy::Vector2(pos.x(), pAlignY->getValue()), pAlignY);
        }
    }

    return nullptr;
}

// 绘制圆
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const DrawCircle2dContext* pDrawCircleContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds)
{
    assert(pDrawCircleContext);
    assert(_pSnapContext);
    assert(pDrawCircleContext == _pSnapContext.get());
    assert(pDb);

    wy::Vector2 drawCenterPnt = pDrawCircleContext->getCenter();
    wy::Vector2 drawVec = pos - drawCenterPnt;
    double drawRadius = drawVec.length();
    if (drawRadius <= wy3d::TOL)
    {
        return nullptr; // 绘制的圆太小则不捕捉
    }

    // 计算与XY坐标轴的切点
    if (_dynSnapObjs.id2PointSnapObjects.find(NULL_ID) == _dynSnapObjs.id2PointSnapObjects.cend())
    {
        // 垂足实际上就是切点
        wy::Vector2 foot;
        if (MathUtils::perpendicularFootOnLine(drawCenterPnt, wy::Vector2::kZero, wy::Vector2::kXAxis, foot))
        {
            Snap2dPointSPtr pSnapObj = std::make_shared<Snap2dTangentPoint>(NULL_ID, foot);
            _dynSnapObjs.id2PointSnapObjects[NULL_ID].emplace_back(std::move(pSnapObj));
        }
        if (MathUtils::perpendicularFootOnLine(drawCenterPnt, wy::Vector2::kZero, wy::Vector2::kYAxis, foot))
        {
            Snap2dPointSPtr pSnapObj = std::make_shared<Snap2dTangentPoint>(NULL_ID, foot);
            _dynSnapObjs.id2PointSnapObjects[NULL_ID].emplace_back(std::move(pSnapObj));
        }
    }

    // 捕捉切点
    Snap2dPointSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[wydb::ElementId::kNull], pos, 10, valuePerPixelX, valuePerPixelY);
    if (pPointSnapRet)
    {
        return makeSnap2dResult(_sketchPlane, pPointSnapRet->getValue(), pPointSnapRet);
    }

    // 相等(参考动态池)
    Snap2dEqualRadiusSPtr pEqualSnapObj = this->snapEqualRadiusInRefElements(
        pDb, drawRadius, 10, std::max(valuePerPixelX, valuePerPixelY), excludeIds);


    // 动态对齐点:与圆心对齐
    if (_dynSnapObjs.alignXs.empty())
    {
        _dynSnapObjs.alignXs.emplace_back(MOVE(MKSPTR<Snap2dAlignX>(NULL_ID, drawCenterPnt)));
    }
    if (_dynSnapObjs.alignYs.empty())
    {
        _dynSnapObjs.alignYs.emplace_back(MOVE(MKSPTR<Snap2dAlignY>(NULL_ID, drawCenterPnt)));
    }

    // 对齐X
    if (Snap2dAlignXSPtr pAlignXSnapObj = this->snapSingleValueInContainer<Snap2dAlignXSPtr, std::vector>(
        _dynSnapObjs.alignXs, pos.x(), 10.0, valuePerPixelX, excludeIds))
    {
        if (pEqualSnapObj) // 捕捉到相等
        {
            double y = pos.y();
            if (drawVec.dot(wy::Vector2::kYAxis) > 0.0) y = drawCenterPnt.y() + pEqualSnapObj->getRadius();
            else y = drawCenterPnt.y() - pEqualSnapObj->getRadius();
            wy::Vector2 retPos(pAlignXSnapObj->getValue(), y);
            return makeSnap2dResult(_sketchPlane, retPos, pAlignXSnapObj, pEqualSnapObj);
        }
        else
        {
            wy::Vector2 retPos(pAlignXSnapObj->getValue(), pos.y());
            return makeSnap2dResult(_sketchPlane, retPos, pAlignXSnapObj);
        }
    }

    // 对齐Y
    if (Snap2dAlignYSPtr pAlignYSnapObj = this->snapSingleValueInContainer<Snap2dAlignYSPtr, std::vector>(
        _dynSnapObjs.alignYs, pos.y(), 10.0, valuePerPixelY, excludeIds))
    {
        if (pEqualSnapObj) // 捕捉到相等
        {
            double x = pos.x();
            if (drawVec.dot(wy::Vector2::kXAxis) > 0.0) x = drawCenterPnt.x() + pEqualSnapObj->getRadius();
            else x = drawCenterPnt.x() - pEqualSnapObj->getRadius();
            wy::Vector2 retPos(x, pAlignYSnapObj->getValue());
            return makeSnap2dResult(_sketchPlane, retPos, pAlignYSnapObj, pEqualSnapObj);
        }
        else
        {
            wy::Vector2 retPos(pos.x(), pAlignYSnapObj->getValue());
            return makeSnap2dResult(_sketchPlane, retPos, pAlignYSnapObj);
        }
    }
    
    // 仅仅捕捉到相等
    if (pEqualSnapObj)
    {
        drawVec.normalize();
        wy::Vector2 retPos = drawCenterPnt + pEqualSnapObj->getRadius() * drawVec;
        return makeSnap2dResult(_sketchPlane, retPos, pEqualSnapObj);
    }

    return nullptr;
}

// 绘制圆弧
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const DrawArc2dContext* pDrawArcContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds)
{
    assert(pDrawArcContext);
    assert(_pSnapContext);
    assert(pDrawArcContext == _pSnapContext.get());
    assert(pDb);
    wy::Vector2 center = pDrawArcContext->getCenter();
    double radius = pDrawArcContext->getRadius();
    if (radius <= wy3d::TOL)
    {
        return nullptr; // 绘制的圆弧半径太小则不捕捉
    }
    wy::Vector2 startPnt = pDrawArcContext->getStartPoint();

    // 计算与起点对齐的两个弧上点
    if (_dynSnapObjs.id2PointSnapObjects.find(NULL_ID) == _dynSnapObjs.id2PointSnapObjects.cend())
    {
        std::list<Snap2dPointSPtr>& pointSnapObjs = _dynSnapObjs.id2PointSnapObjects[NULL_ID];

        // 与起点x相同的点
        wy::Vector2 alignXPnt(startPnt.x(), center.y() - (startPnt.y() - center.y()));
        if ((alignXPnt - startPnt).length() > wy3d::TOL)
            pointSnapObjs.emplace_back(MOVE(MKSPTR<Snap2dAlignPoint>(NULL_ID, alignXPnt, startPnt, Snap2dType::AlignX)));
        // 与起点y相同的点
        wy::Vector2 alignYPnt(center.x() - (startPnt.x() - center.x()), startPnt.y());
        if ((alignYPnt - startPnt).length() > wy3d::TOL)
            pointSnapObjs.emplace_back(MOVE(MKSPTR<Snap2dAlignPoint>(NULL_ID, alignYPnt, startPnt, Snap2dType::AlignY)));
    }

    // 捕捉交点
    if (Snap2dPointSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[NULL_ID], pos, 10, valuePerPixelX, valuePerPixelY))
    {
        return makeSnap2dResult(_sketchPlane, pPointSnapRet->getValue(), pPointSnapRet);
    }

    return nullptr;
}

void Snap2dSystemImpl::onDatabaseChanged(
    const wydb::Database* pDatabase,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        _modifiedIds.insert(id);
    }

    for (const wydb::ElementId& id : changeInfo.addedIds)
    {
        // 删除后恢复: 撤销墓碑, 否则引用该元素的动态数据会被误剔除
        _erasedIds.erase(id);
    }

    for (const wydb::ElementId& id : changeInfo.erasedIds)
    {
        _erasedIds.insert(id);
    }

    _isPartiallyUpdatePending = true;
}

} // namespace wy3dApp
