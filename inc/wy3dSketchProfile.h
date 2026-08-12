///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_PROFILE_H
#define WY3D_SKETCH_PROFILE_H

#include <vector>
#include <list>
#include <map>
#include <memory>

#include <RTree/RTree.h>

#include <wyVector2.h>
#include <wy3dBoundingBox2.h>
#include <wy3dDefs.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchCurveGraph.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchProfile
{
public:
    SketchProfile(const Sketch* pSketch, double tol = 1e-5);

    // 校验
    bool check();

    // 获取错误
    std::shared_ptr<SketchError> getError() const { return _pError; }

    // 环
    struct Loop
    {
        Loop() : isClockWise(true) {}

        std::vector<BiCurve> curves;
        bool isClockWise;
    };
    typedef std::shared_ptr<Loop> LoopSPtr;

    // 面
    struct Face
    {
        // 序号为0:外环
        // 其余:内环
        std::vector<LoopSPtr> loops;
    };
    typedef std::shared_ptr<Face> FaceSPtr;

    // 获取面集
    const std::vector<FaceSPtr>& getFaces() const { return _faces; }

protected:
    // 初始化
    bool init();
    // 前置校验器
    virtual bool preValid(const wydb::Database* pDb) { return true; }

protected:
    const Sketch* _pSketch;
    double _tol;
    bool _isValid;

    // 面
    std::vector<FaceSPtr> _faces;

    // 错误信息
    std::shared_ptr<SketchError> _pError;
    std::shared_ptr<SketchError> newErrorOfUndefined() const;
};

class SketchCurveGraph_Profile : public SketchCurveGraph
{
public:
    class CurveLoop
    {
    public:
        CurveLoop() : _curveEntries(), _bbox(), _siedArea(0.0), _area(0.0), _isClockWise(true) {}

        // 所有曲线
        const std::vector<CurveEntry>& curves() const
        {
            return _curveEntries;
        }

        void reserve(size_t n)
        {
            _curveEntries.reserve(n);
        }

        void push_back(const CurveEntry& curveEntry)
        {
            _curveEntries.push_back(curveEntry);
        }

        void pop_back()
        {
            _curveEntries.pop_back();
        }

        // 获取外包围盒
        const wy3d::BoundingBox2& getBoundingBox() const { return _bbox; }
        // 设置外包围盒
        void setBoundingBox(const wy3d::BoundingBox2& bbox)
        {
            _bbox = bbox;
        }

        // 获取有向面积
        double getSignedArea() const
        {
            return _siedArea;
        }
        // 设置有向面积
        void setSignedArea(double siedArea)
        {
            _siedArea = siedArea;
            _area = std::fabs(_siedArea);
        }
        // 获取面积
        double getArea() const
        {
            return _area;
        }

        // 获取是否是顺时针
        bool isClockWise() const
        {
            return _isClockWise;
        }
        // 设置是否是顺时针
        void setIsClockWise(bool isClockWise)
        {
            _isClockWise = isClockWise;
        }

        // 离散化
        void discretize(const std::vector<const SketchCurve*>& curves);
        const std::vector<wy::Vector2>& getPoints() const { return _polygonPnts; }

    private:
        std::vector<CurveEntry> _curveEntries;
        wy3d::BoundingBox2 _bbox;
        double _siedArea;
        double _area;
        bool _isClockWise;

        // added by wangyao 2025.05.03 {
        // 离散化多边形用于判断Loop之间的包含关系
        std::vector<wy::Vector2> _polygonPnts;
        // }
    };
    typedef std::shared_ptr<CurveLoop> CurveLoopSPtr;

    struct CurveFace
    {
        // 序号为0:外环
        // 其余:内环
        std::vector<CurveLoopSPtr> loops;
    };
    typedef std::shared_ptr<CurveFace> CurveFaceSPtr;

public:
    // 构造函数
    explicit SketchCurveGraph_Profile(const std::vector<const SketchCurve*>& curves, double tol = 1e-5);

    // 查找闭合的环
    bool findClosedLoops();
    const std::vector<std::shared_ptr<CurveLoop>>& getClosedLoops() const { return _closedLoops; }

    // 查找所有连通曲线链（开放+闭合）
    bool findLoops();
    const std::vector<std::shared_ptr<CurveLoop>>& getLoops() const { return _loops; }

    // 区分面
    bool distinguishFaces();
    // 获取面
    const std::vector<CurveFaceSPtr>& getFaces() const { return _faces; }

private:
    // 深度优先搜索找环
    bool dfsFindCycle(
        std::vector<bool>& used,
        const std::vector<bool>& degenerated,
        std::shared_ptr<CurveLoop> pCurveLoop,
        std::list<std::shared_ptr<CurveLoop>>& closedLoops);

    // 计算环的有向面积
    double computeSideArea(const CurveLoop& loop);

    // 提取面
    CurveFaceSPtr extractFace(
        const std::vector<CurveLoopSPtr>& closedLoops,
        const RTree<size_t, double, 2>& rtree,
        std::vector<bool>& visited) const;

    // 判断环是否完全包含环
    bool isCurveLoopContains(
        const CurveLoopSPtr& pOuterLoop,
        const CurveLoopSPtr& pLoop,
        double tol) const;
    // 判断圆是否完全包含环
    bool isCircleContains(
        const wy3d::SketchCircle* pOuterCircle,
        const CurveLoopSPtr& pLoop,
        double tol) const;
    // 判断椭圆是否完全包含环
    bool isEllipseContains(
        const CurveLoopSPtr& pOuterLoop,
        const wy3d::SketchEllipse* pOuterEllipse,
        const CurveLoopSPtr& pLoop,
        double tol) const;

    // 判断环是否完全包含点
    // 返回值: 0---点在环上;-1---点在环外;1---点在环内
    int isCurveLoopContainPoint(
        const CurveLoopSPtr& pLoop,
        const wy::Vector2& pnt,
        double tol) const;
    int isCurveLoopContainPoint(
        const std::vector<wy::Vector2>& loopPolygon,
        const wy::Vector2& pnt,
        double tol) const;

private:
    // 闭合环
    std::vector<CurveLoopSPtr> _closedLoops;
    // 所有连通曲线链（含开放和闭合）
    std::vector<CurveLoopSPtr> _loops;
    // 曲线的外包围盒
    std::vector<wy3d::BoundingBox2> _curveBBoxs;
    // 闭合环整体外包围盒
    wy3d::BoundingBox2 _loopsTotalBBox;
    // 面
    std::vector<CurveFaceSPtr> _faces;
};

NS_WY3D_END

#endif // WY3D_SKETCH_PROFILE_H