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

#ifndef WY3DAPP_SNAP2D_SYSTEM_IMPL_H
#define WY3DAPP_SNAP2D_SYSTEM_IMPL_H

#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <osgViewer/View>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dPlane.h>
#include <wy3dApp/Snap2dContext.h>
#include "SketchSnapObject.h"
#include "utils/StlUtil.h"
#include <wy3dApp/GuiCmdTransient.h>
#include <wy3dApp/Snap2dSystem.h>

namespace wy3dApp {

// Internal snap rule protocol (rich): returns the full snap result. Built-in rules
// are thin adapters delegating to the snap implementations below; external rules
// are wrapped into this protocol by an adapter.
class Snap2dRuleHandler
{
public:
    virtual ~Snap2dRuleHandler() = default;

    // Snap after an element was picked; null when nothing snapped.
    virtual Snap2dResultSPtr snapPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds)
    {
        return nullptr;
    }

    // Snap with no picked element; null when nothing snapped.
    virtual Snap2dResultSPtr snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds)
    {
        return nullptr;
    }
};

class Snap2dSystemImpl : public Snap2dSystem, public wydb::DatabaseReactor
{
public:
    explicit Snap2dSystemImpl(const wy3d::Plane& workPlane);
    virtual ~Snap2dSystemImpl();
    Snap2dSystemImpl(const Snap2dSystemImpl&) = delete;
    Snap2dSystemImpl& operator=(const Snap2dSystemImpl&) = delete;
    Snap2dSystemImpl(Snap2dSystemImpl&&) = delete;
    Snap2dSystemImpl& operator=(Snap2dSystemImpl&&) = delete;

    // 局部更新
    bool partiallyUpdate(const wydb::Database* pDb) override;

    // 捕捉
    Snap2dResultSPtr snap(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
        Snap2dContextSPtr pContext) override;

    // 获取捕捉结果
    Snap2dResultSPtr getSnapResult() const override { return _pSnapResult; }
    // 设置捕捉结果
    void setSnapResult(Snap2dResultSPtr pSnapResult) override;
    // 清空捕捉结果
    void clearSnapResult() override { _pSnapResult = nullptr; }

    // 注册外部规则
    wy::ErrorStatus addRule(Snap2dRuleSPtr pRule) override;

