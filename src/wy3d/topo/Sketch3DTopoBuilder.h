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

#ifndef WY3D_SKETCH3D_TOPO_BUILDER_H
#define WY3D_SKETCH3D_TOPO_BUILDER_H

#include <map>

#include <TopoDS_Edge.hxx>
#include <Geom_Curve.hxx>

#include <wy3dDefs.h>

NS_WY3D_BEG

class SketchEntity3D;
class SketchLine3D;
class SketchCircle3D;
class SketchArc3D;
class SketchEllipse3D;
class SketchEllipseArc3D;
class SketchSpline3D;

class WY3D_EXPORT Sketch3DTopoBuilder
{
public:
    explicit Sketch3DTopoBuilder(bool recordTopoHistory = false);

    TopoDS_Edge makeEdge(const wy3d::SketchEntity3D* pEntity);
    TopoDS_Edge makeEdge(const wy3d::SketchLine3D* pLine);
    TopoDS_Edge makeEdge(const wy3d::SketchCircle3D* pCircle);
    TopoDS_Edge makeEdge(const wy3d::SketchArc3D* pArc);
    TopoDS_Edge makeEdge(const wy3d::SketchEllipse3D* pEllipse);
    TopoDS_Edge makeEdge(const wy3d::SketchEllipseArc3D* pEllipseArc);
    TopoDS_Edge makeEdge(const wy3d::SketchSpline3D* pSpline);

    // The entity's supporting curve plus the parameter interval the entity occupies on it.
    // Lines and arcs come back untrimmed with the interval given separately: the intersection
    // code needs the interval, and wrapping them in a Geom_TrimmedCurve would only make it
    // unwrap them again. A null handle means the entity has no usable geometry.
    Handle(Geom_Curve) toGeomCurve(const wy3d::SketchEntity3D* pEntity, double& first, double& last) const;

    // Same curve widened so the entity may reach `reach` past its own ends: a line becomes the
    // segment reaching `reach` beyond each end, an arc or an ellipse arc becomes the whole conic,
    // a circle or an ellipse is unchanged. Widening is clipped rather than infinite so that every
    // operand keeps a finite range - GeomAPI_ExtremaCurveCurve and the bounding boxes both need it.
    // A spline has no widened form: it comes back unchanged and the caller adds the tangent rays.
    Handle(Geom_Curve) toGeomCurveWidened(const wy3d::SketchEntity3D* pEntity, double reach,
        double& first, double& last) const;

    const std::map<Handle(Geom_Curve), unsigned int>& getCurve2IdMap() const
    {
        return _curve2Id;
    }

private:
    TopoDS_Edge makeEdgeFromCurve(const Handle(Geom_Curve)& geomCurve, unsigned int entityId);

private:
    bool _recordTopoHistory;
    std::map<Handle(Geom_Curve), unsigned int> _curve2Id;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_TOPO_BUILDER_H
