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
#include <deque>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <utility>
#include <osgViewer/View>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dPlane.h>
#include <wy3dApp/Snap2dContext.h>
#include "Snap2dObject.h"
#include "utils/StlUtil.h"
#include <wy3dApp/GuiCmdTransient.h>
#include <wy3dApp/Snap2dSystem.h>

namespace wy3dApp {

class Snap2dResultDisplay;

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
        double valuePerPixelX, double valuePerPixelY,
        const std::set<wydb::ElementId>& excludeIds)
    {
        return nullptr;
    }

    // Snap with no picked element; null when nothing snapped.
    virtual Snap2dResultSPtr snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY,
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

    // 捕捉：全局几何点优先，未命中才投影并走 2D 索引。
    // 返回捕捉结果与原始平面点(全局命中时第二个值也可用)。
    std::pair<Snap2dResultSPtr, wy::Vector2> snap(
        double x, double y, osgViewer::View* pView,
        const std::set<wydb::ElementId>& excludeIds,
        Snap2dContextSPtr pContext);

    // 只走 2D 索引(调用方已自行跑过全局层)。
    Snap2dResultSPtr snapIndexOnly(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
        Snap2dContextSPtr pContext);

    // 是否启用全局几何点捕捉。
    void setSnap3dEnabled(bool isEnabled) { _isSnap3dEnabled = isEnabled; }
    bool isSnap3dEnabled() const { return _isSnap3dEnabled; }

    // 获取捕捉结果
    Snap2dResultSPtr getSnapResult() const override { return _pSnapResult; }
    // 设置捕捉结果
    void setSnapResult(Snap2dResultSPtr pSnapResult);
    // 清空捕捉结果
    void clearSnapResult() override { this->setSnapResult(nullptr); }

    // 注册外部规则
    wy::ErrorStatus addRule(
        const wyrx::ClassInfo* pContextClass, Snap2dRuleSPtr pRule) override;
    // 移除外部规则
    wy::ErrorStatus removeRule(const wyrx::ClassInfo* pContextClass) override;

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
    // Element highlight transient (none by default: plane users have no element highlight)
    virtual GuiCmdTransientSPtr createHighlightTransient(const wydb::ElementId& id) const
    {
        return nullptr;
    }

    // Picked-side snap implementations (nullptr by default: only sketch systems
    // snap picked elements)
    virtual Snap2dResultSPtr snapImplPicked(
        const Locate2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY);
    virtual Snap2dResultSPtr snapImplPicked(
        const DrawLine2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY);
    virtual Snap2dResultSPtr snapImplPicked(
        const DrawCircle2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY);
    virtual Snap2dResultSPtr snapImplPicked(
        const DrawArc2dContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY);

    // 初始化默认捕捉对象
    void initDefaultSnapObjects();

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

    // 捕捉点
    Snap2dPointSPtr snapPointSnapObject(
        const std::list<Snap2dPointSPtr>& snapObjs,
        const wy::Vector2& pos, double tol,
        double valuePerPixelX, double valuePerPixelY)
    {
        assert(tol > 0.0);
        assert(valuePerPixelX > 0.0);
        assert(valuePerPixelY > 0.0);

        double tolX = std::fabs(tol * valuePerPixelX); // transform tol from pixel to sketch
        double tolY = std::fabs(tol * valuePerPixelY); // transform tol from pixel to sketch
        for (const Snap2dPointSPtr& pSnapObj : snapObjs)
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

    // Apply accumulated database changes.
    bool partiallyUpdate(const wydb::Database* pDb);

    // 捕捉具体实现
    Snap2dResultSPtr snapImpl(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<Snap2dContext> pContext);

    // 定位点
    Snap2dResultSPtr snapImplNoPicked(
        const Locate2dContext* pLocateContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds);

    // 绘制直线
    Snap2dResultSPtr snapImplNoPicked(
        const DrawLine2dContext* pDrawLineContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds);

    // 绘制圆
    Snap2dResultSPtr snapImplNoPicked(
        const DrawCircle2dContext* pDrawCircleContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds);

    // 绘制圆弧
    Snap2dResultSPtr snapImplNoPicked(
        const DrawArc2dContext* pDrawArcContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double valuePerPixelX, double valuePerPixelY, const std::set<wydb::ElementId>& excludeIds);

    // 捕捉角度
    Snap2dAngleSPtr snapAngleSnapObject(
        const std::vector<Snap2dAngleSPtr>& angleSnapObjs,
        double angle, double angleTol);

    // 捕捉对齐:动态
    template <typename T, template <typename...> class Container = std::vector>
    T snapSingleValueInContainer(
        const Container<T>& container,
        double value,
        double tol, double valuePerPixel, const std::set<wydb::ElementId>& excludeIds)
    {
        assert(tol > 0.0);
        assert(valuePerPixel > 0.0);

        tol = std::fabs(tol * valuePerPixel); // transform from pixel to sketch

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
        double tol, double valuePerPixel, const std::set<wydb::ElementId>& excludeIds)
    {
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

protected:
    // 草图平面
    wy3d::Plane _sketchPlane;
    // 捕捉上下文
    Snap2dContextSPtr _pSnapContext;

    // 对齐参照点：X/Y 两个索引对象成对保存，id 与参照点直接从对象读取
    struct AlignRefPoint
    {
        Snap2dAlignXSPtr pAlignX;
        Snap2dAlignYSPtr pAlignY;
    };
    static constexpr size_t kMaxAlignRefPointCount = 6; // 6 动态 + 常驻原点 = 7

    // 捕捉上下文（命令/步骤）切换时清理动态对齐点
    void beginSnapContext(const Snap2dContextSPtr& pContext);
    // 追加对齐参照点：按位置去重、刷新为最新、超上限淘汰最旧
    void appendAlignRefPoint(const wydb::ElementId& id, const wy::Vector2& point);
    // 从捕捉结果中提取基础点加入动态对齐点序列
    void appendAlignRefPoints(const Snap2dResultSPtr& pSnapResult);
    // 是否属于可进入对齐序列的基础点类型
    static bool isAlignRefPointType(Snap2dType type);
    // 移除引用已失效元素的参照点
    void pruneAlignRefPoints(const std::set<wydb::ElementId>& invalidIds);
    // 对齐X/Y查找：线性扫描，返回容差内距离最近的参照点
    Snap2dAlignXSPtr snapAlignRefX(double value, double tolPx, double valuePerPixel,
        const std::set<wydb::ElementId>& excludeIds) const;
    Snap2dAlignYSPtr snapAlignRefY(double value, double tolPx, double valuePerPixel,
        const std::set<wydb::ElementId>& excludeIds) const;
    const AlignRefPoint* findAlignRefPoint(double value, bool isX, double tolPx,
        double valuePerPixel, const std::set<wydb::ElementId>& excludeIds) const;

    // 常驻原点参照
    AlignRefPoint _originAlignRef;
    // 动态对齐参照点（队首最旧、队尾最新，最多 kMaxAlignRefPointCount 个）
    std::deque<AlignRefPoint> _alignRefPoints;

    // 动态参考图元池：只存 id，几何在查询时从数据库实时读取。
    // 队首最旧、队尾最新；仅收集当前草图的直线/中心线/圆/圆弧。
    static constexpr size_t kMaxSnapRefElementCount = 6;

    // 从捕捉结果中采集参考图元
    void appendSnapRefElements(const Snap2dResultSPtr& pSnapResult,
        const wydb::Database* pDb, const std::set<wydb::ElementId>& excludeIds);
    // 采集单个参考图元：先池内去重，首次入池才做数据库/类型校验
    void appendSnapRefElement(const wydb::ElementId& id,
        const wydb::Database* pDb, const std::set<wydb::ElementId>& excludeIds);
    // 移除已删除的参考图元
    void pruneSnapRefElements(const std::set<wydb::ElementId>& invalidIds);
    // 取得可作为参考的草图曲线(过滤空 id/已删除/类型/父草图)
    const wy3d::SketchCurve* getSnapRefCurve(const wydb::Database* pDb,
        const wydb::ElementId& id) const;
    // 参考图元作用域：默认不收集(工作平面型系统保持旧行为)
    virtual bool isSnapRefElementAllowed(const wy3d::SketchCurve& curve) const
    {
        return false;
    }

    // 平行/垂直(参考动态池)
    Snap2dAngleSPtr snapParallelOrPerpendicular(const wydb::Database* pDb,
        double drawAngle, double angleTol,
        const std::set<wydb::ElementId>& excludeIds) const;
    // 相等长度(参考动态池)
    Snap2dEqualLengthSPtr snapEqualLengthInRefElements(const wydb::Database* pDb,
        double length, double tolPx, double valuePerPixel, double angle,
        const std::set<wydb::ElementId>& excludeIds) const;
    // 相等半径(参考动态池)
    Snap2dEqualRadiusSPtr snapEqualRadiusInRefElements(const wydb::Database* pDb,
        double radius, double tolPx, double valuePerPixel,
        const std::set<wydb::ElementId>& excludeIds) const;

    std::deque<wydb::ElementId> _snapRefElements;
    // 动态捕捉对象集合
    // 和绘制的上下文相关,当上下文变更时需要清空
    struct DynSnapObjs
    {
        // 切点和垂足
        std::map<wydb::ElementId, std::list<Snap2dPointSPtr>> id2PointSnapObjects;
        // 动态对齐
        std::vector<Snap2dAlignXSPtr> alignXs;
        std::vector<Snap2dAlignYSPtr> alignYs;

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

private:
    // Snap rule table: one rule per context class name (WYRX class), both faces
    std::map<std::string, std::shared_ptr<Snap2dRuleHandler>> _rules;
    // 捕捉结果
    Snap2dResultSPtr _pSnapResult;
    // 是否启用全局几何点捕捉(步骤策略，由命令按步骤设置)
    bool _isSnap3dEnabled = true;
    // 当前结果的显示状态(跨结果复用高亮)
    std::unique_ptr<Snap2dResultDisplay> _pResultDisplay;
    // 水平和竖直
    std::vector<Snap2dAngleSPtr> _defaultAngleSnapObjects;

    // 数据库更新信息(用于局部更新捕捉数据)
    bool _isPartiallyUpdatePending = false;
    std::set<wydb::ElementId> _erasedIds;    // 实际删除的元素
    // 修改的元素
    // 如果一个元素先修改再移除会同时存在于_erasedIds&_modifiedIds;
    std::set<wydb::ElementId> _modifiedIds;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_SYSTEM_IMPL_H
