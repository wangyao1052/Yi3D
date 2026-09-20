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

#include <utils/wy3dSketch3DTrimNode.h>
#include <utils/wy3dSketch3DCurveParam.h>
#include <utils/wy3dSketch3DTrimGraph.h>

#include <algorithm>
#include <cassert>

#include <wy3dDatabase.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG

namespace
{
constexpr double kTol = 1e-7;

// A parameter a knot may hold and still be worth considering. Anything else was written before a
// trim and no longer describes a point on the curve.
bool isInValidRange(double t)
{
    return t >= -kTol && t <= 1.0 + kTol;
}
} // namespace

Sketch3DTrimNode::Sketch3DTrimNode(const wydb::ElementId& id) : _id(id)
{
}

std::shared_ptr<Sketch3DTrimNode> Sketch3DTrimNode::clone(const wydb::ElementId& id)
{
    Sketch3DTrimNodeSPtr pCopy = std::make_shared<Sketch3DTrimNode>(id);
    pCopy->_knots = _knots;
    return pCopy;
}

void Sketch3DTrimNode::appendKnot(const wy::Vector3& pos, const wydb::ElementId& otherOwner)
{
    _knots.emplace_back(Sketch3DTrimKnot(pos, DBL_MAX, otherOwner));
}

void Sketch3DTrimNode::appendKnot(const Sketch3DTrimKnot& knot)
{
    _knots.emplace_back(knot);
}

void Sketch3DTrimNode::appendChild(std::shared_ptr<Sketch3DTrimNode> pChild)
{
    _children.emplace_back(pChild);
}

