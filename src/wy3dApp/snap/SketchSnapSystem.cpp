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

#include "application/Application.h"
#include "SnapContext.h"
#include "SnapConsts.h"
#include "utils/MathUtils.h"
#include "commands/GuiCommand.h"
#include "utils/StlUtil.h"
#include "scene/nodes/ElementNodeType.h"

#include <iostream>

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

SketchSnapSystem::SketchSnapSystem(const wy3d::Sketch* pSketch)
    : _sketchId(wydb::ElementId::kNull), _angleSpansCnt(30), _angleSpan(1.0)
{
    assert(pSketch);
    _sketchId = pSketch->getId();
    _sketchPlane = pSketch->getPlane();

    // 构造平行垂直捕捉数据
    this->constructParlAndPerpsData();

    // 初始化
    this->init(pSketch);

    // 点选选项
    _pickOption.pickHalfSize = SnapConsts::PickSize;
    _pickOption.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
    _pickOption.selType = wy3d::SelectionType::Element;
    _pickOption.pSelPreFilter = std::make_shared<SketchSnapPreSelFilter>(wy3d::SketchCurve::classInfo(), nullptr);
}

SketchSnapSystem::SketchSnapSystem(const wy3d::SketchPlane& workPlane)
    : _sketchId(wydb::ElementId::kNull), _sketchPlane(workPlane), _angleSpansCnt(30), _angleSpan(1.0)
{
    // 构造平行垂直捕捉数据
    this->constructParlAndPerpsData();

    // 水平和竖直
    _defaultAngleSnapObjects.reserve(5);
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_HORZ()));
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_VERT()));

    // 坐标原点对齐
    wy::Vector2 origin = wy::Vector2::kZero;
    this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(NULL_ID, origin));
    this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(NULL_ID, origin));

    // 平行
    /*
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(1.0, 1.0)), _parlAndPerps);
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(-1.0, 1.0)), _parlAndPerps);
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(-1.0, -1.0)), _parlAndPerps);
    this->appendAngleSnapObject(MKSPTR_Parl(wydb::ElementId::kNull, wy::Vector2::kZero, wy::Vector2(1.0, -1.0)), _parlAndPerps);
    */

    // 点选选项
    _pickOption.pickMask = 0; // 不支持Pick
}

void SketchSnapSystem::constructParlAndPerpsData()
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

SketchSnapSystem::~SketchSnapSystem()
{
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

    // 水平和竖直
    _defaultAngleSnapObjects.reserve(5);
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_HORZ()));
    _defaultAngleSnapObjects.emplace_back(MOVE(MKSPTR_VERT()));

    // 坐标原点对齐
    wy::Vector2 origin = wy::Vector2::kZero;
    this->appendAlignSnapObject<AlignXSPtr>(_alignXMap, MKSPTR_AlignX(NULL_ID, origin));
    this->appendAlignSnapObject<AlignYSPtr>(_alignYMap, MKSPTR_AlignY(NULL_ID, origin));

    // 创建所有的捕捉对象
    wydb::ElementId id(wydb::ElementId::kNull);
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        id = iter.current();
        this->appendSnapObjects(pDb, id);
    }

    return true;
}

void SketchSnapSystem::reset()
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

