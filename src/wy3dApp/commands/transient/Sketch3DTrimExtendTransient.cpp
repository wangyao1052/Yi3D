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

#include "Sketch3DTrimExtendTransient.h"

#include <cassert>
#include <cmath>
#include <vector>

#include <osg/LineWidth>
#include <osg/Vec4>

#include <utils/wy3dSketch3DCurveParam.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include "scene/RenderConst.h"

namespace
{
constexpr int kCurveSegments = 64;
constexpr int kOverhangSegments = 8;

// One point of a conic-like entity, in the entity's own frame: yDir = normal x xDir, matching every
// one of the entities' getPointAt.
wy::Vector3 evaluateConic(const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir,
    double xRadius, double yRadius, double angle)
{
    const wy::Vector3 yDir = normal.cross(xDir);
    return center + xDir * (xRadius * std::cos(angle)) + yDir * (yRadius * std::sin(angle));
}

// The polar angle range a conic covers, or false for a curve that is not parameterized by an angle.
// An arc's parameter wraps forward from its start rather than going negative, so a knot behind the
// start arrives above 1 and the two parameters say nothing about which way the piece runs - only
// the angles do, which is what subArcAngles is for.
bool conicAngleRange(const wy3d::SketchCurve3D* pCurve, double startParam, double endParam,
    double& fromAngle, double& toAngle)
{
    double startAngle = 0.0;
    double totalAngle = 0.0;
    if (wy3d::SketchCircle3D::cast(pCurve) || wy3d::SketchEllipse3D::cast(pCurve))
    {
        totalAngle = wy3d::TWO_PI;
    }
    else if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        startAngle = pArc->getStartAngle();
        totalAngle = pArc->getTotalAngle();
    }
    else if (const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pCurve))
    {
        startAngle = pEllipseArc->getStartAngle();
        totalAngle = pEllipseArc->getTotalAngle();
    }
    else
    {
        return false;
    }

    if (totalAngle <= 0.0) return false;

    wy3d::Sketch3DCurveParam::subArcAngles(startAngle, totalAngle, startParam, endParam,
        fromAngle, toAngle);
    return true;
}

// One point of a conic at a polar angle. An ellipse converts to its eccentric anomaly on the way,
// exactly as its own getPointAt does.
wy::Vector3 evaluateConicAt(const wy3d::SketchCurve3D* pCurve, double angle)
{
    if (const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pCurve))
    {
        return evaluateConic(pCircle->getCenter(), pCircle->getNormal(), pCircle->getXDir(),
            pCircle->getRadius(), pCircle->getRadius(), angle);
    }
    else if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        return evaluateConic(pArc->getCenter(), pArc->getNormal(), pArc->getXDir(),
            pArc->getRadius(), pArc->getRadius(), angle);
    }
    else if (const wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pCurve))
    {
        const double major = pEllipse->getMajorRadius();
        const double minor = pEllipse->getMinorRadius();
        return evaluateConic(pEllipse->getCenter(), pEllipse->getNormal(), pEllipse->getXDir(),
            major, minor, wy3d::ellipsePolarAngleToParametricAngle(angle, major, minor));
    }
    else if (const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pCurve))
    {
        const double major = pEllipseArc->getMajorRadius();
        const double minor = pEllipseArc->getMinorRadius();
        return evaluateConic(pEllipseArc->getCenter(), pEllipseArc->getNormal(), pEllipseArc->getXDir(),
            major, minor, wy3d::ellipsePolarAngleToParametricAngle(angle, major, minor));
    }

    assert(false);
    return wy::Vector3::kZero;
}

