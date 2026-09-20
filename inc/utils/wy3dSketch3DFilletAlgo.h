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

#ifndef WY3D_SKETCH3D_FILLET_ALGO_H
#define WY3D_SKETCH3D_FILLET_ALGO_H

#include <Geom2d_BSplineCurve.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class SketchCurve3D;

// The plane a pair of 3D sketch curves is solved in, as a right-handed frame:
//
//     normal == xDir.cross(yDir)        equivalently   yDir == normal.cross(xDir)
//
// That is the convention every 3D conic entity already evaluates in - Sketch3DCurveParam's
// computeFrame states it for the parameter side, and each entity's getPointAt is written to it. It
// is what makes the angles of a 2D solution reusable as they are: SketchFilletAlgo measures every
// centre and tangent angle from its own X axis, this frame's xDir *is* that X axis, and
// SketchArc3D::create evaluates center + xDir*r*cos(a) + (normal x xDir)*r*sin(a). So an angle
// crosses the boundary in neither direction with any conversion applied to it.
struct WY3D_EXPORT Sketch3DFrame
{
    wy::Vector3 origin;
    wy::Vector3 xDir;
    wy::Vector3 yDir;
    wy::Vector3 normal;

    // The plane coordinates of a point, which for a point off the plane is its projection onto it.
    // That is deliberate: a 3D pick lands near the curve rather than on it, and this is what lets a
    // pick be handed in as it is.
    wy::Vector2 to2D(const wy::Vector3& pnt) const;
    wy::Vector3 to3D(const wy::Vector2& pnt) const;

    // How far a conic's own xDir is rotated from this frame's, measured from xDir towards yDir and
    // normalized to [0, 2PI). A conic stores its angles against its own xDir, so a caller adds this
    // to read them in the frame's terms, and subtracts nothing on the way back: what comes out of
    // the 2D algorithm for an arc is a sweep ratio, which the shift leaves alone.
    double deltaOf(const wy::Vector3& xDirOfConic) const;
};

// One curve as the 2D algorithm sees it. Only the members of the matching kind carry anything.
struct Sketch3DProjectedCurve
{
    enum class Kind
    {
        Line,
        Circle,
        Arc,
        Spline,
    };

    Kind kind = Kind::Line;

    // Line
    wy::Vector2 lineStart;
    wy::Vector2 lineEnd;

    // Circle and arc
    wy::Vector2 center;
    double radius = 0.0;
    double startAngle = 0.0;  // arc only, already carried into the frame by deltaOf
    double endAngle = 0.0;    // arc only, already carried into the frame by deltaOf

    // Spline. The poles are projected and everything else is kept: an isometry commutes with
    // B-spline evaluation, so knots, multiplicities and degree pass through untouched, and the
    // normalized [0,1] parameter comes out the same on both sides.
    Handle(Geom2d_BSplineCurve) pBSpline;
};

// What a fillet between two 3D sketch curves came to. Every parameter is normalized to [0,1] over
// the curve's own extent, as SketchFilletData's are, and may fall outside that range where the
// curve has to be extended rather than trimmed.
struct Sketch3DFilletData
{
    // first curve
    double startParam1st;
    double endParam1st;

    // second curve
    double startParam2nd;
    double endParam2nd;

    // Fillet centre, already carried back into space
    wy::Vector3 filletCenter;
    double filletRadius;
    double filletStartAngle;
    double filletEndAngle;

    // The plane the fillet was solved in. The command layer needs it both to build the arc entity
    // and to sample the preview, and it is the one part of the answer with no 2D counterpart.
    Sketch3DFrame frame;

    // Only the parameters move: the centre, the radius and the two angles say nothing about which
    // curve is the first, so an answer solved the other way round needs nothing else put right.
    void swap();
};

// The 3D counterpart of SketchFilletAlgo, which solves the pair once it is flat.
//
// Nothing here decides whether a fillet exists: that is the 2D algorithm's answer, and this class
// exists to hand it a faithful planar picture of the pair and to carry the answer back out. The one
// judgement it does make is coplanarity, because a fillet between 3D curves is not defined without
// it - two curves in different planes have no single plane to sweep an arc in, and no plane for the
// result to be reported in.
class WY3D_EXPORT Sketch3DFilletAlgo
{
public:
    enum class Result
    {
        Ok = 0,
        UnsupportedPair,  // an ellipse or an ellipse arc, which the 2D algorithm refuses too
        SameEntity,       // the same curve in both slots
        DegenerateInput,  // a null curve, or one whose geometry is unusable
        NotCoplanar,      // the two do not lie in one plane
        NoSolution,       // coplanar and supported, but no fillet of that radius fits
    };

    // The frame the pair is solved in, picked from the kinds involved:
    //
    //   line x line        origin at the first line's start, xDir along it, normal across the two
    //   line x conic       the conic's own plane
    //   line x spline      the spline's poles
    //   conic x conic      the first conic's own plane
    //   conic x spline     the conic's own plane
    //   spline x spline    the first spline's poles
    //
    // Coplanarity is tested against whichever plane was chosen, so the call is also the refusal.
    static Result computeFrame(const SketchCurve3D* pCurve1st, const SketchCurve3D* pCurve2nd,
        Sketch3DFrame& outFrame);

    // pCurve in the frame's coordinates. An arc's angles are carried into the frame here.
    static Result projectCurve(const SketchCurve3D* pCurve, const Sketch3DFrame& frame,
        Sketch3DProjectedCurve& outProjected);

    // The fillet between two curves, at radius R, with a pick on each saying which side of their
    // meeting the fillet belongs on. The picks need not lie on their curves.
    //
    // This is the pair the 2D algorithm solves, plus the two things that are about being in space:
    // the plane comes first and is what makes the pair solvable at all, and the answer's centre is
    // carried back out of it. Whether a fillet of that radius exists is left entirely to the 2D
    // algorithm - a result of NoSolution here means it declined, and the two refusals above it
    // (NotCoplanar, UnsupportedPair) mean it was never asked.
    static Result fillet(double R, double tol,
        const SketchCurve3D* pCurve1st, const wy::Vector3& pickPos1st,
        const SketchCurve3D* pCurve2nd, const wy::Vector3& pickPos2nd,
        Sketch3DFilletData& outData);

    // The tolerance coplanarity is judged by. Wider than the tolerances the parameter arithmetic
    // uses because its only job is to certify that one plane exists: it is a yes-or-no question
    // about the input, not a measurement of the answer.
    static constexpr double kCoplanarTol = 1e-6;

    // How far two directions may be from parallel before the supports count as meeting in a point.
    // The closed-form line-line solver inside Sketch3DCurveIntersectionUtil works on squared sines
    // against wy3d::EPS, which is far looser than anything the frame selection wants.
    static constexpr double kParallelSinTol = 1e-6;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_FILLET_ALGO_H
