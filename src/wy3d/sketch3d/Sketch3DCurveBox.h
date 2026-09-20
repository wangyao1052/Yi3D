///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3D_SKETCH3D_CURVE_BOX_H
#define WY3D_SKETCH3D_CURVE_BOX_H

// Internal to the wy3d library: the trim and extend graphs both need a bounding box per 3D sketch
// curve, and a 3D curve has no getBoundingBox of its own the way the 2D ones do.

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>

#include <wyVector3.h>
#include <wy3dDefs.h>

#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG
namespace detail
{

// An axis-aligned box, or empty when nothing has been expanded into it yet.
class CurveBox3
{
public:
    CurveBox3() : min{DBL_MAX, DBL_MAX, DBL_MAX}, max{-DBL_MAX, -DBL_MAX, -DBL_MAX} {}

    bool isEmpty() const
    {
        return min[0] > max[0] || min[1] > max[1] || min[2] > max[2];
    }

    void expand(const wy::Vector3& pnt)
    {
        min[0] = std::min(min[0], pnt.x());
        min[1] = std::min(min[1], pnt.y());
        min[2] = std::min(min[2], pnt.z());
        max[0] = std::max(max[0], pnt.x());
        max[1] = std::max(max[1], pnt.y());
        max[2] = std::max(max[2], pnt.z());
    }

    // The exact extent of an ellipse swept about its own two axes: |x|*a + |y|*b component by
    // component, rather than a box around the whole circle of the major radius.
    void expandEllipse(const wy::Vector3& center, const wy::Vector3& xDir, double xRadius,
        const wy::Vector3& yDir, double yRadius)
    {
        wy::Vector3 half;
        half.set(std::fabs(xDir.x()) * xRadius + std::fabs(yDir.x()) * yRadius,
            std::fabs(xDir.y()) * xRadius + std::fabs(yDir.y()) * yRadius,
            std::fabs(xDir.z()) * xRadius + std::fabs(yDir.z()) * yRadius);
        expand(center - half);
        expand(center + half);
    }

    void inflate(double amount)
    {
        for (int i = 0; i < 3; ++i)
        {
            min[i] -= amount;
            max[i] += amount;
        }
    }

    // Conservative: true whenever the two could still touch.
    bool overlaps(const CurveBox3& other) const
    {
        for (int i = 0; i < 3; ++i)
        {
            if (min[i] > other.max[i] || other.min[i] > max[i])
            {
                return false;
            }
        }
        return true;
    }

    double diagonal() const
    {
        if (this->isEmpty())
        {
            return 0.0;
        }
        return wy::Vector3(max[0] - min[0], max[1] - min[1], max[2] - min[2]).length();
    }

public:
    double min[3];
    double max[3];
};

// The curve's own extent. False for an entity type this does not know about.
inline bool computeCurveBox3(const SketchCurve3D* pCurve, CurveBox3& outBox)
{
    wy::Vector3 yDir;

    if (const SketchLine3D* pLine = SketchLine3D::cast(pCurve))
    {
        outBox.expand(pLine->getStartPoint());
        outBox.expand(pLine->getEndPoint());
        return true;
    }
    else if (const SketchCircle3D* pCircle = SketchCircle3D::cast(pCurve))
    {
        yDir = pCircle->getNormal().cross(pCircle->getXDir());
        outBox.expandEllipse(pCircle->getCenter(), pCircle->getXDir(), pCircle->getRadius(), yDir,
            pCircle->getRadius());
        return true;
    }
    else if (const SketchArc3D* pArc = SketchArc3D::cast(pCurve))
    {
        yDir = pArc->getNormal().cross(pArc->getXDir());
        // The whole circle, which is a superset of the sweep and is also the arc's extension.
        outBox.expandEllipse(pArc->getCenter(), pArc->getXDir(), pArc->getRadius(), yDir, pArc->getRadius());
        return true;
    }
    else if (const SketchEllipse3D* pEllipse = SketchEllipse3D::cast(pCurve))
    {
        yDir = pEllipse->getNormal().cross(pEllipse->getXDir());
        outBox.expandEllipse(pEllipse->getCenter(), pEllipse->getXDir(), pEllipse->getMajorRadius(), yDir,
            pEllipse->getMinorRadius());
        return true;
    }
    else if (const SketchEllipseArc3D* pEllipseArc = SketchEllipseArc3D::cast(pCurve))
    {
        yDir = pEllipseArc->getNormal().cross(pEllipseArc->getXDir());
        outBox.expandEllipse(pEllipseArc->getCenter(), pEllipseArc->getXDir(), pEllipseArc->getMajorRadius(),
            yDir, pEllipseArc->getMinorRadius());
        return true;
    }
    else if (const SketchSpline3D* pSpline = SketchSpline3D::cast(pCurve))
    {
        constexpr int kSamples = 32;
        for (int i = 0; i <= kSamples; ++i)
        {
            outBox.expand(pSpline->getPointAt(static_cast<double>(i) / kSamples));
        }
        return true;
    }

    assert(false);
    return false;
}

} // namespace detail
NS_WY3D_END

#endif // WY3D_SKETCH3D_CURVE_BOX_H
