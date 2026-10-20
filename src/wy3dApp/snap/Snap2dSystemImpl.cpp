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

#include <cassert>
#include <cmath>

#include <wyVector2.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>

#include <wy3dApp/Application.h>
#include "utils/MathUtils.h"

namespace wy3dApp {

typedef Snap2dAngleSPtr AngleSPtr;
typedef Snap2dParallelSPtr ParlSPtr;
typedef Snap2dPerpendicularSPtr PerpSPtr;
typedef Snap2dAlignXSPtr AlignXSPtr;
typedef Snap2dAlignYSPtr AlignYSPtr;

#define NULL_ID wydb::ElementId::kNull
#define MKSPTR std::make_shared
#define MKSPTR_HORZ   std::make_shared<Snap2dHorizontal>
#define MKSPTR_VERT   std::make_shared<Snap2dVertical>
#define MKSPTR_AlignX std::make_shared<Snap2dAlignX>
#define MKSPTR_AlignY std::make_shared<Snap2dAlignY>
#define MKSPTR_Parl   std::make_shared<Snap2dParallel>
#define MKSPTR_Perp   std::make_shared<Snap2dPerpendicular>
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
          [this](const wydb::ElementId& id) { return this->createHighlightTransient(id); })),
      _angleSpansCnt(30), _angleSpan(1.0)
{
    // 构造平行垂直捕捉数据
    this->constructParlAndPerpsData();

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

    // 平行
    /*
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(1.0, 1.0)), _parlAndPerps);
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(-1.0, 1.0)), _parlAndPerps);
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(-1.0, -1.0)), _parlAndPerps);
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(1.0, -1.0)), _parlAndPerps);
    */

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

void Snap2dSystemImpl::rebuildAll()
{
    this->reset();
    this->initDefaultSnapObjects();
}

void Snap2dSystemImpl::appendSnapObjects(const wydb::Database* pDb, const wydb::ElementId& id)
{
}

void Snap2dSystemImpl::constructParlAndPerpsData()
{
    // 平行垂直捕捉
    // _angleSpansCnt = 30 <==> 角度跨距为 180 / 30 = 6 degree
    _angleSpan = wy3d::PI / _angleSpansCnt;
    _parlAndPerps.resize(_angleSpansCnt);
    double min(0.0), max(0.0);
    for (size_t i = 0; i < _angleSpansCnt; ++i)
    {
        max = (i + 1) * _angleSpan;
        _parlAndPerps[i].min = min;
        _parlAndPerps[i].max = max;
        min = max;
    }
    _parlAndPerps[_angleSpansCnt - 1].max = wy3d::PI;
    assert(_parlAndPerps.front().min == 0.0);
    assert(_parlAndPerps.back().max == wy3d::PI);
}

void Snap2dSystemImpl::initDefaultSnapObjects()
{
    // 水平和竖直
    _defaultAngleSnapObjects.reserve(5);
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_HORZ()));
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_VERT()));

    // 坐标原点对齐
    wy::Vector2 origin = wy::Vector2::kZero;
    this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(NULL_ID, origin));
    this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(NULL_ID, origin));
}

void Snap2dSystemImpl::reset()
{
    _pSnapContext = nullptr;
    _pSnapResult = nullptr;

    _defaultAngleSnapObjects.clear();
    // moified by wangyao 2025.04.12 {
    // 平行和垂直捕捉的角度区间是划分好的,30个区间分割180度,不能清空,只可以清空每个区间的捕捉对象.
    for (SnapRange<Snap2dAngleSPtr>& snapRange : _parlAndPerps)
    {
        snapRange.objects.clear();
    }
    // }
    _alignXMap.clear();
    _alignYMap.clear();
    _equalLengthMap.clear();
    _equalRadiusMap.clear();
    _dynSnapObjs.clear();

    _addedIds.clear();
    _erasedIds.clear();
    _modifiedIds.clear();
    _ids.clear();
}

