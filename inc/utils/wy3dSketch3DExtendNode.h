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

#ifndef WY3D_SKETCH3D_EXTEND_NODE_H
#define WY3D_SKETCH3D_EXTEND_NODE_H

#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>

NS_WY3D_BEG

class Sketch3DExtendGraph;
class SketchCurve3D;

// A crossing recorded on a curve that is being extended. Unlike a trim knot, the parameter here is
// deliberately allowed to sit outside [0,1]: that overshoot is the whole point, it is where the
// curve would have to grow to reach the other one.
class WY3D_EXPORT Sketch3DExtendKnot
{
public:
    Sketch3DExtendKnot() : _pos(), _t(DBL_MAX), _otherOwner(wydb::ElementId::kNull) {}
    Sketch3DExtendKnot(const wy::Vector3& pos, double t, wydb::ElementId otherOwner = wydb::ElementId::kNull)
        : _pos(pos), _t(t), _otherOwner(otherOwner) {}

    double getParam() const { return _t; }
    void setParam(double t) { _t = t; }

    const wy::Vector3& getPosition() const { return _pos; }
    void setPosition(const wy::Vector3& pos) { _pos = pos; }

    wydb::ElementId getOtherOwner() const { return _otherOwner; }

    // Any finite parameter is meaningful here, so only the non-finite sentinels are refused.
    bool isValid() const
    {
        return _t != DBL_MAX && _t != -DBL_MAX && !std::isnan(_t) && !std::isinf(_t);
    }

private:
    double _t;
    wy::Vector3 _pos;
    wydb::ElementId _otherOwner;
};

// The stretch of a curve an extend would produce: the curve itself, with one end pushed out to a
// knot on some other curve.
class WY3D_EXPORT Sketch3DExtendSegment
{
public:
    Sketch3DExtendKnot startKnot;
    Sketch3DExtendKnot endKnot;

    bool isValid() const { return startKnot.isValid() && endKnot.isValid(); }
};

class WY3D_EXPORT Sketch3DExtendNode
{
public:
    explicit Sketch3DExtendNode(const wydb::ElementId& id);

    wydb::ElementId getId() const { return _id; }

    // A closed curve has nowhere to grow, so it is never a candidate for extension.
    bool isClosed() const { return _isClosed; }
    void setIsClosed(bool isClosed) { _isClosed = isClosed; }

    void appendKnot(const wy::Vector3& pos, const wydb::ElementId& otherOwner);
    void appendKnot(const Sketch3DExtendKnot& knot);

    void refresh(const wydb::Database* pDb);

    Sketch3DExtendSegment pick(Sketch3DExtendGraph* pGraph, const wy::Vector3& pos, double tol = 1e-6);

private:
    // The parameter of pos on this node's curve, or DBL_MAX when the curve does not answer.
    double getParam(const wydb::Database& db, const wy::Vector3& pos) const;

    // Whether pos lands on the curve proper rather than on its extension.
    bool isOnCurve(const wydb::Database& db, const wy::Vector3& pos) const;

    // Whether the knot's overshoot is still answered for by the curve it came from - a piece of a
    // curve that has since been trimmed away stops being a valid place to extend to.
    bool isOnOtherCurve(const wydb::Database& db, Sketch3DExtendGraph* pGraph,
        const Sketch3DExtendKnot& knot) const;

private:
    wydb::ElementId _id;
    std::vector<Sketch3DExtendKnot> _knots;
    bool _isClosed;
};

typedef std::shared_ptr<Sketch3DExtendNode> Sketch3DExtendNodeSPtr;

NS_WY3D_END

#endif // WY3D_SKETCH3D_EXTEND_NODE_H