double Sketch3DTrimNode::getParam(const wydb::Database* pDb, const wy::Vector3& pos) const
{
    assert(pDb);

    // The curve may have been erased since this node was built.
    const SketchCurve3D* pCurve = SketchCurve3D::cast(pDb->getElement(_id));
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

void Sketch3DTrimNode::refresh(const wydb::Database* pDb)
{
    assert(pDb);

    const SketchCurve3D* pCurve = SketchCurve3D::cast(pDb->getElement(_id));

    // An arc's parameter is allowed to run past its sweep - that is a knot the arc could be
    // extended to - so reviseT brings a knot just outside the ends back onto them. On a closed
    // curve there is no such outside, and snapping would be wrong.
    const bool isArc = SketchArc3D::cast(pCurve) != nullptr || SketchEllipseArc3D::cast(pCurve) != nullptr;

    for (Sketch3DTrimKnot& knot : _knots)
    {
        double t = this->getParam(pDb, knot.getPosition());
        if (isArc && t != DBL_MAX)
        {
            t = Sketch3DCurveParam::reviseT(t);
        }
        knot.setParam(t);
    }

    // The pick below walks the knots in parameter order and relies on it.
    std::sort(_knots.begin(), _knots.end(), [](const Sketch3DTrimKnot& lhs, const Sketch3DTrimKnot& rhs) {
        return lhs.getParam() < rhs.getParam(); });
}

Sketch3DTrimSegment Sketch3DTrimNode::pick(Sketch3DTrimGraph* pGraph, const wy::Vector3& position, double tol)
{
    assert(pGraph);
    Sketch3DTrimSegment segment; // invalid until filled in

    const wydb::Database* pDb = pGraph->getDatabase();
    if (!pDb)
    {
        assert(false);
        return segment;
    }

    const SketchCurve3D* pCurve = SketchCurve3D::cast(pDb->getElement(_id));
    if (!pCurve || pCurve->isErased())
    {
        assert(false);
        return segment;
    }

    // First pass: how many distinct crossings does this curve still carry? Too few means the pick
    // is asking to move the whole curve, not to cut a piece out of it.
    bool isWholeCurve(false);
    if (pCurve->isClosed())
    {
        std::size_t num = 0;
        std::size_t lastIdx = static_cast<std::size_t>(-1);
        double firstParam = DBL_MAX;
        for (std::size_t i = 0; i < _knots.size(); ++i)
        {
            const double t = _knots[i].getParam();
            if (!isInValidRange(t))
            {
                continue;
            }

            if (Sketch3DTrimNodeSPtr pOtherNode = pGraph->getNode(_knots[i].getOtherOwner()))
            {
                if (!pOtherNode->isValidKnotPosition(pDb, _knots[i].getPosition())) continue;
            }

            if (static_cast<std::size_t>(-1) == lastIdx)
            {
                ++num;
                lastIdx = i;
                firstParam = t;
            }
            else if (std::fabs(t - _knots[lastIdx].getParam()) > tol)
            {
                // On a closed curve the last crossing and the first may be the same one seen from
                // either side of the seam.
                if (t > 0.99 && firstParam < 0.01)
                {
                    if (std::fabs(t - 1.0 - firstParam) > tol)
                    {
                        ++num;
                        lastIdx = i;
                    }
                }
                else
                {
                    ++num;
                    lastIdx = i;
                }
            }
        }

        isWholeCurve = (num <= 1);
    }
    else
    {
        assert(_knots.size() >= 2); // the ends were added when the graph was built
        std::size_t num = 0;
        std::size_t lastIdx = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i < _knots.size(); ++i)
        {
            const double t = _knots[i].getParam();
            if (!isInValidRange(t))
            {
                continue;
            }

            // The ends are not crossings, so they are never asked to vouch for themselves.
            if (t != 0.0 && t != 1.0)
            {
                if (Sketch3DTrimNodeSPtr pOtherNode = pGraph->getNode(_knots[i].getOtherOwner()))
                {
                    if (!pOtherNode->isValidKnotPosition(pDb, _knots[i].getPosition())) continue;
                }
            }

            if (static_cast<std::size_t>(-1) == lastIdx)
            {
                ++num;
                lastIdx = i;
            }
            else if (std::fabs(t - _knots[lastIdx].getParam()) > tol)
            {
                ++num;
            }
        }

        // An open curve carries its two ends on top of the crossings, so two means none.
        isWholeCurve = (num <= 2);
    }

    if (isWholeCurve)
    {
        segment.startKnot.setParam(0.0);
        segment.startKnot.setPosition(pCurve->getStartPoint());
        segment.endKnot.setParam(1.0);
        segment.endKnot.setPosition(pCurve->getEndPoint());
        return segment;
    }

    // The pick point's parameter, so the loop below can tell which side of it each knot falls on.
    double t = 0.0;
    if (const SketchLine3D* pLine = SketchLine3D::cast(pCurve))
    {
        t = Sketch3DCurveParam::getParamOfLine(pLine, position);
    }
    else if (const SketchCircle3D* pCircle = SketchCircle3D::cast(pCurve))
    {
        t = Sketch3DCurveParam::getParamOfCircle(pCircle, position);
    }
    else if (const SketchArc3D* pArc = SketchArc3D::cast(pCurve))
    {
        t = Sketch3DCurveParam::getParamOfArc(pArc, position);
    }
    else if (const SketchEllipse3D* pEllipse = SketchEllipse3D::cast(pCurve))
    {
        t = Sketch3DCurveParam::getParamOfEllipse(pEllipse, position);
    }
    else if (const SketchEllipseArc3D* pEllipseArc = SketchEllipseArc3D::cast(pCurve))
    {
        t = Sketch3DCurveParam::getParamOfEllipseArc(pEllipseArc, position);
    }
    else if (const SketchSpline3D* pSpline = SketchSpline3D::cast(pCurve))
    {
        t = Sketch3DCurveParam::getPickParamOfSpline(*pSpline, position);
    }
    else
    {
        assert(false);
        return segment;
    }

    // The two knots bracketing the pick point.
    std::size_t theFirstIndex = static_cast<std::size_t>(-1);
    std::size_t theLastIndex = static_cast<std::size_t>(-1);
    std::size_t startIndex = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < _knots.size(); ++i)
    {
        const double currParam = _knots[i].getParam();
        if (!isInValidRange(currParam)) continue;

        if (currParam != 0.0 && currParam != 1.0)
        {
            Sketch3DTrimNodeSPtr pOtherOwner = pGraph->getNode(_knots[i].getOtherOwner());
            if (pOtherOwner && !pOtherOwner->isValidKnotPosition(pDb, _knots[i].getPosition()))
            {
                continue;
            }
        }

        if (static_cast<std::size_t>(-1) == theFirstIndex) theFirstIndex = i;
        theLastIndex = i;

        if (static_cast<std::size_t>(-1) == startIndex)
        {
            if (t >= currParam)
            {
                segment.startKnot = _knots[i];
                startIndex = i;
            }
        }
        else if (std::fabs(currParam - segment.startKnot.getParam()) > tol) // skip coincident knots
        {
            if (t <= currParam)
            {
                segment.endKnot = _knots[i];
                break;
            }
            else
            {
                segment.startKnot = _knots[i];
                startIndex = i;
            }
        }
    }
    assert(theFirstIndex != static_cast<std::size_t>(-1) && theLastIndex != static_cast<std::size_t>(-1)
        && theFirstIndex != theLastIndex);

    if (pCurve->isClosed())
    {
        // The piece runs off one end of the parameter range and back in at the other.
        if (segment.startKnot.getParam() == DBL_MAX)
        {
            segment.startKnot = _knots[theLastIndex];
        }
        if (segment.endKnot.getParam() == DBL_MAX)
        {
            segment.endKnot = _knots[theFirstIndex];
        }
    }
    else
    {
        assert(segment.startKnot.getParam() != DBL_MAX && segment.endKnot.getParam() != DBL_MAX);
    }

    return segment;
}

bool Sketch3DTrimNode::isValidKnotPosition(const wydb::Database* pDb, const wy::Vector3& pos) const
{
    assert(pDb);

    if (isInValidRange(this->getParam(pDb, pos)))
    {
        return true;
    }

    // A knot left over from before a trim still counts if the piece it was cut down to covers it.
    for (const std::shared_ptr<Sketch3DTrimNode>& pChildNode : _children)
    {
        assert(pChildNode);
        if (pChildNode->isValidKnotPosition(pDb, pos))
        {
            return true;
        }
    }

    return false;
}

NS_WY3D_END