bool Snap2dSystemImpl::partiallyUpdate(const wydb::Database* pDb)
{
    assert(pDb);
    if (!pDb)
    {
        return false;
    }

    // 刷新平面
    if (!this->refreshPlane(pDb))
    {
        return false;
    }
    _isPartiallyUpdatePending = false;

    std::set<wydb::ElementId> dirtyIds;

    // 实际修改的元素ID
    for (const wydb::ElementId& id : _modifiedIds)
    {
        // 1.如果一个元素先新增再修改然后删除,则对于捕捉系统而言该元素是ghost,通过该条件过滤.
        // 2.有些被修改的元素是没有捕捉对象的,比如:草图本身;后续也可能会添加一些比如注释文本之类的元素.
        if (_ids.find(id) != _ids.cend())
        {
            dirtyIds.insert(id);
        }
    }

    // 需要添加捕捉对象的元素IDS
    std::set<wydb::ElementId> addIds = _addedIds;
    addIds.insert(dirtyIds.cbegin(), dirtyIds.cend()); // 实际修改的元素会先移除其捕捉对象所以需要添加.

    // 所有失效的元素
    for (const wydb::ElementId& id : _erasedIds)
    {
        // 同前
        if (_ids.find(id) != _ids.cend())
        {
            dirtyIds.insert(id);
            // added by wangyao 2025.04.29 {
            // 删除的元素不应该重新添加捕捉对象到捕捉系统
            // case: 1.新建水平直线段A;
            //       2.新建两条竖直的直线段B和C把A分成三段
            //       3.使用修剪命令先Trim掉中间段,再Trim掉左边;(A被修改然后被删除;新增了D;此时A在dirtyIds中,需要从addIds中移除)
            addIds.erase(id);
            // }
        }
    }

    // 既没有失效的元素也没有新增的元素则直接返回
    if (dirtyIds.empty() && addIds.empty())
    {
        return true;
    }

    // 失效的元素过半则重新生成
    if (dirtyIds.size() > 10 && dirtyIds.size() > _ids.size() / 2) // 失效的元素过多则重生
    {
        this->rebuildAll();
        return true;
    }

    // 增量更新
    for (const wydb::ElementId& dirtyId : dirtyIds)
    {
        _ids.erase(dirtyId);
    }
    _addedIds.clear();
    _modifiedIds.clear();
    _erasedIds.clear();

    // 清空捕捉上下文和捕捉结果
    _pSnapContext = nullptr;
    _pSnapResult = nullptr;

    // 清空动态捕捉对象
    _dynSnapObjs.clear();
   
    // 移除失效的捕捉对象
    if (!dirtyIds.empty())
    {
        // 水平垂直
        for (SnapRange<Snap2dAngleSPtr>& snapRange : _parlAndPerps)
        {
            eraseDirtySnapObjects(snapRange, dirtyIds);
        }
        // 对齐X
        for (auto& kvp : _alignXMap)
        {
            eraseDirtySnapObjects(kvp.second, dirtyIds);
        }
        // 对齐Y
        for (auto& kvp : _alignYMap)
        {
            eraseDirtySnapObjects(kvp.second, dirtyIds);
        }
        // 相等(长度)
        for (auto& kvp : _equalLengthMap)
        {
            eraseDirtySnapObjects(kvp.second, dirtyIds);
        }
        // 相等(半径)
        for (auto& kvp : _equalRadiusMap)
        {
            eraseDirtySnapObjects(kvp.second, dirtyIds);
        }
    }
    
    // 添加捕捉对象
    if (!addIds.empty())
    {
        for (const wydb::ElementId& id : addIds)
        {
            assert(!id.isNull());
            this->appendSnapObjects(pDb, id);
        }
    }

    return true;
}

