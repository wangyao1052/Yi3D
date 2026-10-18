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
#include "Snap2dResult.h"

#include <cassert>
#include <cmath>

#include <wyVector2.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <utils/wy3dCurveIntersectionUtil.h>

#include <wy3dApp/Application.h>
#include "utils/MathUtils.h"

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
// own context type. The static_cast in the bodies is safe because the rule table
// keys by the exact context typeid.
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
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>&) override
    {
        return _sys.snapImplPicked(
            static_cast<const TContext*>(pContext),
            pDb, pickedId, pos, scaleScreenX, scaleScreenY);
    }

    Snap2dResultSPtr snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds) override
    {
        return _sys.snapImplNoPicked(
            static_cast<const TContext*>(pContext),
            pDb, pos, scaleScreenX, scaleScreenY, excludeIds);
    }

private:
    Snap2dSystemImpl& _sys;
};

// Adapter wrapping an external rule into the internal protocol: a hit becomes a
// Snap2dResult over the system's plane and current context.
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
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds) override
    {
        Snap2dRuleHit hit;
        if (!_pRule->snapPicked(pContext, pDb, pickedId, pos,
            scaleScreenX, scaleScreenY, excludeIds, hit))
        {
            return nullptr;
        }

        return this->makeResult(hit);
    }

    Snap2dResultSPtr snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds) override
    {
        Snap2dRuleHit hit;
        if (!_pRule->snapNoPicked(pContext, pDb, pos,
            scaleScreenX, scaleScreenY, excludeIds, hit))
        {
            return nullptr;
        }

        return this->makeResult(hit);
    }

private:
    Snap2dResultSPtr makeResult(const Snap2dRuleHit& hit) const
    {
        Snap2dResultInfo info;
        info.type = hit.snapType;
        info.highlightId = hit.highlightId;
        info.point = hit.point;
        info.refPoint = hit.refPoint;

        return std::make_shared<Snap2dResult>(
            _sys._sketchPlane, hit.point, _sys._pSnapContext, info);
    }

private:
    Snap2dSystemImpl& _sys;
    Snap2dRuleSPtr _pRule;
};

Snap2dSystemImpl::Snap2dSystemImpl(const wy3d::Plane& workPlane)
    : _sketchPlane(workPlane), _angleSpansCnt(30), _angleSpan(1.0)
{
    // 构造平行垂直捕捉数据
    this->constructParlAndPerpsData();

    // 初始化默认捕捉对象
    this->initDefaultSnapObjects();

    // Register the built-in rules: one rule per context type, both faces. The
    // picked face lands on the virtual hooks (null for plane systems, overridden
    // by the sketch system).
    _rules[std::type_index(typeid(Locate2dContext))] = std::make_shared<Rule<Locate2dContext>>(*this);
    _rules[std::type_index(typeid(SketchDrawLineContext))] = std::make_shared<Rule<SketchDrawLineContext>>(*this);
    _rules[std::type_index(typeid(DrawCircle2dContext))] = std::make_shared<Rule<DrawCircle2dContext>>(*this);
    _rules[std::type_index(typeid(DrawArc2dContext))] = std::make_shared<Rule<DrawArc2dContext>>(*this);
    _rules[std::type_index(typeid(SketchCurveEndPointContext))] = std::make_shared<Rule<SketchCurveEndPointContext>>(*this);

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

wy::ErrorStatus Snap2dSystemImpl::addRule(Snap2dRuleSPtr pRule)
{
    if (!pRule) return wy::ErrorStatus::InvalidInput;

    const std::type_index key = std::type_index(pRule->getContextType());
    if (_rules.cend() != _rules.find(key)) return wy::ErrorStatus::DuplicateKey;

    _rules[key] = std::make_shared<ExternalRuleAdapter>(*this, pRule);

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
    double scaleScreenX, double scaleScreenY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const SketchDrawLineContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const DrawCircle2dContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const DrawArc2dContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplPicked(
    const SketchCurveEndPointContext* pContext, const wydb::Database* pDb,
    const wydb::ElementId& pickedId,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    return nullptr;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const SketchCurveEndPointContext* pContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds)
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
    for (SnapRange<AngleSnapObjectSPtr>& snapRange : _parlAndPerps)
    {
        snapRange.objects.clear();
    }
    // }
    _alignXMap.clear();
    _alignYMap.clear();
    _equalMap.clear();
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
        for (SnapRange<AngleSnapObjectSPtr>& snapRange : _parlAndPerps)
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
        // 相等
        for (auto& kvp : _equalMap)
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
    AngleSnapObjectSPtr&& pAngleSnapObj,
    std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions)
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
    assert(angle >= angleRegions[idx].min && angle < angleRegions[idx].max);
    angleRegions[idx].objects.emplace_back(std::move(pAngleSnapObj));
}