bool SketchSnapSystem::partiallyUpdate(const wydb::Database* pDb)
{
    assert(pDb);
    if (!pDb)
    {
        return false;
    }

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
        this->reset();
        this->init(this->getSketch());
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

void SketchSnapSystem::appendAngleSnapObject(
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

void SketchSnapSystem::appendEqualSnapObject(EqualSnapObjectSPtr&& pSnapObj)
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

SketchSnapResultSPtr SketchSnapSystem::snap(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, SketchSnapContextSPtr pContext)
{
    assert(pContext);
    SketchSnapResultSPtr pSnapResult = this->snapImpl(x, y, pView, pos, pContext, nullptr);
    this->setSnapResult(pSnapResult);
    return pSnapResult;
}

SketchSnapResultSPtr SketchSnapSystem::snap(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, SketchSnapContextSPtr pContext,
    const std::set<wydb::ElementId>& idsToExclude)
{
    assert(pContext);
    SketchSnapResultSPtr pSnapResult = this->snapImpl(x, y, pView, pos, pContext, &idsToExclude);
    this->setSnapResult(pSnapResult);
    return pSnapResult;
}

void SketchSnapSystem::setSnapResult(SketchSnapResultSPtr pSnapResult)
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
                std::map<wydb::ElementId, SketchCurveTransientSPtr> cache = _pSnapResult->getSketchCurveTransients();
                _pSnapResult = pSnapResult;
                _pSnapResult->show(&cache);
            }
        }
        else
        {
            _pSnapResult = pSnapResult;
            _pSnapResult->show();
        }
    }
    else
    {
        _pSnapResult = nullptr;
    }
}

SketchSnapResultSPtr SketchSnapSystem::snapImpl(
    double x, double y, osgViewer::View* pView,
    const wy::Vector2& pos, std::shared_ptr<SketchSnapContext> pContext,
    const std::set<wydb::ElementId>* pIdsToExclude)
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
    static const std::set<wydb::ElementId> emptyIds;
    SketchSnapPreSelFilter* pPreSelFilter = dynamic_cast<SketchSnapPreSelFilter*>(_pickOption.pSelPreFilter.get());
    if (pPreSelFilter) pPreSelFilter->setExcludeIds(pContext ? &(pContext->getExcludedIds()) : nullptr);
    wyap::Selection sel = PointPick::pick(pDb, pView, x, y, _pickOption);
    wydb::ElementId pickedId = sel.getElementId();

    // 拾取到元素
    if (!pickedId.isNull())
    {
        if (const SketchLocateContext* pLocateContext = dynamic_cast<const SketchLocateContext*>(pContext.get()))
        {
            SketchSnapResultSPtr pSnapRet = this->snapImplPicked(
                pLocateContext, pDb, pickedId, pos, xPerPixel, yPerPixel);
            if (pSnapRet) return pSnapRet;
        }
        else if (const SketchDrawLineContext* pDrawLineContext = dynamic_cast<const SketchDrawLineContext*>(pContext.get()))
        {
            SketchSnapResultSPtr pSnapRet = this->snapImplPicked(
                pDrawLineContext, pDb, pickedId, pos, xPerPixel, yPerPixel);
            if (pSnapRet) return pSnapRet;
        }
        else if (const SketchDrawCircleContext* pDrawCircleContext = dynamic_cast<const SketchDrawCircleContext*>(pContext.get()))
        {
            SketchSnapResultSPtr pSnapRet = this->snapImplPicked(
                pDrawCircleContext, pDb, pickedId, pos, xPerPixel, yPerPixel);
            if (pSnapRet) return pSnapRet;
        }
        else if (const SketchDrawArcContext* pDrawArcContext = dynamic_cast<const SketchDrawArcContext*>(pContext.get()))
        {
            SketchSnapResultSPtr pSnapRet = this->snapImplPicked(
                pDrawArcContext, pDb, pickedId, pos, xPerPixel, yPerPixel);
            if (pSnapRet) return pSnapRet;
        }
    }

    // 定位点
    if (const SketchLocateContext* pLocateContext = dynamic_cast<const SketchLocateContext*>(pContext.get()))
    {
        SketchSnapResultSPtr pSnapRet = this->snapImplNoPicked(
            pLocateContext, pDb, pos, xPerPixel, yPerPixel);
        if (pSnapRet) return pSnapRet;
    }
    // 绘制直线
    else if (const SketchDrawLineContext* pDrawLineContext = dynamic_cast<const SketchDrawLineContext*>(pContext.get()))
    {
        SketchSnapResultSPtr pSnapRet = this->snapImplNoPicked(
            pDrawLineContext, pDb, pos, xPerPixel, yPerPixel);
        if (pSnapRet) return pSnapRet;
    }
    // 绘制圆
    else if (const SketchDrawCircleContext* pDrawCircleContext = dynamic_cast<const SketchDrawCircleContext*>(pContext.get()))
    {
        SketchSnapResultSPtr pSnapRet = this->snapImplNoPicked(
            pDrawCircleContext, pDb, pos, xPerPixel, yPerPixel);
        if (pSnapRet) return pSnapRet;
    }
    // 绘制圆弧
    else if (const SketchDrawArcContext* pDrawArcContext = dynamic_cast<const SketchDrawArcContext*>(pContext.get()))
    {
        SketchSnapResultSPtr pSnapRet = this->snapImplNoPicked(
            pDrawArcContext, pDb, pos, xPerPixel, yPerPixel);
        if (pSnapRet) return pSnapRet;
    }

    return nullptr;
}

