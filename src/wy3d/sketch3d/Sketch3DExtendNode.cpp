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

#include <utils/wy3dSketch3DExtendNode.h>
#include <utils/wy3dSketch3DCurveParam.h>
#include <utils/wy3dSketch3DExtendGraph.h>

#include <algorithm>
#include <cassert>

#include <wy3dDatabase.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG

Sketch3DExtendNode::Sketch3DExtendNode(const wydb::ElementId& id) : _id(id), _isClosed(false)
{
}

void Sketch3DExtendNode::appendKnot(const wy::Vector3& pos, const wydb::ElementId& otherOwner)
{
    _knots.emplace_back(Sketch3DExtendKnot(pos, DBL_MAX, otherOwner));
}

void Sketch3DExtendNode::appendKnot(const Sketch3DExtendKnot& knot)
{
    _knots.emplace_back(knot);
}

double Sketch3DExtendNode::getParam(const wydb::Database& db, const wy::Vector3& pos) const
{
    const SketchCurve3D* pCurve = SketchCurve3D::cast(db.getElement(_id));
    if (!pCurve || pCurve->isErased())
    {
        return DBL_MAX;
    }

    if (const SketchLine3D* pLine = SketchLine3D::cast(pCurve))
    {
        return Sketch3DCurveParam::getParamOfLine(pLine, pos);
    }
    else if (const SketchCircle3D* pCircle = SketchCircle3D::cast(pCurve))
    {
        return Sketch3DCurveParam::getParamOfCircle(pCircle, pos);
    }
    else if (const SketchArc3D* pArc = SketchArc3D::cast(pCurve))
    {
        return Sketch3DCurveParam::getParamOfArc(pArc, pos);
    }
    else if (const SketchEllipse3D* pEllipse = SketchEllipse3D::cast(pCurve))
    {
        return Sketch3DCurveParam::getParamOfEllipse(pEllipse, pos);
    }
    else if (const SketchEllipseArc3D* pEllipseArc = SketchEllipseArc3D::cast(pCurve))
    {
        return Sketch3DCurveParam::getParamOfEllipseArc(pEllipseArc, pos);
    }
    else if (const SketchSpline3D* pSpline = SketchSpline3D::cast(pCurve))
    {
        return Sketch3DCurveParam::getParamOfSpline(pSpline, pos);
    }
    else
    {
        assert(false);
        return DBL_MAX;
    }
}

bool Sketch3DExtendNode::isOnCurve(const wydb::Database& db, const wy::Vector3& pos) const
{
    const double param = this->getParam(db, pos);
    return param >= 0.0 && param <= 1.0;
}

void Sketch3DExtendNode::refresh(const wydb::Database* pDb)
{
    assert(pDb);

    // No snapping to the ends here: a knot just past an end is exactly the one an extend wants,
    // and round-tripping it through a trim's tolerance would erase the overshoot it was kept for.
    for (Sketch3DExtendKnot& knot : _knots)
    {
        knot.setParam(this->getParam(*pDb, knot.getPosition()));
    }

    // A curve that cannot answer for a point at all - a zero sweep arc, say - leaves the sentinel in
    // the knot, and nothing has to sweep those up: Sketch3DExtendKnot::isValid refuses the sentinel
    // and Sketch3DExtendSegment::isValid refuses the knot, so a curve that answers with none of them
    // can never come back as a usable segment.
    std::sort(_knots.begin(), _knots.end(), [](const Sketch3DExtendKnot& lhs, const Sketch3DExtendKnot& rhs) {
        return lhs.getParam() < rhs.getParam(); });
}

bool Sketch3DExtendNode::isOnOtherCurve(const wydb::Database& db, Sketch3DExtendGraph* pGraph,
    const Sketch3DExtendKnot& knot) const
{
    assert(pGraph);
    Sketch3DExtendNodeSPtr pOtherNode = pGraph->getNode(knot.getOtherOwner());
    if (!pOtherNode)
    {
        assert(false);
        return false;
    }

    // A closed curve covers its own seam from every side, so any knot on it counts.
    if (pOtherNode->isClosed())
    {
        return true;
    }
    else
    {
        return pOtherNode->isOnCurve(db, knot.getPosition());
    }
}