void Snap2dSystemImpl::appendEqualSnapObject(EqualSnapObjectSPtr&& pSnapObj)
{
    assert(pSnapObj);
    double value = pSnapObj->getValue();
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
    _equalMap[DoubleMinMax(min, max)].objects.emplace_back(std::move(pSnapObj));
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
    if (pSnapResult)
    {
        if (_pSnapResult)
        {
            if (pSnapResult->isLooseEqual(*_pSnapResult))
            {
                _pSnapResult->setPosition(pSnapResult->getPosition());
                _pSnapResult->update();
            }
            else
            {
                std::map<wydb::ElementId, GuiCmdTransientSPtr> highlights =
                    this->createHighlightTransients(*pSnapResult, _pSnapResult->getHighlightTransients());
                _pSnapResult = pSnapResult;
                _pSnapResult->show(std::move(highlights));
            }
        }
        else
        {
            std::map<wydb::ElementId, GuiCmdTransientSPtr> highlights =
                this->createHighlightTransients(*pSnapResult, {});
            _pSnapResult = pSnapResult;
            _pSnapResult->show(std::move(highlights));
        }
    }
    else
    {
        _pSnapResult = nullptr;
    }
}

std::map<wydb::ElementId, GuiCmdTransientSPtr> Snap2dSystemImpl::createHighlightTransients(
    const Snap2dResult& result,
    const std::map<wydb::ElementId, GuiCmdTransientSPtr>& prevCache) const
{
    std::map<wydb::ElementId, GuiCmdTransientSPtr> rets;
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return rets;

    for (const Snap2dResultItem& item : result.getItems())
    {
        const wydb::ElementId& id = item.info.highlightId;
        if (id.isNull() || rets.cend() != rets.find(id))
        {
            continue;
        }

        auto iter = prevCache.find(id);
        if (iter != prevCache.cend())
        {
            assert(iter->second);
            rets[id] = iter->second;
            continue;
        }

        if (GuiCmdTransientSPtr pTransient = this->createHighlightTransient(pDb, id))
        {
            rets[id] = std::move(pTransient);
        }
    }
    return rets;
}