void Snap2dSystemImpl::appendAngleSnapObject(
    Snap2dAngleSPtr&& pAngleSnapObj,
    std::vector<SnapRange<Snap2dAngleSPtr>>& angleRegions)
{
    assert(pAngleSnapObj);
    double angle = pAngleSnapObj->getValue();
    assert(angle >= 0.0 && angle < wy3d::PI);
    if (angle < 0.0)
    {
        assert(false);
        return;
    }
    size_t idx = angle / _angleSpan;
    assert(idx >= 0 && idx < _angleSpansCnt);
    if (idx >= _angleSpansCnt)
    {
        assert(false);
        idx = _angleSpansCnt - 1;
    }
    //assert(angle >= angleRegions[idx].min && angle < angleRegions[idx].max);
    angleRegions[idx].objects.emplace_back(std::move(pAngleSnapObj));
}

template<typename T>
void Snap2dSystemImpl::appendEqualSnapObjectImpl(
    std::map<DoubleMinMax, SnapRange<T>>& map, double value, T&& pSnapObj)
{
    assert(value > 0.0);
    // 向下取整
    // <1>floor(3.2)=3.0; <2>floor(-3.2)=-4.0; <3> floor(3.0)=3.0
    double min = std::floor(value);
    // 向上取整
    // <1>ceil(3.2)=4.0; <2>ceil(-3.2)=-3.0; <3> ceil(3.0)=3.0
    double max = std::ceil(value);
    // 刚好是整数值
    if (value == max)
    {
        assert(min == max);
        max = value + 1;
    }
    map[DoubleMinMax(min, max)].objects.emplace_back(std::move(pSnapObj));
}

void Snap2dSystemImpl::appendEqualSnapObject(Snap2dEqualLengthSPtr&& pSnapObj)
{
    assert(pSnapObj);
    this->appendEqualSnapObjectImpl(_equalLengthMap, pSnapObj->getLength(), std::move(pSnapObj));
}

void Snap2dSystemImpl::appendEqualSnapObject(Snap2dEqualRadiusSPtr&& pSnapObj)
{
    assert(pSnapObj);
    this->appendEqualSnapObjectImpl(_equalRadiusMap, pSnapObj->getRadius(), std::move(pSnapObj));
}

Snap2dResultSPtr Snap2dSystemImpl::snap(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
    Snap2dContextSPtr pContext)
{
    assert(pContext);
    Snap2dResultSPtr pSnapResult = this->snapImpl(x, y, pView, pos, excludeIds, pContext);
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
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) { assert(false); return nullptr; }

    // partiallyUpdate clears the snap context, so apply pending changes first.
    if (_isPartiallyUpdatePending) this->partiallyUpdate(pDb);

    if (_pSnapContext != pContext)
    {
        _pSnapContext = pContext;
        _dynSnapObjs.clear();
    }

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
    if (pContext)
    {
        auto iter = _rules.find(pContext->getClassInfo()->className());
        if (iter != _rules.cend())
        {
            // Picked face
            if (!pickedId.isNull())
            {
                Snap2dResultSPtr pSnapRet = iter->second->snapPicked(
                    pContext.get(), pDb, pickedId, pos, xPerPixel, yPerPixel, excludeIds);
                if (pSnapRet) return pSnapRet;
            }

            // No-picked face
            Snap2dResultSPtr pSnapRet = iter->second->snapNoPicked(
                pContext.get(), pDb, pos, xPerPixel, yPerPixel, excludeIds);
            if (pSnapRet) return pSnapRet;
        }
    }

    return nullptr;
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