Sketch3DExtendSegment Sketch3DExtendNode::pick(Sketch3DExtendGraph* pGraph, const wy::Vector3& pos, double tol)
{
    assert(pGraph);
    Sketch3DExtendSegment segment; // invalid until filled in

    if (this->isClosed())
    {
        return segment;
    }

    const wydb::Database* pDb = pGraph->getDatabase();
    if (!pDb)
    {
        assert(false);
        return segment;
    }

    const SketchCurve3D* pCurve = SketchCurve3D::cast(pDb->getElement(_id));
    if (!pCurve || pCurve->isErased() || pCurve->isClosed())
    {
        assert(false);
        return segment;
    }

    // Can this curve reach anything at all? Every knot off the curve has to still be answered for
    // by the curve it was found against.
    bool extendable = false;
    for (const Sketch3DExtendKnot& knot : _knots)
    {
        if (knot.getParam() >= -wy3d::EPS && knot.getParam() <= 1.0 + wy3d::EPS)
        {
            continue;
        }

        if (this->isOnOtherCurve(*pDb, pGraph, knot))
        {
            extendable = true;
            break;
        }
    }

    if (!extendable)
    {
        return segment;
    }

    // Which end the cursor is nearer decides the direction.
    double pickParam = 0.0;
    if (const SketchLine3D* pLine = SketchLine3D::cast(pCurve))
    {
        pickParam = Sketch3DCurveParam::getParamOfLine(pLine, pos);
    }
    else if (const SketchArc3D* pArc = SketchArc3D::cast(pCurve))
    {
        pickParam = Sketch3DCurveParam::getParamOfArc(pArc, pos);
    }
    else if (const SketchEllipseArc3D* pEllipseArc = SketchEllipseArc3D::cast(pCurve))
    {
        pickParam = Sketch3DCurveParam::getParamOfEllipseArc(pEllipseArc, pos);
    }
    else if (const SketchSpline3D* pSpline = SketchSpline3D::cast(pCurve))
    {
        pickParam = Sketch3DCurveParam::getPickParamOfSpline(*pSpline, pos);
    }
    else
    {
        assert(false);
        return segment;
    }

    const bool forward = (pickParam >= 0.5);

    // Until a reachable knot is found the answer is the whole curve, which the command reads as
    // "nothing to do".
    segment.startKnot.setParam(0.0);
    segment.startKnot.setPosition(pCurve->getStartPoint());
    segment.endKnot.setParam(1.0);
    segment.endKnot.setPosition(pCurve->getEndPoint());

    if (SketchLine3D::cast(pCurve) || SketchSpline3D::cast(pCurve))
    {
        // A line leaves along its own direction and a spline along its end tangent, so the nearest
        // knot past the chosen end is the one to stop at.
        if (forward)
        {
            for (const Sketch3DExtendKnot& knot : _knots)
            {
                if (knot.getParam() > 1.0 + wy3d::EPS && this->isOnOtherCurve(*pDb, pGraph, knot))
                {
                    segment.endKnot = knot;
                    break;
                }
            }
        }
        else
        {
            for (auto riter = _knots.crbegin(); riter != _knots.crend(); ++riter)
            {
                if (riter->getParam() < -wy3d::EPS && this->isOnOtherCurve(*pDb, pGraph, *riter))
                {
                    segment.startKnot = *riter;
                    break;
                }
            }
        }
    }
    else // an arc or an ellipse arc, which grows around its own circle
    {
        double startAngle = 0.0;
        double endAngle = 0.0;
        double totalAngle = 0.0;
        if (const SketchArc3D* pArc = SketchArc3D::cast(pCurve))
        {
            startAngle = pArc->getStartAngle();
            endAngle = pArc->getEndAngle();
            totalAngle = pArc->getTotalAngle();
        }
        else if (const SketchEllipseArc3D* pEllipseArc = SketchEllipseArc3D::cast(pCurve))
        {
            startAngle = pEllipseArc->getStartAngle();
            endAngle = pEllipseArc->getEndAngle();
            totalAngle = pEllipseArc->getTotalAngle();
        }
        else
        {
            assert(false);
            return segment;
        }

        startAngle = wy3d::normalizeRadian(startAngle);
        endAngle = wy3d::normalizeRadian(endAngle);

        // A knot's parameter is read back as an angle, which sidesteps the question of which way
        // round it was stored. Past the end is measured forward from endAngle, before the start
        // backward from startAngle, and the nearer of the two wins.
        double minDisAngle = DBL_MAX;
        std::size_t minDisIndex = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i < _knots.size(); ++i)
        {
            const double param = _knots[i].getParam();
            if (param >= -wy3d::EPS && param <= 1.0 + wy3d::EPS) continue;

            double curAngle = wy3d::normalizeRadian(startAngle + param * totalAngle);
            if (forward)
            {
                if (curAngle < endAngle) curAngle += wy3d::TWO_PI;
                if ((curAngle - endAngle) < minDisAngle && this->isOnOtherCurve(*pDb, pGraph, _knots[i]))
                {
                    minDisAngle = curAngle - endAngle;
                    minDisIndex = i;
                }
            }
            else
            {
                if (curAngle > startAngle) curAngle -= wy3d::TWO_PI;
                if ((startAngle - curAngle) < minDisAngle && this->isOnOtherCurve(*pDb, pGraph, _knots[i]))
                {
                    minDisAngle = startAngle - curAngle;
                    minDisIndex = i;
                }
            }
        }

        if (static_cast<std::size_t>(-1) != minDisIndex)
        {
            if (forward)
            {
                segment.endKnot = _knots[minDisIndex];
            }
            else
            {
                segment.startKnot = _knots[minDisIndex];
            }
        }
    }

    return segment;
}

NS_WY3D_END