Snap2dResultSPtr Snap2dSystemImpl::snapImpl(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
    std::shared_ptr<Snap2dContext> pContext)
{
    if (_pSnapContext != pContext)
    {
        _pSnapContext = pContext;
        _dynSnapObjs.clear();
    }

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) { assert(false); return nullptr; }

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
    // exact context-type match, the picked face runs first (sketch-side rules),
    // then the no-picked face (base rules); the first non-null result wins.
    if (pContext)
    {
        auto iter = _rules.find(std::type_index(typeid(*pContext)));
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

std::shared_ptr<AngleSnapObject> Snap2dSystemImpl::snapAngleSnapObject(
    const std::vector<AngleSnapObjectSPtr>& angleSnapObjs,
    double angle, double tol)
{
    assert(angle >= 0.0 && angle < wy3d::PI);
    assert(tol > 0.0);
    double delta(0.0);
    for (const std::shared_ptr<AngleSnapObject>& pAngleSnapObj : angleSnapObjs)
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

std::shared_ptr<AngleSnapObject> Snap2dSystemImpl::snapAngleSnapObject(
    const std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions,
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
    AngleSnapObjectSPtr pSnapResult;
    auto snapInList = [angle, tol, &excludeIds, &minDelta, &pSnapResult](
        const std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions,
        size_t idx)
    {
        for (const AngleSnapObjectSPtr& pAngleSnapObj : angleRegions[idx].objects)
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

EqualSnapObjectSPtr Snap2dSystemImpl::snapEqualInMap(
    const std::map<DoubleMinMax, SnapRange<EqualSnapObjectSPtr>>& map,
    double value, double tol, double scScreen2Sketch, short flags, double value2, const std::set<wydb::ElementId>& excludeIds)
{
    assert(value > 0.0);
    assert(tol > 0.0);
    assert(scScreen2Sketch > 0.0);

    // 获取值的范围
    double min = std::floor(value);
    double max = std::ceil(value);
    if (value == max)
    {
        assert(min == max);
        max = min + 1;
    }

    // 草图坐标系下的捕捉精度
    tol = std::fabs(tol * scScreen2Sketch); // transform tol from pixel to sketch
    assert(tol >= 0.0);

    // 捕捉具体实现lambda
    auto snapInList = [value, tol, flags, value2, this, &excludeIds]
        (const std::list<EqualSnapObjectSPtr>& objects) -> EqualSnapObjectSPtr
    {
        for (const EqualSnapObjectSPtr& pSnapObj : objects)
        {
            assert(pSnapObj);
            if (excludeIds.find(pSnapObj->getId()) != excludeIds.cend()) continue;
            if (this->_erasedIds.find(pSnapObj->getId()) != this->_erasedIds.cend()) continue;
            if (!(flags & pSnapObj->getExtra())) continue;
            // added by wangyao 2025.04.11 {
            // 对于直线段的相等捕捉还需要判断是否是平行或垂直
            if (value2 != DBL_MAX)
            {
                double delta = std::fabs(value2 - pSnapObj->getValue2());
                if (!(delta <= wy3d::EPS || std::fabs(delta - wy3d::PI_2) <= wy3d::EPS))
                {
                    continue;
                }
            }
            // }
            if (std::fabs(pSnapObj->getValue() - value) <= tol)
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
    EqualSnapObjectSPtr pSnapMiddle = (iterMiddle != map.cend()) ?
        snapInList(iterMiddle->second.objects) : nullptr;
    if (num == 0) return pSnapMiddle;

    // 左边捕捉结果
    EqualSnapObjectSPtr pSnapLeft(nullptr);
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
    EqualSnapObjectSPtr pSnapRight(nullptr);
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
    EqualSnapObjectSPtr pRet(nullptr);
    if (pSnapMiddle)
    {
        double dis = std::fabs(pSnapMiddle->getValue() - value);
        if (dis < deltaMin)
        {
            deltaMin = dis;
            pRet = pSnapMiddle;
        }
    }
    if (pSnapLeft)
    {
        double dis = std::fabs(pSnapLeft->getValue() - value);
        if (dis < deltaMin)
        {
            deltaMin = dis;
            pRet = pSnapLeft;
        }
    }
    if (pSnapRight)
    {
        double dis = std::fabs(pSnapRight->getValue() - value);
        if (dis < deltaMin)
        {
            deltaMin = dis;
            pRet = pSnapRight;
        }
    }

    return pRet;
}

// 定位点
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const Locate2dContext* pLocateContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds)
{
    assert(pLocateContext);
    assert(_pSnapContext);
    assert(pLocateContext == _pSnapContext.get());
    assert(pDb);

    // 对齐X
    AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(_alignXMap, pos.x(), 10, scaleScreenX, excludeIds);
    // 对齐Y
    AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(_alignYMap, pos.y(), 10, scaleScreenY, excludeIds);
    // 结果
    if (pAlignXSnapObj && pAlignYSnapObj)
    {
        return std::make_shared<Snap2dResult>(_sketchPlane,
            wy::Vector2(pAlignXSnapObj->getValue(), pAlignYSnapObj->getValue()), _pSnapContext, pAlignXSnapObj, pAlignYSnapObj);
    }
    else if (pAlignXSnapObj)
    {
        return std::make_shared<Snap2dResult>(_sketchPlane,
            wy::Vector2(pAlignXSnapObj->getValue(), pos.y()), _pSnapContext, pAlignXSnapObj);
    }
    else if (pAlignYSnapObj)
    {
        return std::make_shared<Snap2dResult>(_sketchPlane,
            wy::Vector2(pos.x(), pAlignYSnapObj->getValue()), _pSnapContext, pAlignYSnapObj);
    }

    return nullptr;
}

// 绘制直线
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds)
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
    double angleTol = std::atan2(15 * (scaleScreenX + scaleScreenY) / 2, drawLength); // 15像素
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
        SketchSnapObjectSPtr pSnapObject;
        double angle;
        wy::Vector2 dir;
        wy::Vector2 retPos;
        SnappedAngleObject() : pSnapObject(nullptr), angle(0.0), dir(1.0, 0.0), retPos(0.0, 0.0) {}
    };
    std::shared_ptr<SnappedAngleObject> pSnappedAngleObject(nullptr);

    // 捕捉起点切线
    std::shared_ptr<SketchDrawLineContext::SnapStartTangent> pSnapStartTangent = pDrawLineContext->getSnapStartTangent();
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
            TangentPointSnapObjectSPtr pTangentSnapObj = std::make_shared<TangentPointSnapObject>(
                startElemId, drawStartPnt, SketchSnapObject::UNDEFINED);
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
        AngleSnapObjectSPtr pAngleSnapObj = this->snapAngleSnapObject(_defaultAngleSnapObjects, drawAngle, angleTol);
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
        const SketchSnapObjectSPtr& pAngleSnapObj = pSnappedAngleObject->pSnapObject;

        // 水平
        if (pAngleSnapObj->getType() == Snap2dType::Horizontal || std::fabs(angle) <= wy3d::EPS)
        {
            // 相等
            if (EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawLength, 10,
                scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY, SketchSnapObject::LINE, 0.0, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getValue() * dir;
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(
                _alignXMap, pos.x(), 10, scaleScreenX, excludeIds))
            {
                retPos.setX(pAlignXSnapObj->getValue());
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignXSnapObj);
            }
        }
        // 竖直
        else if (pAngleSnapObj->getType() == Snap2dType::Vertical || std::fabs(angle - wy3d::PI_2) <= wy3d::EPS)
        {
            // 相等
            if (EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawLength, 10,
                scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY, SketchSnapObject::LINE, 0.0, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getValue() * dir;
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐Y
            else if (AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(
                _alignYMap, pos.y(), 10, scaleScreenY, excludeIds))
            {
                retPos.setY(pAlignYSnapObj->getValue());
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignYSnapObj);
            }
        }
        // 其它角度
        else
        {
            // 相等
            if (EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawLength, 10,
                scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY, SketchSnapObject::LINE, angle, excludeIds))
            {
                retPos = drawStartPnt + pEqualSnapObj->getValue() * dir;
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(
                _alignXMap, pos.x(), 10, scaleScreenX, excludeIds))
            {
                if (std::fabs(std::cos(angle)) > wy3d::EPS)
                {
                    // (y - startY) / (x - startX) = sin(t) / cos(t) 
                    // ==> y = (x - startX) * sin(t) / cos(t) + startY
                    double x = pAlignXSnapObj->getValue();
                    double y = (x - drawStartPnt.x()) * std::sin(angle) / std::cos(angle) + drawStartPnt.y();
                    retPos.set(x, y);
                    return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignXSnapObj);
                }
                else
                {
                    assert(false);
                }
            }
            // 对齐Y
            else if (AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(
                _alignYMap, pos.y(), 10, scaleScreenY, excludeIds))
            {
                if (std::fabs(std::sin(angle)) > wy3d::EPS)
                {
                    // (y - startY) / (x - startX) = sin(t) / cos(t)
                    // ==> x = (y - startY) * cos(t) / sin(t) + startX
                    double y = pAlignYSnapObj->getValue();
                    double x = (y - drawStartPnt.y()) * std::cos(angle) / std::sin(angle) + drawStartPnt.x();
                    retPos.set(x, y);
                    return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignYSnapObj);
                }
                else
                {
                    assert(false);
                }
            }
        }

        return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj);
    }
    // 没有捕捉到角度
    else
    {
        // 对齐X
        AlignXSnapObjectSPtr pAlignX = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(
            _alignXMap, pos.x(), 10, scaleScreenX, excludeIds);
        // 对齐Y
        AlignYSnapObjectSPtr pAlignY = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(
            _alignYMap, pos.y(), 10, scaleScreenY, excludeIds);
        // 结果
        if (pAlignX && pAlignY)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane,
                wy::Vector2(pAlignX->getValue(), pAlignY->getValue()), _pSnapContext, pAlignX, pAlignY);
        }
        else if (pAlignX)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane,
                wy::Vector2(pAlignX->getValue(), pos.y()), _pSnapContext, pAlignX);
        }
        else if (pAlignY)
        {
            return std::make_shared<Snap2dResult>(_sketchPlane,
                wy::Vector2(pos.x(), pAlignY->getValue()), _pSnapContext, pAlignY);
        }
    }

    return nullptr;
}