std::shared_ptr<Snap2dAngle> Snap2dSystemImpl::snapAngleSnapObject(
    const std::vector<SnapRange<Snap2dAngleSPtr>>& angleRegions,
    double angle, double tol, const std::set<wydb::ElementId>& excludeIds)
{
    assert(angle >= 0.0 && angle < wy3d::PI);
    assert(tol > 0.0);

    // 确定需要搜索的区域序号
    size_t idx = angle / _angleSpan;
    size_t idx2nd(-1);
    if (idx >= _angleSpansCnt)
    {
        idx = _angleSpansCnt - 1;
    }
    else
    {
        double angleMod = std::fmod(angle, _angleSpan);
        assert(angleMod >= 0.0);
        if (angleMod < tol)
        {
            if (idx > 0) idx2nd = idx - 1;
        }
        else if (angleMod + tol >= _angleSpan)
        {
            if (idx < _angleSpansCnt - 1) idx2nd = idx + 1;
        }
    }

    double minDelta = DBL_MAX;
    Snap2dAngleSPtr pSnapResult;
    auto snapInList = [angle, tol, &excludeIds, &minDelta, &pSnapResult](
        const std::vector<SnapRange<Snap2dAngleSPtr>>& angleRegions,
        size_t idx)
    {
        for (const Snap2dAngleSPtr& pAngleSnapObj : angleRegions[idx].objects)
        {
            assert(pAngleSnapObj);
            if (excludeIds.find(pAngleSnapObj->getId()) != excludeIds.cend()) continue;
            double delta = std::fabs(pAngleSnapObj->getValue() - angle);
            if (delta > tol) continue;
            if (delta < minDelta)
            {
                pSnapResult = pAngleSnapObj;
                minDelta = delta;
            }
        }
    };

    snapInList(angleRegions, idx);
    if (idx2nd != -1) snapInList(angleRegions, idx2nd);
    return pSnapResult;
}