    // 数据库变更响应
    virtual void onDatabaseChanged(
        const wydb::Database* pDatabase,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

private:
    // 捕捉对象区域
    template <typename T>
    class SnapRange
    {
    public:
        SnapRange() : min(0.0), max(1.0) {}

        double min; // >=
        double max; // <
        std::list<T> objects;
    };

    // 浮点范围值
    struct DoubleMinMax
    {
        double min;
        double max;

        DoubleMinMax(double argMin, double argMax) : min(argMin), max(argMax) {}

        bool operator<(const DoubleMinMax& other) const
        {
            if (min < other.min) return true;
            else if (min > other.min) return false;
            else return max < other.max;
        }

        bool operator>(const DoubleMinMax& other) const
        {
            return other < *this;
        }

        bool operator>=(const DoubleMinMax& other) const
        {
            return !((*this) < other);
        }

        bool operator<=(const DoubleMinMax& other) const
        {
            return !((*this) > other);
        }
    };

protected:
    // 拾取元素(默认不拾取)
    virtual wydb::ElementId pickElement(
        const wydb::Database* pDb, osgViewer::View* pView,
        double x, double y, const std::set<wydb::ElementId>& excludeIds);
    // 刷新平面(默认不刷新)
    virtual bool refreshPlane(const wydb::Database* pDb);
    // 重建捕捉对象(默认重置并恢复默认对象)
    virtual void rebuildAll();
    // 添加捕捉对象(默认无)
    virtual void appendSnapObjects(const wydb::Database* pDb, const wydb::ElementId& id);
    // Element highlight transient (none by default: plane users have no element highlight)
    virtual GuiCmdTransientSPtr createHighlightTransient(
        const wydb::Database* pDb, const wydb::ElementId& id) const
    {
        return nullptr;
    }

    // Picked-side snap implementations (nullptr by default: only sketch systems
    // snap picked elements)
    virtual Snap2dResultSPtr snapImplPicked(
        const Locate2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);
    virtual Snap2dResultSPtr snapImplPicked(
        const SketchDrawLineContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);
    virtual Snap2dResultSPtr snapImplPicked(
        const DrawCircle2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);
    virtual Snap2dResultSPtr snapImplPicked(
        const DrawArc2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);
    virtual Snap2dResultSPtr snapImplPicked(
        const SketchCurveEndPointContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 构造平行&垂直数据
    void constructParlAndPerpsData();

    // 初始化默认捕捉对象
    void initDefaultSnapObjects();

    // 重置
    void reset();

    // 添加角度捕捉对象
    void appendAngleSnapObject(
        AngleSnapObjectSPtr&& pAngleSnapObj,
        std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions);

    // 添加XY捕捉对象
    template<typename T>
    void appendAlignSnapObject(
        std::map<DoubleMinMax, SnapRange<T>>& map,
        T&& pSnapObj)
    {
        assert(pSnapObj);
        double value = pSnapObj->getValue();
        double min = std::floor(value);
        double max = std::ceil(value);
        if (value == max)
        {
            assert(min == max);
            max = value + 1;
        }
        map[DoubleMinMax(min, max)].objects.push_back(std::move(pSnapObj));
    }

    // 添加相等捕捉对象
    void appendEqualSnapObject(EqualSnapObjectSPtr&& pSnapObj);

    // 移除失效的捕捉对象
    template <typename T>
    void eraseDirtySnapObjects(SnapRange<T>& snapRange, const std::set<wydb::ElementId>& dirtyIds)
    {
        for (auto iter = snapRange.objects.cbegin(); iter != snapRange.objects.cend(); )
        {
            assert(*iter);
            if (dirtyIds.find((*iter)->getId()) != dirtyIds.cend())
            {
                iter = snapRange.objects.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    // 捕捉点
    PointSnapObjectSPtr snapPointSnapObject(
        const std::list<PointSnapObjectSPtr>& snapObjs,
        const wy::Vector2& pos, double tol,
        double scaleScreenX, double scaleScreenY)
    {
        assert(tol > 0.0);
        assert(scaleScreenX > 0.0);
        assert(scaleScreenY > 0.0);

        double tolX = std::fabs(tol * scaleScreenX); // transform tol from pixel to sketch
        double tolY = std::fabs(tol * scaleScreenY); // transform tol from pixel to sketch
        for (const PointSnapObjectSPtr& pSnapObj : snapObjs)
        {
            assert(pSnapObj);
            wy::Vector2 value = pSnapObj->getValue();
            if (std::fabs(value.x() - pos.x()) <= tolX && std::fabs(value.y() - pos.y()) <= tolY)
            {
                return pSnapObj;
            }
        }

        return nullptr;
    }

private:
    // Built-in rule (defined in the .cpp; both faces delegate to this class).
    template <typename TContext> class Rule;

    // Adapter wrapping an external rule into the internal protocol.
    class ExternalRuleAdapter;

    // 捕捉具体实现
    Snap2dResultSPtr snapImpl(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<Snap2dContext> pContext);

    // 定位点
    Snap2dResultSPtr snapImplNoPicked(
        const Locate2dContext* pLocateContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds);

    // 绘制直线
    Snap2dResultSPtr snapImplNoPicked(
        const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds);

    // 绘制圆
    Snap2dResultSPtr snapImplNoPicked(
        const DrawCircle2dContext* pDrawCircleContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds);

    // 绘制圆弧
    Snap2dResultSPtr snapImplNoPicked(
        const DrawArc2dContext* pDrawArcContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds);

    // 曲线端点(未拾取:无对齐捕捉)
    Snap2dResultSPtr snapImplNoPicked(
        const SketchCurveEndPointContext* pCurveEndPointContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY, const std::set<wydb::ElementId>& excludeIds);

    // Resolve element highlight transients: reuse prevCache hits, create the rest via createHighlightTransient
    std::map<wydb::ElementId, GuiCmdTransientSPtr> createHighlightTransients(
        const Snap2dResult& result,
        const std::map<wydb::ElementId, GuiCmdTransientSPtr>& prevCache) const;

    // 捕捉角度
    AngleSnapObjectSPtr snapAngleSnapObject(
        const std::vector<AngleSnapObjectSPtr>& angleSnapObjs,
        double angle, double angleTol);
    AngleSnapObjectSPtr snapAngleSnapObject(
        const std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions,
        double angle, double angleTol, const std::set<wydb::ElementId>& excludeIds);

    // 捕捉对齐:动态
    template <typename T, template <typename...> class Container = std::vector>
    T snapSingleValueInContainer(
        const Container<T>& container,
        double value,
        double tol, double scScreen2Sketch, const std::set<wydb::ElementId>& excludeIds)
    {
        assert(tol > 0.0);
        assert(scScreen2Sketch > 0.0);

        tol = std::fabs(tol * scScreen2Sketch); // transform from pixel to sketch

        for (const T& pSnapObj : container)
        {
            assert(pSnapObj);
            if (excludeIds.find(pSnapObj->getId()) != excludeIds.cend()) continue;
            if (_erasedIds.find(pSnapObj->getId()) != _erasedIds.cend()) continue;
            if (std::fabs(pSnapObj->getValue() - value) <= tol)
            {
                return pSnapObj;
            }
        }

        return nullptr;
    }

    // 捕捉对齐
    template<typename T>
    T snapSingleValueInRangeMap(
        const std::map<DoubleMinMax, SnapRange<T>>& map,
        double value,
        double tol, double scScreen2Sketch, const std::set<wydb::ElementId>& excludeIds)
    {
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
        auto snapInList = [value, tol, this, &excludeIds]
            (const std::list<T>& objects) -> T
        {
            for (const T& pSnapObj : objects)
            {
                assert(pSnapObj);
                if (excludeIds.find(pSnapObj->getId()) != excludeIds.cend()) continue;
                if (this->_erasedIds.find(pSnapObj->getId()) != this->_erasedIds.cend()) continue;
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
                if (iterLeft->first.max == iterMiddle->first.min) // 紧挨着
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

    // 捕捉相等
    EqualSnapObjectSPtr snapEqualInMap(
        const std::map<DoubleMinMax, SnapRange<EqualSnapObjectSPtr>>& map,
        double value, double tol, double scaleScreen, short flags, double value2, const std::set<wydb::ElementId>& excludeIds);

protected:
    // 草图平面
    wy3d::Plane _sketchPlane;
    // 捕捉上下文
    Snap2dContextSPtr _pSnapContext;
    // 平行和垂直
    std::vector<SnapRange<AngleSnapObjectSPtr>> _parlAndPerps;
    // 对齐X
    std::map<DoubleMinMax, SnapRange<AlignXSnapObjectSPtr>> _alignXMap;
    // 对齐Y
    std::map<DoubleMinMax, SnapRange<AlignYSnapObjectSPtr>> _alignYMap;
    // 动态捕捉对象集合
    // 和绘制的上下文相关,当上下文变更时需要清空
    struct DynSnapObjs
    {
        // 切点和垂足
        std::map<wydb::ElementId, std::list<PointSnapObjectSPtr>> id2PointSnapObjects;
        // 动态对齐
        std::vector<AlignXSnapObjectSPtr> alignXs;
        std::vector<AlignYSnapObjectSPtr> alignYs;

        DynSnapObjs()
        {
            alignXs.reserve(5);
            alignYs.reserve(5);
        }

        void clear()
        {
            id2PointSnapObjects.clear();
            alignXs.clear();
            alignYs.clear();
        }
    } _dynSnapObjs;

    // 当前捕捉系统中的元素
    std::set<wydb::ElementId> _ids;

private:
    // Snap rule table: one rule per context type, both faces
    std::map<std::type_index, std::shared_ptr<Snap2dRuleHandler>> _rules;
    // 捕捉结果
    Snap2dResultSPtr _pSnapResult;
    // 水平和竖直
    std::vector<AngleSnapObjectSPtr> _defaultAngleSnapObjects;
    // 平行和垂直
    size_t _angleSpansCnt;
    double _angleSpan;
    // 相等
    std::map<DoubleMinMax, SnapRange<EqualSnapObjectSPtr>> _equalMap;

    // 数据库更新信息(用于局部更新捕捉数据)
    std::set<wydb::ElementId> _addedIds;    // 实际新增的元素
    std::set<wydb::ElementId> _erasedIds;    // 实际删除的元素
    // 修改的元素
    // 如果一个元素先修改再移除会同时存在于_erasedIds&_modifiedIds;
    std::set<wydb::ElementId> _modifiedIds;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_SYSTEM_IMPL_H