std::shared_ptr<AngleSnapObject> SketchSnapSystem::snapAngleSnapObject(
    const std::vector<AngleSnapObjectSPtr>& angleSnapObjs,
    double angle, double tol, const SketchSnapContext* pContext)
{
    assert(angle >= 0.0 && angle < wy3d::PI);
    assert(tol > 0.0);
    assert(pContext);
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

std::shared_ptr<AngleSnapObject> SketchSnapSystem::snapAngleSnapObject(
    const std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions,
    double angle, double tol,
    const SketchSnapContext* pContext)
{
    assert(angle >= 0.0 && angle < wy3d::PI);
    assert(tol > 0.0);
    assert(pContext);
    wydb::ElementId excludeId = wydb::ElementId::kNull;
    if (pContext)
    {
        excludeId = pContext->getId();
    }

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
    auto snapInList = [angle, tol, excludeId, &minDelta, &pSnapResult](
        const std::vector<SnapRange<AngleSnapObjectSPtr>>& angleRegions,
        size_t idx)
    {
        if (excludeId.isNull())
        {
            for (const AngleSnapObjectSPtr& pAngleSnapObj : angleRegions[idx].objects)
            {
                assert(pAngleSnapObj);
                double delta = std::fabs(pAngleSnapObj->getValue() - angle);
                if (delta > tol) continue;
                if (delta < minDelta)
                {
                    pSnapResult = pAngleSnapObj;
                    minDelta = delta;
                }
            }
        }
        else
        {
            for (const AngleSnapObjectSPtr& pAngleSnapObj : angleRegions[idx].objects)
            {
                assert(pAngleSnapObj);
                if (pAngleSnapObj->getId() == excludeId) continue;
                double delta = std::fabs(pAngleSnapObj->getValue() - angle);
                if (delta > tol) continue;
                if (delta < minDelta)
                {
                    pSnapResult = pAngleSnapObj;
                    minDelta = delta;
                }
            }
        }
    };

    snapInList(angleRegions, idx);
    if (idx2nd != -1) snapInList(angleRegions, idx2nd);
    return pSnapResult;
}

EqualSnapObjectSPtr SketchSnapSystem::snapEqualInMap(
    const std::map<DoubleMinMax, SnapRange<EqualSnapObjectSPtr>>& map,
    double value, double tol, double scScreen2Sketch, short flags, double value2,
    const SketchSnapContext* pContext)
{
    assert(value > 0.0);
    assert(tol > 0.0);
    assert(scScreen2Sketch > 0.0);
    assert(pContext);

    // 排除绘制对象自身
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
    auto snapInList = [value, tol, flags, value2, excludeId, this]
        (const std::list<EqualSnapObjectSPtr>& objects) -> EqualSnapObjectSPtr
    {
        for (const EqualSnapObjectSPtr& pSnapObj : objects)
        {
            assert(pSnapObj);
            if (pSnapObj->getId() == excludeId) continue;
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

// 定位点
SketchSnapResultSPtr SketchSnapSystem::snapImplNoPicked(
    const SketchLocateContext* pLocateContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
{
    assert(pLocateContext);
    assert(_pSnapContext);
    assert(pLocateContext == _pSnapContext.get());
    assert(pDb);

    // 对齐X
    AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(_alignXMap, pos.x(), 10, scaleScreenX, pLocateContext);
    // 对齐Y
    AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(_alignYMap, pos.y(), 10, scaleScreenY, pLocateContext);
    // 结果
    if (pAlignXSnapObj && pAlignYSnapObj)
    {
        return std::make_shared<SketchSnapResult>(_sketchPlane,
            wy::Vector2(pAlignXSnapObj->getValue(), pAlignYSnapObj->getValue()), _pSnapContext, pAlignXSnapObj, pAlignYSnapObj);
    }
    else if (pAlignXSnapObj)
    {
        return std::make_shared<SketchSnapResult>(_sketchPlane,
            wy::Vector2(pAlignXSnapObj->getValue(), pos.y()), _pSnapContext, pAlignXSnapObj);
    }
    else if (pAlignYSnapObj)
    {
        return std::make_shared<SketchSnapResult>(_sketchPlane,
            wy::Vector2(pos.x(), pAlignYSnapObj->getValue()), _pSnapContext, pAlignYSnapObj);
    }

    return nullptr;
}

// 定位点+捕捉到对象
SketchSnapResultSPtr SketchSnapSystem::snapImplPicked(
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    else
    {
        return nullptr;
    }

    return nullptr;
}

// 绘制直线
SketchSnapResultSPtr SketchSnapSystem::snapImplNoPicked(
    const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
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
        AngleSnapObjectSPtr pAngleSnapObj = this->snapAngleSnapObject(_defaultAngleSnapObjects, drawAngle, angleTol, pDrawLineContext);
        // 2.平行垂直
        if (!pAngleSnapObj) pAngleSnapObj = this->snapAngleSnapObject(_parlAndPerps, drawAngle, angleTol, pDrawLineContext);
        // 捕捉到角度
        if (pAngleSnapObj)
        {
            // 捕捉位置
            double angle = pAngleSnapObj->getValue();
            assert(angle >= 0.0 && angle < wy3d::PI);
            // added by wangyao 2025.04.10 {
            // 水平和竖直时直接设置dir为(0,1)or(1,0)而不是通过角度计算是为了避免误差
            wy::Vector2 dir;
            if (pAngleSnapObj->getType() == SketchSnapType::Horizontal)
                dir.set(1.0, 0.0);
            else if (pAngleSnapObj->getType() == SketchSnapType::Vertical)
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
        if (pAngleSnapObj->getType() == SketchSnapType::Horizontal || std::fabs(angle) <= wy3d::EPS)
        {
            // 相等
            if (EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawLength, 10,
                scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY, SketchSnapObject::LINE, 0.0,
                pDrawLineContext))
            {
                retPos = drawStartPnt + pEqualSnapObj->getValue() * dir;
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(
                _alignXMap, pos.x(), 10, scaleScreenX, pDrawLineContext))
            {
                retPos.setX(pAlignXSnapObj->getValue());
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignXSnapObj);
            }
        }
        // 竖直
        else if (pAngleSnapObj->getType() == SketchSnapType::Vertical || std::fabs(angle - wy3d::PI_2) <= wy3d::EPS)
        {
            // 相等
            if (EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawLength, 10,
                scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY, SketchSnapObject::LINE, 0.0,
                pDrawLineContext))
            {
                retPos = drawStartPnt + pEqualSnapObj->getValue() * dir;
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐Y
            else if (AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(
                _alignYMap, pos.y(), 10, scaleScreenY, pDrawLineContext))
            {
                retPos.setY(pAlignYSnapObj->getValue());
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignYSnapObj);
            }
        }
        // 其它角度
        else
        {
            // 相等
            if (EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawLength, 10,
                scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY, SketchSnapObject::LINE, angle,
                pDrawLineContext))
            {
                retPos = drawStartPnt + pEqualSnapObj->getValue() * dir;
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pEqualSnapObj);
            }
            // 对齐X
            else if (AlignXSnapObjectSPtr pAlignXSnapObj = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(
                _alignXMap, pos.x(), 10, scaleScreenX, pDrawLineContext))
            {
                if (std::fabs(std::cos(angle)) > wy3d::EPS)
                {
                    // (y - startY) / (x - startX) = sin(t) / cos(t) 
                    // ==> y = (x - startX) * sin(t) / cos(t) + startY
                    double x = pAlignXSnapObj->getValue();
                    double y = (x - drawStartPnt.x()) * std::sin(angle) / std::cos(angle) + drawStartPnt.y();
                    retPos.set(x, y);
                    return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignXSnapObj);
                }
                else
                {
                    assert(false);
                }
            }
            // 对齐Y
            else if (AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(
                _alignYMap, pos.y(), 10, scaleScreenY, pDrawLineContext))
            {
                if (std::fabs(std::sin(angle)) > wy3d::EPS)
                {
                    // (y - startY) / (x - startX) = sin(t) / cos(t)
                    // ==> x = (y - startY) * cos(t) / sin(t) + startX
                    double y = pAlignYSnapObj->getValue();
                    double x = (y - drawStartPnt.y()) * std::cos(angle) / std::sin(angle) + drawStartPnt.x();
                    retPos.set(x, y);
                    return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj, pAlignYSnapObj);
                }
                else
                {
                    assert(false);
                }
            }
        }

        return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAngleSnapObj);
    }
    // 没有捕捉到角度
    else
    {
        // 对齐X
        AlignXSnapObjectSPtr pAlignX = this->snapSingleValueInRangeMap<AlignXSnapObjectSPtr>(
            _alignXMap, pos.x(), 10, scaleScreenX, pDrawLineContext);
        // 对齐Y
        AlignYSnapObjectSPtr pAlignY = this->snapSingleValueInRangeMap<AlignYSnapObjectSPtr>(
            _alignYMap, pos.y(), 10, scaleScreenY, pDrawLineContext);
        // 结果
        if (pAlignX && pAlignY)
        {
            return std::make_shared<SketchSnapResult>(_sketchPlane,
                wy::Vector2(pAlignX->getValue(), pAlignY->getValue()), _pSnapContext, pAlignX, pAlignY);
        }
        else if (pAlignX)
        {
            return std::make_shared<SketchSnapResult>(_sketchPlane,
                wy::Vector2(pAlignX->getValue(), pos.y()), _pSnapContext, pAlignX);
        }
        else if (pAlignY)
        {
            return std::make_shared<SketchSnapResult>(_sketchPlane,
                wy::Vector2(pos.x(), pAlignY->getValue()), _pSnapContext, pAlignY);
        }
    }

    return nullptr;
}