// 绘制圆
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const DrawCircle2dContext* pDrawCircleContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds)
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
            PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(NULL_ID, foot, SketchSnapObject::X_AXIS_0);
            _dynSnapObjs.id2PointSnapObjects[NULL_ID].emplace_back(std::move(pSnapObj));
        }
        if (MathUtils::perpendicularFootOnLine(drawCenterPnt, wy::Vector2::kZero, wy::Vector2::kYAxis, foot))
        {
            PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(NULL_ID, foot, SketchSnapObject::Y_AXIS_0);
            _dynSnapObjs.id2PointSnapObjects[NULL_ID].emplace_back(std::move(pSnapObj));
        }
    }

    // 捕捉切点
    PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[wydb::ElementId::kNull], pos, 10, scaleScreenX, scaleScreenY);
    if (pPointSnapRet)
    {
        return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
    }

    // 相等
    EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawRadius, 10,
        scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY,
        SketchSnapObject::CIRCLE | SketchSnapObject::ARC,
        DBL_MAX, excludeIds);


    if (pDrawCircleContext->isForDrawArc())
    {
        // 动态对齐点:与圆心对齐
        if (_dynSnapObjs.alignXs.empty())
        {
            _dynSnapObjs.alignXs.emplace_back(MOVE(MKSPTR<AlignXSnapObject>(NULL_ID, drawCenterPnt)));
        }
        if (_dynSnapObjs.alignYs.empty())
        {
            _dynSnapObjs.alignYs.emplace_back(MOVE(MKSPTR<AlignYSnapObject>(NULL_ID, drawCenterPnt)));
        }

        // 对齐X
        if (AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInContainer<AlignXSnapObjectSPtr, std::vector>(
            _dynSnapObjs.alignXs, pos.x(), 10.0, scaleScreenX, excludeIds))
        {
            if (pEqualSnapObj) // 捕捉到相等
            {
                double y = pos.y();
                if (drawVec.dot(wy::Vector2::kYAxis) > 0.0) y = drawCenterPnt.y() + pEqualSnapObj->getValue();
                else y = drawCenterPnt.y() - pEqualSnapObj->getValue();
                wy::Vector2 retPos(pAlignXSnapObj->getValue(), y);
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAlignXSnapObj, pEqualSnapObj);
            }
            else
            {
                wy::Vector2 retPos(pAlignXSnapObj->getValue(), pos.y());
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAlignXSnapObj);
            }
        }
        
        // 对齐Y
        if (AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInContainer<AlignYSnapObjectSPtr, std::vector>(
            _dynSnapObjs.alignYs, pos.y(), 10.0, scaleScreenY, excludeIds))
        {
            if (pEqualSnapObj) // 捕捉到相等
            {
                double x = pos.x();
                if (drawVec.dot(wy::Vector2::kXAxis) > 0.0) x = drawCenterPnt.x() + pEqualSnapObj->getValue();
                else x = drawCenterPnt.x() - pEqualSnapObj->getValue();
                wy::Vector2 retPos(x, pAlignYSnapObj->getValue());
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAlignYSnapObj, pEqualSnapObj);
            }
            else
            {
                wy::Vector2 retPos(pos.x(), pAlignYSnapObj->getValue());
                return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pAlignYSnapObj);
            }
        }
    }
    
    // 仅仅捕捉到相等
    if (pEqualSnapObj)
    {
        drawVec.normalize();
        wy::Vector2 retPos = drawCenterPnt + pEqualSnapObj->getValue() * drawVec;
        return std::make_shared<Snap2dResult>(_sketchPlane, retPos, _pSnapContext, pEqualSnapObj);
    }

    return nullptr;
}

