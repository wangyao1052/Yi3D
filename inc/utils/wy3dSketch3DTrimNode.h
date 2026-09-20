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

#ifndef WY3D_SKETCH3D_TRIM_NODE_H
#define WY3D_SKETCH3D_TRIM_NODE_H

#include <cfloat>
#include <list>
#include <memory>
#include <vector>

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>

NS_WY3D_BEG

class Sketch3DTrimGraph;
class SketchCurve3D;

// One curve-against-curve crossing kept on a curve, as a position rather than a parameter. The
// position is what survives: a trim rewrites the curve, and every knot is reparameterized against
// whatever the curve looks like now. _t is only a cache of that answer.
class WY3D_EXPORT Sketch3DTrimKnot
{
public:
    Sketch3DTrimKnot() : _pos(), _t(DBL_MAX), _otherOwner(wydb::ElementId::kNull) {}
    Sketch3DTrimKnot(const wy::Vector3& pos, double t, wydb::ElementId otherOwner = wydb::ElementId::kNull)
        : _pos(pos), _t(t), _otherOwner(otherOwner) {}

    double getParam() const { return _t; }
    void setParam(double t) { _t = t; }

    const wy::Vector3& getPosition() const { return _pos; }
    void setPosition(const wy::Vector3& pos) { _pos = pos; }

    // The curve this knot was found against; the same position is a knot on that one too.
    wydb::ElementId getOtherOwner() const { return _otherOwner; }

    // DBL_MAX is the invalid marker, so a knot outside [0,1] is not simply "past the end" - only
    // the refresh that produced it knows whether it means anything.
    bool isValid() const { return _t >= 0.0 && _t <= 1.0; }

private:
    double _t;
    wy::Vector3 _pos;
    wydb::ElementId _otherOwner;
};

// The stretch of a curve between two knots. Both ends are invalid until pick fills them in.
class WY3D_EXPORT Sketch3DTrimSegment
{
public:
    Sketch3DTrimKnot startKnot;
    Sketch3DTrimKnot endKnot;

    bool isValid() const { return startKnot.isValid() && endKnot.isValid(); }
};

class WY3D_EXPORT Sketch3DTrimNode
{
public:
    explicit Sketch3DTrimNode(const wydb::ElementId& id);

    wydb::ElementId getId() const { return _id; }

    // A copy that keeps the knots but starts with no children. Trimming leaves the original node
    // behind as a child of the piece it was cut down to, which is what keeps a later undo able to
    // answer for the knots the cut removed.
    std::shared_ptr<Sketch3DTrimNode> clone(const wydb::ElementId& id);

    void appendKnot(const wy::Vector3& pos, const wydb::ElementId& otherOwner);
    void appendKnot(const Sketch3DTrimKnot& knot);

    void appendChild(std::shared_ptr<Sketch3DTrimNode> pChild);

    // Recomputes every knot's parameter against the curve as it is now. Knots that no longer land
    // on the curve come back invalid.
    void refresh(const wydb::Database* pDb);

    Sketch3DTrimSegment pick(Sketch3DTrimGraph* pGraph, const wy::Vector3& pos, double tol = 1e-6);

private:
    // The position's parameter on this node's own curve, or DBL_MAX when it does not land on it.
    // Every entity type answers through Sketch3DCurveParam, which is why refresh is nearly free.
    double getParam(const wydb::Database* pDb, const wy::Vector3& pos) const;

    bool isValidKnotPosition(const wydb::Database* pDb, const wy::Vector3& pos) const;

private:
    wydb::ElementId _id;
    std::vector<Sketch3DTrimKnot> _knots;
    std::list<std::shared_ptr<Sketch3DTrimNode>> _children;
};

typedef std::shared_ptr<Sketch3DTrimNode> Sketch3DTrimNodeSPtr;

NS_WY3D_END

#endif // WY3D_SKETCH3D_TRIM_NODE_H
