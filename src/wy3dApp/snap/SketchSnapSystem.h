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

#ifndef WY3DAPP_SKETCH_SNAP_SYSTEM_H
#define WY3DAPP_SKETCH_SNAP_SYSTEM_H

#include <vector>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <memory>
#include <QString>
#include <osg/Node>
#include <RTree/RTree.h>
#include <osgGA/GUIEventHandler>
#include <osgGA/GUIActionAdapter>
#include <osgViewer/View>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include "SnapSystemBase.h"
#include "utils/StlUtil.h"
#include <wyapDocManager.h>
#include <wy3dSketch.h>

#include "commands/transient/SketchCurveTransient.h"
#include "SketchSnapContext.h"
#include "SketchSnapObject.h"
#include "SketchSnapResult.h"
#include "select/PointPick.h"

class SketchSnapSystem : public wydb::DatabaseReactor
{
public:
    explicit SketchSnapSystem(const wy3d::Sketch* pSketch);
    explicit SketchSnapSystem(const wy3d::SketchPlane& workPlane);
    ~SketchSnapSystem();
    SketchSnapSystem(const SketchSnapSystem&) = delete;
    SketchSnapSystem& operator=(const SketchSnapSystem&) = delete;
    SketchSnapSystem(SketchSnapSystem&&) = delete;
    SketchSnapSystem& operator=(SketchSnapSystem&&) = delete;

    // 局部更新
    bool partiallyUpdate(const wydb::Database* pDb);

    // 草图捕捉
    SketchSnapResultSPtr snap(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, SketchSnapContextSPtr pContext);
    SketchSnapResultSPtr snap(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, SketchSnapContextSPtr pContext,
        const std::set<wydb::ElementId>& idsToExclude);

    // 获取捕捉结果
    SketchSnapResultSPtr getSnapResult() const { return _pSnapResult; }
    // 设置捕捉结果
    void setSnapResult(SketchSnapResultSPtr pSnapResult);
    // 清空捕捉结果
    void clearSnapResult() { _pSnapResult = nullptr; }