template<typename T, typename GetValue, typename Accept>
T Snap2dSystemImpl::snapEqualInMapImpl(
    const std::map<DoubleMinMax, SnapRange<T>>& map,
    double value, double tol, double valuePerPixel,
    GetValue getValue, const Accept& accept,
    const std::set<wydb::ElementId>& excludeIds)
{
    assert(value > 0.0);
    assert(tol > 0.0);
    assert(valuePerPixel > 0.0);

    // 获取值的范围
    double min = std::floor(value);
    double max = std::ceil(value);
    if (value == max)
    {
        assert(min == max);
        max = min + 1;
    }

    // 草图坐标系下的捕捉精度
    tol = std::fabs(tol * valuePerPixel); // transform tol from pixel to sketch
    assert(tol >= 0.0);

    // 捕捉具体实现lambda
    auto snapInList = [value, tol, getValue, accept, this, &excludeIds]
        (const std::list<T>& objects) -> T
    {
        for (const T& pSnapObj : objects)
        {
            assert(pSnapObj);
            if (excludeIds.find(pSnapObj->getId()) != excludeIds.cend()) continue;
            if (this->_erasedIds.find(pSnapObj->getId()) != this->_erasedIds.cend()) continue;
            if (!accept(*pSnapObj)) continue;
            if (std::fabs(getValue(*pSnapObj) - value) <= tol)
            {
                return pSnapObj;
            }
        }

        return nullptr;
    };

    // 搜索
    size_t num = static_cast<size_t>(std::ceil(tol)); // 向上取整
    DoubleMinMax mmMin(min - num, max - num);
    DoubleMinMax mmMax(min + num, max + num);
    DoubleMinMax mm(min, max);
    auto iterMiddle = map.find(mm);
    auto iterLeft = StlUtil::findFirstLTE(map, DoubleMinMax(min, max));
    if (iterMiddle != map.cend() && iterLeft == iterMiddle)
    {
        if (iterLeft == map.cbegin()) iterLeft = map.cend(); // 对begin()迭代器递减是未定义行为
        else --iterLeft;
    }
    auto iterRight = StlUtil::findFirstGTE(map, DoubleMinMax(min, max));
    if (iterMiddle != map.cend() && iterRight == iterMiddle)
    {
        ++iterRight;
    }

    // 中间捕捉结果
    T pSnapMiddle = (iterMiddle != map.cend()) ?
        snapInList(iterMiddle->second.objects) : nullptr;
    if (num == 0) return pSnapMiddle;

    // 左边捕捉结果
    T pSnapLeft(nullptr);
    if (pSnapMiddle)
    {
        assert(iterMiddle != map.cend());
        assert(iterMiddle != iterLeft);
        if (iterLeft != map.cend() && iterLeft->first >= mmMin)
        {
            if (iterLeft->first.max == iterMiddle->first.min)
            {
                pSnapLeft = snapInList(iterLeft->second.objects);
            }
        }
    }
    else
    {
        while (true)
        {
            if (iterLeft != map.cend() && iterLeft->first >= mmMin)
            {
                pSnapLeft = snapInList(iterLeft->second.objects);
                if (pSnapLeft) break;

                if (iterLeft == map.cbegin())
                    iterLeft = map.cend(); // 对beign()迭代器递减是未定义行为
                else 
                    --iterLeft;
            }
            else
            {
                break;
            }
        }
    }

    // 右边捕捉结果
    T pSnapRight(nullptr);
    if (pSnapMiddle)
    {
        assert(iterMiddle != map.cend());
        assert(iterMiddle != iterRight);
        if (iterRight != map.cend() && iterRight->first <= mmMax)
        {
            if (iterRight->first.min == iterMiddle->first.max)
            {
                pSnapRight = snapInList(iterRight->second.objects);
            }
        }
    }
    else
    {
        while (true)
        {
            if (iterRight != map.cend() && iterRight->first <= mmMax)
            {
                pSnapRight = snapInList(iterRight->second.objects);
                if (pSnapRight) break;
                ++iterRight;
            }
            else
            {
                break;
            }
        }
    }
    
    // 比较结果取最小值
    double deltaMin(DBL_MAX);
    T pRet(nullptr);
    if (pSnapMiddle)
    {
        double dis = std::fabs(getValue(*pSnapMiddle) - value);
        if (dis < deltaMin)
        {
            deltaMin = dis;
            pRet = pSnapMiddle;
        }
    }
    if (pSnapLeft)
    {
        double dis = std::fabs(getValue(*pSnapLeft) - value);
        if (dis < deltaMin)
        {
            deltaMin = dis;
            pRet = pSnapLeft;
        }
    }
    if (pSnapRight)
    {
        double dis = std::fabs(getValue(*pSnapRight) - value);
        if (dis < deltaMin)
        {
            deltaMin = dis;
            pRet = pSnapRight;
        }
    }

    return pRet;
}

Snap2dEqualLengthSPtr Snap2dSystemImpl::snapEqualLengthInMap(
    double length, double tol, double valuePerPixel, double angle,
    const std::set<wydb::ElementId>& excludeIds)
{
    return this->snapEqualInMapImpl(_equalLengthMap, length, tol, valuePerPixel,
        [](const Snap2dEqualLength& snapObj) { return snapObj.getLength(); },
        [angle](const Snap2dEqualLength& snapObj)
        {
            // 对于直线段的相等捕捉还需要判断是否是平行或垂直
            double delta = std::fabs(angle - snapObj.getLineAngle());
            return delta <= wy3d::EPS || std::fabs(delta - wy3d::PI_2) <= wy3d::EPS;
        },
        excludeIds);
}