// 绘制圆弧
Snap2dResultSPtr Snap2dSystemImpl::snapImplNoPicked(
    const DrawArc2dContext* pDrawArcContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds)
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

    // 计算关键点
    if (_dynSnapObjs.id2PointSnapObjects.find(NULL_ID) == _dynSnapObjs.id2PointSnapObjects.cend())
    {
        std::list<PointSnapObjectSPtr>& pointSnapObjs = _dynSnapObjs.id2PointSnapObjects[NULL_ID];

        // 与X轴交点
        {
            wy::Vector2 intPnt1, intPnt2;
            unsigned int numIntPnts = wy3d::intersectLineCircle(wy::Vector2::kZero, wy::Vector2::kXAxis,
                center, radius, intPnt1, intPnt2);
            if (1 == numIntPnts)
            {
                PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(NULL_ID, intPnt1, SketchSnapObject::X_AXIS_0);
                pointSnapObjs.emplace_back(std::move(pSnapObj));
            }
            else if (2 == numIntPnts)
            {
                PointSnapObjectSPtr pSnapObj1 = std::make_shared<IntersectionPointSnapObject>(NULL_ID, intPnt1, SketchSnapObject::X_AXIS_0);
                pointSnapObjs.emplace_back(std::move(pSnapObj1));
                PointSnapObjectSPtr pSnapObj2 = std::make_shared<IntersectionPointSnapObject>(NULL_ID, intPnt2, SketchSnapObject::X_AXIS_1);
                pointSnapObjs.emplace_back(std::move(pSnapObj2));
            }
        }
        // 与Y轴交点
        {
            wy::Vector2 intPnt1, intPnt2;
            unsigned int numIntPnts = wy3d::intersectLineCircle(wy::Vector2::kZero, wy::Vector2::kYAxis,
                center, radius, intPnt1, intPnt2);
            if (1 == numIntPnts)
            {
                PointSnapObjectSPtr pSnapObj = std::make_shared<TangentPointSnapObject>(NULL_ID, intPnt1, SketchSnapObject::Y_AXIS_0);
                pointSnapObjs.emplace_back(std::move(pSnapObj));
            }
            else if (2 == numIntPnts)
            {
                PointSnapObjectSPtr pSnapObj1 = std::make_shared<IntersectionPointSnapObject>(NULL_ID, intPnt1, SketchSnapObject::Y_AXIS_0);
                pointSnapObjs.emplace_back(std::move(pSnapObj1));
                PointSnapObjectSPtr pSnapObj2 = std::make_shared<IntersectionPointSnapObject>(NULL_ID, intPnt2, SketchSnapObject::Y_AXIS_1);
                pointSnapObjs.emplace_back(std::move(pSnapObj2));
            }
        }

        // 关键点:四个象限点
        std::list<wy::Vector2> keyPnts {
            wy::Vector2(center.x() + radius, center.y()),
            wy::Vector2(center.x() - radius, center.y()),
            wy::Vector2(center.x(), center.y() + radius),
            wy::Vector2(center.x(), center.y() - radius)
        };
        short keyIndex(0);
        for (const wy::Vector2& keyPnt : keyPnts)
        {
            if ((keyPnt - startPnt).length() <= wy3d::TOL) continue;
            pointSnapObjs.emplace_back(MOVE(MKSPTR<KeyPointSnapObject>(NULL_ID, keyPnt, ++keyIndex, center)));
        }
        // 关键点:两个对称点
        if (std::fabs(startPnt.y() - center.y()) > wy3d::TOL &&
            std::fabs(startPnt.x() - center.x()) > wy3d::TOL) // 不在四个象限点上
        {
            pointSnapObjs.emplace_back(MOVE(MKSPTR<KeyPointSnapObject>(NULL_ID,
                wy::Vector2(startPnt.x(), center.y() - (startPnt.y() - center.y())), ++keyIndex, startPnt)));
            pointSnapObjs.emplace_back(MOVE(MKSPTR<KeyPointSnapObject>(NULL_ID,
                wy::Vector2(center.x() - (startPnt.x() - center.x()), startPnt.y()), ++keyIndex, startPnt)));
            pointSnapObjs.emplace_back(MOVE(MKSPTR<KeyPointSnapObject>(NULL_ID,
                wy::Vector2(center.x() - (startPnt.x() - center.x()), center.y() - (startPnt.y() - center.y())),
                ++keyIndex, startPnt)));
        }
    }

    // 捕捉交点
    if (PointSnapObjectSPtr pPointSnapRet = this->snapPointSnapObject(_dynSnapObjs.id2PointSnapObjects[NULL_ID], pos, 10, scaleScreenX, scaleScreenY))
    {
        return std::make_shared<Snap2dResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
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
}

} // namespace wy3dApp