    // 数据库变更响应
    virtual void onDatabaseChanged(
        const wydb::Database* pDatabase,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

private:
    // 构造平行&垂直数据
    void constructParlAndPerpsData();
    // 初始化
    bool init(const wy3d::Sketch* pSketch);

    const wy3d::Sketch* getSketch() const;

    // 重置
    void reset();

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

private:
    // 添加捕捉对象
    void appendSnapObjects(const wydb::Database* pDb, const wydb::ElementId& id);

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

    // 捕捉具体实现
    SketchSnapResultSPtr snapImpl(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, std::shared_ptr<SketchSnapContext> pContext, const std::set<wydb::ElementId>* pIdsToExclude = nullptr);

    // 定位点
    SketchSnapResultSPtr snapImplNoPicked(
        const SketchLocateContext* pLocateContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 定位点+捕捉到对象
    SketchSnapResultSPtr snapImplPicked(
        const SketchLocateContext* pLocateContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制直线
    SketchSnapResultSPtr snapImplNoPicked(
        const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制直线+捕捉到对象
    SketchSnapResultSPtr snapImplPicked(
        const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制圆
    SketchSnapResultSPtr snapImplNoPicked(
        const SketchDrawCircleContext* pDrawCircleContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制圆+捕捉到对象
    SketchSnapResultSPtr snapImplPicked(
        const SketchDrawCircleContext* pDrawCircleContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制圆弧
    SketchSnapResultSPtr snapImplNoPicked(
        const SketchDrawArcContext* pDrawArcContext, const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制圆弧+捕捉到对象
    SketchSnapResultSPtr snapImplPicked(
        const SketchDrawArcContext* pDrawCircleContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 曲线端点+捕捉到对象
    SketchSnapResultSPtr snapImplPicked(
        const SketchCurveEndPointContext* pCurveEndPointContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 捕捉角度
    AngleSnapObjectSPtr snapAngleSnapObject(
        const std::vector<AngleSnapObjectSPtr>& angleSnapObjs,
        double angle, double angleTol, const SketchSnapContext* pContext);
    AngleSnapObjectSPtr snapAngleSnapObject(
        const std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions,
        double angle, double angleTol, const SketchSnapContext* pContext);

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

    // 捕捉点
    PointSnapObjectSPtr snapPointSnapObject_Nearest(
        const std::list<PointSnapObjectSPtr>& snapObjs,
        const wy::Vector2& pos, double tol,
        double scaleScreenX, double scaleScreenY)
    {
        assert(tol > 0.0);
        assert(scaleScreenX > 0.0);
        assert(scaleScreenY > 0.0);

        double tolX = std::fabs(tol * scaleScreenX); // transform tol from pixel to sketch
        double tolY = std::fabs(tol * scaleScreenY); // transform tol from pixel to sketch
        double minDist(DBL_MAX), disX(0.0), disY(0.0);
        PointSnapObjectSPtr pRet(nullptr);
        for (const PointSnapObjectSPtr& pSnapObj : snapObjs)
        {
            assert(pSnapObj);
            wy::Vector2 value = pSnapObj->getValue();
            disX = std::fabs(value.x() - pos.x());
            disY = std::fabs(value.y() - pos.y());
            if (disX <= tolX && disY <= tolY)
            {
                if (disX + disY < minDist)
                {
                    minDist = disX + disY;
                    pRet = pSnapObj;
                }
            }
        }

        return pRet;
    }

    // 捕捉对齐:动态
    template <typename T, template <typename...> class Container = std::vector>
    T snapSingleValueInContainer(
        const Container<T>& container,
        double value,
        double tol, double scScreen2Sketch,
        const SketchSnapContext* pContext)
    {
        assert(tol > 0.0);
        assert(scScreen2Sketch > 0.0);
        assert(pContext);

        tol = std::fabs(tol * scScreen2Sketch); // transform from pixel to sketch

        if (pContext && !pContext->getId().isNull())
        {
            const wydb::ElementId idToExclude = pContext->getId();
            for (const T& pSnapObj : container)
            {
                assert(pSnapObj);
                if (_erasedIds.find(pSnapObj->getId()) != _erasedIds.cend()) continue;
                if (idToExclude == pSnapObj->getId()) continue; // 在草图元素拖动Gizmo时不能捕捉自身的对齐点
                if (std::fabs(pSnapObj->getValue() - value) <= tol)
                {
                    return pSnapObj;
                }
            }
        }
        else
        {
            for (const T& pSnapObj : container)
            {
                assert(pSnapObj);
                if (_erasedIds.find(pSnapObj->getId()) != _erasedIds.cend()) continue;
                if (std::fabs(pSnapObj->getValue() - value) <= tol)
                {
                    return pSnapObj;
                }
            }
        }

        return nullptr;
    }

    // 捕捉对齐
    template<typename T>
    T snapSingleValueInRangeMap(
        const std::map<DoubleMinMax, SnapRange<T>>& map,
        double value,
        double tol, double scScreen2Sketch,
        const SketchSnapContext* pContext)
    {
        assert(tol > 0.0);
        assert(scScreen2Sketch > 0.0);
        assert(pContext);

        wydb::ElementId excludeId = wydb::ElementId::kNull;
        if (pContext)
        {
            excludeId = pContext->getId();
        }

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
        auto snapInList = [value, tol, excludeId, this]
            (const std::list<T>& objects) -> T
        {
            if (excludeId.isNull())
            {
                for (const T& pSnapObj : objects)
                {
                    assert(pSnapObj);
                    if (this->_erasedIds.find(pSnapObj->getId()) != this->_erasedIds.cend()) continue;
                    if (std::fabs(pSnapObj->getValue() - value) <= tol)
                    {
                        return pSnapObj;
                    }
                }
            }
            else
            {
                for (const T& pSnapObj : objects)
                {
                    assert(pSnapObj);
                    if (pSnapObj->getId() == excludeId) continue;
                    if (this->_erasedIds.find(pSnapObj->getId()) != this->_erasedIds.cend()) continue;
                    if (std::fabs(pSnapObj->getValue() - value) <= tol)
                    {
                        return pSnapObj;
                    }
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
        double value, double tol, double scaleScreen, short flags, double value2,
        const SketchSnapContext* pContext);

private:
    // 草图ID
    wydb::ElementId _sketchId;
    // 草图平面
    wy3d::SketchPlane _sketchPlane;
    // 捕捉上下文
    SketchSnapContextSPtr _pSnapContext;
    // 捕捉结果
    SketchSnapResultSPtr _pSnapResult;

    // 水平和竖直
    std::vector<AngleSnapObjectSPtr> _defaultAngleSnapObjects;
    // 平行和垂直
    size_t _angleSpansCnt;
    double _angleSpan;
    std::vector<SnapRange<AngleSnapObjectSPtr>> _parlAndPerps;
    // 对齐X
    std::map<DoubleMinMax, SnapRange<AlignXSnapObjectSPtr>> _alignXMap;
    // 对齐Y
    std::map<DoubleMinMax, SnapRange<AlignYSnapObjectSPtr>> _alignYMap;
    // 相等
    std::map<DoubleMinMax, SnapRange<EqualSnapObjectSPtr>> _equalMap;
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

    // 数据库更新信息(用于局部更新捕捉数据)
    std::set<wydb::ElementId> _addedIds;    // 实际新增的元素
    std::set<wydb::ElementId> _erasedIds;    // 实际删除的元素
    // 修改的元素
    // 如果一个元素先修改再移除会同时存在于_erasedIds&_modifiedIds;
    std::set<wydb::ElementId> _modifiedIds;
    // 当前捕捉系统中的元素
    std::set<wydb::ElementId> _ids;

    // 点选选项
    PointPickOption _pickOption;
};

typedef std::shared_ptr<SketchSnapSystem> SketchSnapSystemSPtr;

#endif // WY3DAPP_SKETCH_SNAP_SYSTEM_H
