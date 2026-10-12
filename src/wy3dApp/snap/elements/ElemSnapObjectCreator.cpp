///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
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

#include "ElemSnapObjectCreator.h"
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <wydbDatabase.h>
#include <wy3dSketch.h>
#include "snap/SnapObject.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> ElemSnapObjectCreator::newSnapObjects(const wydb::ElementId& id, const TopoDS_Shape& shape) const
{
    try
    {
        std::list<wyap::SnapObjectSPtr> snapPoints;
        if (shape.IsNull())
        {
            return snapPoints;
        }

        TopTools_IndexedMapOfShape indexMapOfVertex;
        TopExp::MapShapes(shape, TopAbs_VERTEX, indexMapOfVertex);
        for (int i = 1; i <= indexMapOfVertex.Extent(); ++i)
        {
            const TopoDS_Shape& shape = indexMapOfVertex.FindKey(i);
            TopoDS_Vertex vertex = TopoDS::Vertex(shape);
            gp_Pnt pnt = BRep_Tool::Pnt(vertex);
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, pnt.X(), pnt.Y(), pnt.Z()));
        }

        TopTools_IndexedMapOfShape indexMapOfEdge;
        TopExp::MapShapes(shape, TopAbs_EDGE, indexMapOfEdge);
        for (int i = 1; i <= indexMapOfEdge.Extent(); ++i)
        {
            const TopoDS_Shape& shape = indexMapOfEdge.FindKey(i);
            TopoDS_Edge edge = TopoDS::Edge(shape);

            BRepAdaptor_Curve curveAdaptor(edge);
            if (curveAdaptor.GetType() == GeomAbs_Circle)
            {
                gp_Circ circle = curveAdaptor.Circle();
                const gp_Pnt& center = circle.Location();
                double paramRange = std::fabs(curveAdaptor.LastParameter() - curveAdaptor.FirstParameter());

                // >=180度
                if (paramRange >= wy3d::PI - wy3d::EPS)
                {
                    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, center.X(), center.Y(), center.Z()));
                }

                // 完整圆
                if (paramRange >= wy3d::TWO_PI - wy3d::EPS)
                {
                    double radius = circle.Radius();
                    const gp_Ax2& localAxes = circle.Position();
                    const gp_Vec& xDir = localAxes.XDirection();
                    const gp_Vec& yDir = localAxes.YDirection();

                    // 1. X轴正方向
                    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id,
                        center.X() + radius * xDir.X(),
                        center.Y() + radius * xDir.Y(),
                        center.Z() + radius * xDir.Z()));

                    // 2. Y轴正方向
                    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id,
                        center.X() + radius * yDir.X(),
                        center.Y() + radius * yDir.Y(),
                        center.Z() + radius * yDir.Z()));

                    // 3. X轴负方向
                    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id,
                        center.X() - radius * xDir.X(),
                        center.Y() - radius * xDir.Y(),
                        center.Z() - radius * xDir.Z()));

                    // 4. Y轴负方向
                    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id,
                        center.X() - radius * yDir.X(),
                        center.Y() - radius * yDir.Y(),
                        center.Z() - radius * yDir.Z()));
                }
            }
        }
        return snapPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }
}

bool ElemSnapObjectCreator::getSketchPlane(const wy3d::SketchEntity* pSketchEntity, wy3d::Plane& outSketchPlane) const
{
    assert(pSketchEntity);
    const wydb::Database* pDb = pSketchEntity->getDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    wydb::ElementId sketchId = pSketchEntity->getParent();
    const wydb::Element* pElem = pDb->getElement(sketchId);
    if (!pElem)
    {
        assert(false);
        return false;
    }
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch)
    {
        assert(false);
        return false;
    }

    const wy3d::Plane& sketchPlane = pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        return false;
    }

    outSketchPlane = sketchPlane;
    return true;
}

}
