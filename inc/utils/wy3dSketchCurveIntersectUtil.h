///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
//  Class Boilerplate Macros 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_CURVE_INTERSECT_UTIL_H
#define WY3D_SKETCH_CURVE_INTERSECT_UTIL_H

#include <cassert>

#include <wyVector2.h>
#include <utils/wy3dCurveIntersectionUtil.h>
#include <wy3dDefs.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>

NS_WY3D_BEG

class SketchCurveIntersectUtil
{
public:
    inline static unsigned int intersect(const SketchLine* pLineImpl1, const SketchLine* pLineImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl1);
        assert(pLineImpl2);
        wy::Vector2 intPnt;
        if (wy3d::intersectLinesegLineseg(pLineImpl1->getStartPoint(), pLineImpl1->getEndPoint(), pLineImpl2->getStartPoint(), pLineImpl2->getEndPoint(), intPnt))
        {
            outIntPnts.emplace_back(intPnt);
            return 1;
        }
        else
        {
            return 0;
        }
    }

    inline static unsigned int intersect(const SketchLine* pLineImpl1, const SketchCenterLine* pLineImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl1);
        assert(pLineImpl2);
        wy::Vector2 intPnt;
        if (wy3d::intersectLinesegLineseg(pLineImpl1->getStartPoint(), pLineImpl1->getEndPoint(), pLineImpl2->getStartPoint(), pLineImpl2->getEndPoint(), intPnt))
        {
            outIntPnts.emplace_back(intPnt);
            return 1;
        }
        else
        {
            return 0;
        }
    }

    inline static unsigned int intersect(const SketchLine* pLineImpl, const SketchCircle* pCircleImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pCircleImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        // added by wangyao 2025.04.11 {
        // 当直线段和圆相切时,由于求交算法的误差没处理好导致会时不时求不出来交点,
        // 为保证算法的稳定性,先牺牲性能.
        wy::Vector2 startPnt = pLineImpl->getStartPoint();
        wy::Vector2 endPnt = pLineImpl->getEndPoint();
        wy::Vector2 center = pCircleImpl->getCenter();
        wy::Vector2 direction = pLineImpl->getDirection();
        double radius = pCircleImpl->getRadius();
        if (std::fabs((startPnt - center).length() - radius) <= 1e-6)
        {
            if (std::fabs((startPnt - center).dot(direction)) <= radius * 1e-6)
            {
                outIntPnts.emplace_back(startPnt);
                return 1;
            }
        }
        else if (std::fabs((endPnt - center).length() - radius) <= 1e-6)
        {
            if (std::fabs((endPnt - center).dot(direction)) <= radius * 1e-6)
            {
                outIntPnts.emplace_back(endPnt);
                return 1;
            }
        }
        // }
        unsigned int num = wy3d::intersectLinesegCircle(startPnt, endPnt, center, radius, intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchLine* pLineImpl, const SketchArc* pArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 startPnt = pLineImpl->getStartPoint();
        wy::Vector2 endPnt = pLineImpl->getEndPoint();
        wy::Vector2 center = pArcImpl->getCenter();
        double radius = pArcImpl->getRadius();
        double startAngle = pArcImpl->getStartAngle();
        double endAngle = pArcImpl->getEndAngle();
        // added by wangyao 2025.04.11 {
        // 当直线段和圆相切时,由于求交算法的误差没处理好导致会时不时求不出来交点,
        // 为保证算法的稳定性,先牺牲性能.
        if (std::fabs((startPnt - center).length() - radius) <= 1e-6)
        {
            double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), startPnt - center);
            if (wy3d::isAngleInArc(angle, startAngle, endAngle))
            {
                outIntPnts.emplace_back(startPnt);
                return 1;
            }
            else
            {
                return 0;
            }
        }
        else if (std::fabs((endPnt - center).length() - radius) <= 1e-6)
        {
            double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), endPnt - center);
            if (wy3d::isAngleInArc(angle, startAngle, endAngle))
            {
                outIntPnts.emplace_back(endPnt);
                return 1;
            }
            else
            {
                return 0;
            }
        }
        // }
        unsigned int num = wy3d::intersectLinesegArc(startPnt, endPnt,
            center, radius, startAngle, endAngle, intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchLine* pLineImpl, const SketchEllipse* pEllipseImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pEllipseImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectLinesegEllipse(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(),
            pEllipseImpl->getCenter(), pEllipseImpl->getMajorAxis(), pEllipseImpl->getRadiusRatio(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchLine* pLineImpl, const SketchEllipseArc* pEllipseArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pEllipseArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectLinesegEllipseArc(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(),
            pEllipseArcImpl->getCenter(), pEllipseArcImpl->getMajorAxis(), pEllipseArcImpl->getRadiusRatio(), pEllipseArcImpl->getStartAngle(), pEllipseArcImpl->getEndAngle(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchLine* pLineImpl, const SketchSpline* pSplineImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pSplineImpl);
        return wy3d::intersectLinesegSpline(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(), pSplineImpl->getOccSpline(), outIntPnts);
    }

    // added by wangyao 2025.03.30 {
    // 中心线
    inline static unsigned int intersect(const SketchCenterLine* pLineImpl1, const SketchCenterLine* pLineImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl1);
        assert(pLineImpl2);
        wy::Vector2 intPnt;
        if (wy3d::intersectLinesegLineseg(pLineImpl1->getStartPoint(), pLineImpl1->getEndPoint(), pLineImpl2->getStartPoint(), pLineImpl2->getEndPoint(), intPnt))
        {
            outIntPnts.emplace_back(intPnt);
            return 1;
        }
        else
        {
            return 0;
        }
    }

    inline static unsigned int intersect(const SketchCenterLine* pLineImpl, const SketchCircle* pCircleImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pCircleImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectLinesegCircle(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(), pCircleImpl->getCenter(), pCircleImpl->getRadius(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchCenterLine* pLineImpl, const SketchArc* pArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectLinesegArc(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(), pArcImpl->getCenter(), pArcImpl->getRadius(),
            pArcImpl->getStartAngle(), pArcImpl->getEndAngle(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchCenterLine* pLineImpl, const SketchEllipse* pEllipseImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pEllipseImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectLinesegEllipse(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(),
            pEllipseImpl->getCenter(), pEllipseImpl->getMajorAxis(), pEllipseImpl->getRadiusRatio(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchCenterLine* pLineImpl, const SketchEllipseArc* pEllipseArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pEllipseArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectLinesegEllipseArc(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(),
            pEllipseArcImpl->getCenter(), pEllipseArcImpl->getMajorAxis(), pEllipseArcImpl->getRadiusRatio(), pEllipseArcImpl->getStartAngle(), pEllipseArcImpl->getEndAngle(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchCenterLine* pLineImpl, const SketchSpline* pSplineImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pLineImpl);
        assert(pSplineImpl);
        return wy3d::intersectLinesegSpline(pLineImpl->getStartPoint(), pLineImpl->getEndPoint(), pSplineImpl->getOccSpline(), outIntPnts);
    }
    // }

    inline static unsigned int intersect(const SketchCircle* pCircleImpl1, const SketchCircle* pCircleImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pCircleImpl1);
        assert(pCircleImpl2);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectCircleCircle(pCircleImpl1->getCenter(), pCircleImpl1->getRadius(), pCircleImpl2->getCenter(), pCircleImpl2->getRadius(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchCircle* pCircleImpl, const SketchArc* pArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pCircleImpl);
        assert(pArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectCircleArc(pCircleImpl->getCenter(), pCircleImpl->getRadius(), pArcImpl->getCenter(), pArcImpl->getRadius(), pArcImpl->getStartAngle(), pArcImpl->getEndAngle(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchCircle* pCircleImpl, const SketchEllipse* pEllipseImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pCircleImpl);
        assert(pEllipseImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectCircleEllipse(pCircleImpl->getCenter(), pCircleImpl->getRadius(), pEllipseImpl->getCenter(), pEllipseImpl->getMajorAxis(), pEllipseImpl->getRadiusRatio(), intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchCircle* pCircleImpl, const SketchEllipseArc* pEllipseArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pCircleImpl);
        assert(pEllipseArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectCircleEllipseArc(pCircleImpl->getCenter(), pCircleImpl->getRadius(),
            pEllipseArcImpl->getCenter(), pEllipseArcImpl->getMajorAxis(), pEllipseArcImpl->getRadiusRatio(), pEllipseArcImpl->getStartAngle(), pEllipseArcImpl->getEndAngle(),
            intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchCircle* pCircleImpl, const SketchSpline* pSplineImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pCircleImpl);
        assert(pSplineImpl);
        return wy3d::intersectCircleSpline(pCircleImpl->getCenter(), pCircleImpl->getRadius(), pSplineImpl->getOccSpline(), outIntPnts);
    }

    inline static unsigned int intersect(const SketchArc* pArcImpl1, const SketchArc* pArcImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pArcImpl1);
        assert(pArcImpl2);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        unsigned int num = wy3d::intersectArcArc(pArcImpl1->getCenter(), pArcImpl1->getRadius(), pArcImpl1->getStartAngle(), pArcImpl1->getEndAngle(), pArcImpl2->getCenter(), pArcImpl2->getRadius(), pArcImpl2->getStartAngle(), pArcImpl2->getEndAngle(), intPnt1, intPnt2);
        assert(num >= 0 && num <= 2);
        if (1 == num)
        {
            outIntPnts.emplace_back(intPnt1);
        }
        else if (2 == num)
        {
            outIntPnts.emplace_back(intPnt1);
            outIntPnts.emplace_back(intPnt2);
        }
        return num;
    }

    inline static unsigned int intersect(const SketchArc* pArcImpl, const SketchEllipse* pEllipseImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pArcImpl);
        assert(pEllipseImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectArcEllipse(pArcImpl->getCenter(), pArcImpl->getRadius(), pArcImpl->getStartAngle(), pArcImpl->getEndAngle(),
            pEllipseImpl->getCenter(), pEllipseImpl->getMajorAxis(), pEllipseImpl->getRadiusRatio(),
            intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchArc* pArcImpl, const SketchEllipseArc* pEllipseArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pArcImpl);
        assert(pEllipseArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectArcEllipseArc(pArcImpl->getCenter(), pArcImpl->getRadius(), pArcImpl->getStartAngle(), pArcImpl->getEndAngle(),
            pEllipseArcImpl->getCenter(), pEllipseArcImpl->getMajorAxis(), pEllipseArcImpl->getRadiusRatio(), pEllipseArcImpl->getStartAngle(), pEllipseArcImpl->getEndAngle(),
            intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchArc* pArcImpl, const SketchSpline* pSplineImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pArcImpl);
        assert(pSplineImpl);
        return wy3d::intersectArcSpline(pArcImpl->getCenter(), pArcImpl->getRadius(), pArcImpl->getStartAngle(), pArcImpl->getEndAngle(),
            pSplineImpl->getOccSpline(), outIntPnts);
    }

    inline static unsigned int intersect(const SketchEllipse* pEllipseImpl1, const SketchEllipse* pEllipseImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pEllipseImpl1);
        assert(pEllipseImpl2);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectEllipseEllipse(
            pEllipseImpl1->getCenter(), pEllipseImpl1->getMajorAxis(), pEllipseImpl1->getRadiusRatio(),
            pEllipseImpl2->getCenter(), pEllipseImpl2->getMajorAxis(), pEllipseImpl2->getRadiusRatio(),
            intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchEllipse* pEllipseImpl, const SketchEllipseArc* pEllipseArcImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pEllipseImpl);
        assert(pEllipseArcImpl);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectEllipseEllipseArc(
            pEllipseImpl->getCenter(), pEllipseImpl->getMajorAxis(), pEllipseImpl->getRadiusRatio(),
            pEllipseArcImpl->getCenter(), pEllipseArcImpl->getMajorAxis(), pEllipseArcImpl->getRadiusRatio(), pEllipseArcImpl->getStartAngle(), pEllipseArcImpl->getEndAngle(),
            intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchEllipse* pEllipseImpl, const SketchSpline* pSplineImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pEllipseImpl);
        assert(pSplineImpl);
        return wy3d::intersectEllipseSpline(pEllipseImpl->getCenter(), pEllipseImpl->getMajorAxis(), pEllipseImpl->getRadiusRatio(),
            pSplineImpl->getOccSpline(), outIntPnts);
    }

    inline static unsigned int intersect(const SketchEllipseArc* pEllipseArcImpl1, const SketchEllipseArc* pEllipseArcImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pEllipseArcImpl1);
        assert(pEllipseArcImpl2);
        wy::Vector2 intPnt1;
        wy::Vector2 intPnt2;
        wy::Vector2 intPnt3;
        wy::Vector2 intPnt4;
        unsigned int num = wy3d::intersectEllipseArcEllipseArc(
            pEllipseArcImpl1->getCenter(), pEllipseArcImpl1->getMajorAxis(), pEllipseArcImpl1->getRadiusRatio(), pEllipseArcImpl1->getStartAngle(), pEllipseArcImpl1->getEndAngle(),
            pEllipseArcImpl2->getCenter(), pEllipseArcImpl2->getMajorAxis(), pEllipseArcImpl2->getRadiusRatio(), pEllipseArcImpl2->getStartAngle(), pEllipseArcImpl2->getEndAngle(),
            intPnt1, intPnt2, intPnt3, intPnt4);
        assert(num >= 0 && num <= 4);
        if (num >= 1) outIntPnts.emplace_back(intPnt1);
        if (num >= 2) outIntPnts.emplace_back(intPnt2);
        if (num >= 3) outIntPnts.emplace_back(intPnt3);
        if (num >= 4) outIntPnts.emplace_back(intPnt4);
        return num;
    }

    inline static unsigned int intersect(const SketchEllipseArc* pEllipseArcImpl, const SketchSpline* pSplineImpl, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pEllipseArcImpl);
        assert(pSplineImpl);
        return wy3d::intersectEllipseArcSpline(pEllipseArcImpl->getCenter(), pEllipseArcImpl->getMajorAxis(), pEllipseArcImpl->getRadiusRatio(),
            pEllipseArcImpl->getStartAngle(), pEllipseArcImpl->getEndAngle(),
            pSplineImpl->getOccSpline(), outIntPnts);
    }

    inline static unsigned int intersect(const SketchSpline* pSplineImpl1, const SketchSpline* pSplineImpl2, std::vector<wy::Vector2>& outIntPnts)
    {
        assert(pSplineImpl1);
        assert(pSplineImpl2);
        return wy3d::intersectSplineSpline(pSplineImpl1->getOccSpline(), pSplineImpl2->getOccSpline(), outIntPnts);
    }
};

NS_WY3D_END

#endif // WY3D_SKETCH_CURVE_INTERSECT_UTIL_H