// A line's or a spline's position at a parameter that is allowed to be outside [0,1].
// SketchCurve3D::getPointAt cannot be used for the overhang: it clamps, and its unclamped form
// asserts on a spline.
wy::Vector3 evaluate(const wy3d::SketchCurve3D* pCurve, double t)
{
    if (const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pCurve))
    {
        const wy::Vector3 startPnt = pLine->getStartPoint();
        return startPnt + (pLine->getEndPoint() - startPnt) * t;
    }
    else if (const wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pCurve))
    {
        // A spline has no support to be evaluated off the end of, so past either end it continues
        // along the end tangent - in world units, which is the convention the extend knot uses too.
        if (t >= 0.0 && t <= 1.0)
        {
            return pSpline->getPointAt(t);
        }

        wy::Vector3 startPnt, startDir, endPnt, endDir;
        if (!wy3d::Sketch3DCurveParam::getBSplineInfo(pSpline->getOccSpline(), startPnt, startDir, endPnt, endDir))
        {
            return pSpline->getPointAt(std::clamp(t, 0.0, 1.0));
        }
        return t < 0.0 ? startPnt + startDir * t : endPnt + endDir * (t - 1.0);
    }

    assert(false);
    return pCurve->getPointAt(std::clamp(t, 0.0, 1.0));
}

// Samples the range the command would act on into a polyline. An overhang is sampled on its own so
// that a long extension does not eat into the resolution of the curve it grows out of.
void linearize(const wy3d::SketchCurve3D* pCurve, double startParam, double endParam,
    std::vector<wy::Vector3>& points)
{
    double fromAngle = 0.0;
    double toAngle = 0.0;
    if (conicAngleRange(pCurve, startParam, endParam, fromAngle, toAngle))
    {
        for (int i = 0; i <= kCurveSegments; ++i)
        {
            points.emplace_back(evaluateConicAt(pCurve,
                fromAngle + (toAngle - fromAngle) * i / kCurveSegments));
        }
        return;
    }

    if (startParam < 0.0)
    {
        for (int i = 0; i < kOverhangSegments; ++i)
        {
            points.emplace_back(evaluate(pCurve,
                startParam + (0.0 - startParam) * i / kOverhangSegments));
        }
    }

    const double innerStart = std::max(startParam, 0.0);
    const double innerEnd = std::min(endParam, 1.0);
    if (innerStart < innerEnd)
    {
        for (int i = 0; i <= kCurveSegments; ++i)
        {
            points.emplace_back(evaluate(pCurve,
                innerStart + (innerEnd - innerStart) * i / kCurveSegments));
        }
    }

    if (endParam > 1.0)
    {
        for (int i = 0; i <= kOverhangSegments; ++i)
        {
            points.emplace_back(evaluate(pCurve, 1.0 + (endParam - 1.0) * i / kOverhangSegments));
        }
    }
}
} // namespace

Sketch3DTrimExtendTransient::Sketch3DTrimExtendTransient(const wy3d::SketchCurve3D* pCurve,
    double startParam, double endParam)
    : GuiCmdTransient(),
      _id(pCurve ? pCurve->getId() : wydb::ElementId::kNull),
      _startParam(startParam),
      _endParam(endParam)
{
    assert(pCurve);
    // No ordering test on the two parameters: an arc's pair runs start-then-end in angle but not in
    // parameter, so only the curve's own kind can say whether the range is empty.
    if (!pCurve || startParam == endParam)
    {
        return;
    }

    std::vector<wy::Vector3> points;
    points.reserve(kOverhangSegments + kCurveSegments + 2);
    linearize(pCurve, startParam, endParam, points);
    if (points.size() < 2)
    {
        return;
    }

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setNodeMask(~PICK_MASK); // the preview must never be picked

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(points.size());
    for (const wy::Vector3& pnt : points)
    {
        vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
    }
    geom->setVertexArray(vertices);

    // Orange, the same as the 2D trim and extend previews.
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);

    std::vector<unsigned int> indices;
    indices.reserve(points.size() * 2);
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        indices.push_back(static_cast<unsigned int>(i));
        indices.push_back(static_cast<unsigned int>(i + 1));
    }
    geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices.cbegin(), indices.cend()));

    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0));

    _root->addChild(geom.get());
}

Sketch3DTrimExtendTransient::~Sketch3DTrimExtendTransient()
{
}