// 绘制直线+捕捉到对象
SketchSnapResultSPtr SketchSnapSystem::snapImplPicked(
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        // 捕捉点在直线段上
        wy::Vector2 projPnt;
        if (projectPointOnLineseg(pos, pLine->getStartPoint(), pLine->getEndPoint(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurveSnapObject = std::make_shared<PointOnCurveSnapObject>(
                pLine->getId(), SketchSnapObject::LINE);
            return std::make_shared<SketchSnapResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurveSnapObject);
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        // 捕捉点在圆上
        wy::Vector2 projPnt;
        if (projectPointOnCircle(pos, pCircle->getCenter(), pCircle->getRadius(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::CIRCLE);
            return std::make_shared<SketchSnapResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }

        // 捕捉点在圆弧上
        wy::Vector2 projPnt;
        if (projectPointOnArc(pos, pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(), projPnt))
        {
            PointOnCurveSnapObjectSPtr pPointOnCurve = std::make_shared<PointOnCurveSnapObject>(
                pickedId, SketchSnapObject::ARC);
            return std::make_shared<SketchSnapResult>(_sketchPlane, projPnt, _pSnapContext, pPointOnCurve);
        }
    }
    else
    {
        return nullptr;
    }

    return nullptr;
}

// 绘制圆
SketchSnapResultSPtr SketchSnapSystem::snapImplNoPicked(
    const SketchDrawCircleContext* pDrawCircleContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
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
        return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
    }

    // 相等
    EqualSnapObjectSPtr pEqualSnapObj = this->snapEqualInMap(_equalMap, drawRadius, 10,
        scaleScreenX > scaleScreenY ? scaleScreenX : scaleScreenY,
        SketchSnapObject::CIRCLE | SketchSnapObject::ARC,
        DBL_MAX, pDrawCircleContext);


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
            _dynSnapObjs.alignXs, pos.x(), 10.0, scaleScreenX, pDrawCircleContext))
        {
            if (pEqualSnapObj) // 捕捉到相等
            {
                double y = pos.y();
                if (drawVec.dot(wy::Vector2::kYAxis) > 0.0) y = drawCenterPnt.y() + pEqualSnapObj->getValue();
                else y = drawCenterPnt.y() - pEqualSnapObj->getValue();
                wy::Vector2 retPos(pAlignXSnapObj->getValue(), y);
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAlignXSnapObj, pEqualSnapObj);
            }
            else
            {
                wy::Vector2 retPos(pAlignXSnapObj->getValue(), pos.y());
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAlignXSnapObj);
            }
        }
        
        // 对齐Y
        if (AlignYSnapObjectSPtr pAlignYSnapObj = this->snapSingleValueInContainer<AlignYSnapObjectSPtr, std::vector>(
            _dynSnapObjs.alignYs, pos.y(), 10.0, scaleScreenY, pDrawCircleContext))
        {
            if (pEqualSnapObj) // 捕捉到相等
            {
                double x = pos.x();
                if (drawVec.dot(wy::Vector2::kXAxis) > 0.0) x = drawCenterPnt.x() + pEqualSnapObj->getValue();
                else x = drawCenterPnt.x() - pEqualSnapObj->getValue();
                wy::Vector2 retPos(x, pAlignYSnapObj->getValue());
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAlignYSnapObj, pEqualSnapObj);
            }
            else
            {
                wy::Vector2 retPos(pos.x(), pAlignYSnapObj->getValue());
                return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pAlignYSnapObj);
            }
        }
    }
    
    // 仅仅捕捉到相等
    if (pEqualSnapObj)
    {
        drawVec.normalize();
        wy::Vector2 retPos = drawCenterPnt + pEqualSnapObj->getValue() * drawVec;
        return std::make_shared<SketchSnapResult>(_sketchPlane, retPos, _pSnapContext, pEqualSnapObj);
    }

    return nullptr;
}

// 绘制圆+捕捉到对象
SketchSnapResultSPtr SketchSnapSystem::snapImplPicked(
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
        }
        else
        {
            return nullptr;
        }
    }

    return nullptr;
}

// 绘制圆弧
SketchSnapResultSPtr SketchSnapSystem::snapImplNoPicked(
    const SketchDrawArcContext* pDrawArcContext, const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY)
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
        return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
    }

    return nullptr;
}

// 绘制圆弧+捕捉到对象
SketchSnapResultSPtr SketchSnapSystem::snapImplPicked(
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
            return std::make_shared<SketchSnapResult>(_sketchPlane, pPointSnapRet->getValue(), _pSnapContext, pPointSnapRet);
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

void SketchSnapSystem::onDatabaseChanged(
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