Snap2dEqualRadiusSPtr Snap2dSystemImpl::snapEqualRadiusInMap(
    double radius, double tol, double valuePerPixel,
    const std::set<wydb::ElementId>& excludeIds)
{
    return this->snapEqualInMapImpl(_equalRadiusMap, radius, tol, valuePerPixel,
        [](const Snap2dEqualRadius& snapObj) { return snapObj.getRadius(); },
        [](const Snap2dEqualRadius&) { return true; },
        excludeIds);
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
    Snap2dAlignXSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<Snap2dAlignXSPtr>(_alignXMap, pos.x(), 10, valuePerPixelX, excludeIds);
    // 对齐Y
    Snap2dAlignYSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<Snap2dAlignYSPtr>(_alignYMap, pos.y(), 10, valuePerPixelY, excludeIds);
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
        // 2.平行垂直
        if (!pAngleSnapObj) pAngleSnapObj = this->snapAngleSnapObject(_parlAndPerps, drawAngle, angleTol, excludeIds);
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
            if (Snap2dEqualLengthSPtr pEqualSnapObj = this->snapEqualLengthInMap(drawLength, 10,
                valuePerPixelX > valuePerPixelY ? valuePerPixelX : valuePerPixelY, 0.0, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getLength() * dir;
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (Snap2dAlignXSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<Snap2dAlignXSPtr>(
                _alignXMap, pos.x(), 10, valuePerPixelX, excludeIds))
            {
                retPos.setX(pAlignXSnapObj->getValue());
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pAlignXSnapObj);
            }
        }
        // 竖直
        else if (pAngleSnapObj->getType() == Snap2dType::Vertical || std::fabs(angle - wy3d::PI_2) <= wy3d::EPS)
        {
            // 相等
            if (Snap2dEqualLengthSPtr pEqualSnapObj = this->snapEqualLengthInMap(drawLength, 10,
                valuePerPixelX > valuePerPixelY ? valuePerPixelX : valuePerPixelY, 0.0, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getLength() * dir;
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐Y
            else if (Snap2dAlignYSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<Snap2dAlignYSPtr>(
                _alignYMap, pos.y(), 10, valuePerPixelY, excludeIds))
            {
                retPos.setY(pAlignYSnapObj->getValue());
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pAlignYSnapObj);
            }
        }
        // 其它角度
        else
        {
            // 相等
            if (Snap2dEqualLengthSPtr pEqualSnapObj = this->snapEqualLengthInMap(drawLength, 10,
                valuePerPixelX > valuePerPixelY ? valuePerPixelX : valuePerPixelY, angle, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getLength() * dir;
                return makeSnap2dResult(_sketchPlane, retPos, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (Snap2dAlignXSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<Snap2dAlignXSPtr>(
                _alignXMap, pos.x(), 10, valuePerPixelX, excludeIds))
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
            else if (Snap2dAlignYSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<Snap2dAlignYSPtr>(
                _alignYMap, pos.y(), 10, valuePerPixelY, excludeIds))
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
        Snap2dAlignXSPtr pAlignX = this->snapSingleValueInRangeMap<Snap2dAlignXSPtr>(
            _alignXMap, pos.x(), 10, valuePerPixelX, excludeIds);
        // 对齐Y
        Snap2dAlignYSPtr pAlignY = this->snapSingleValueInRangeMap<Snap2dAlignYSPtr>(
            _alignYMap, pos.y(), 10, valuePerPixelY, excludeIds);
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

    // 相等
    Snap2dEqualRadiusSPtr pEqualSnapObj = this->snapEqualRadiusInMap(drawRadius, 10,
        valuePerPixelX > valuePerPixelY ? valuePerPixelX : valuePerPixelY, excludeIds);


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
        assert(_addedIds.find(id) == _addedIds.cend());
        auto iter = _erasedIds.find(id);
        if (iter != _erasedIds.cend())// 新增删除相互抵消
        {
            _erasedIds.erase(iter);
        }
        else
        {
            _addedIds.insert(id);
        }
    }

    for (const wydb::ElementId& id : changeInfo.erasedIds)
    {
        assert(_erasedIds.find(id) == _erasedIds.cend());
        auto iter = _addedIds.find(id);
        if (iter != _addedIds.cend()) // 新增删除相互抵消
        {
            _addedIds.erase(iter);
        }
        else
        {
            _erasedIds.insert(id);
        }
    }

    _isPartiallyUpdatePending = true;
}

} // namespace wy3dApp
