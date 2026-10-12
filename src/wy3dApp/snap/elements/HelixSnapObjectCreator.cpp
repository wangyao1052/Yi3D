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

#include "HelixSnapObjectCreator.h"
#include <cassert>

#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>

#include <wy3dHelix.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> HelixSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Helix* pHelix = wy3d::Helix::cast(pElem);
    if (!pHelix)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }    if (!pHelix)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    TopoDS_Edge edge = pHelix->getEdge();
    if (edge.IsNull())
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    try
    {
        std::list<wyap::SnapObjectSPtr> snapPoints;

        TopTools_IndexedMapOfShape indexMapOfVertex;
        TopExp::MapShapes(edge, TopAbs_VERTEX, indexMapOfVertex);
        for (int i = 1; i <= indexMapOfVertex.Extent(); ++i)
        {
            const TopoDS_Shape& shape = indexMapOfVertex.FindKey(i);
            TopoDS_Vertex vertex = TopoDS::Vertex(shape);
            gp_Pnt pnt = BRep_Tool::Pnt(vertex);
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pHelix->getId(), pnt.X(), pnt.Y(), pnt.Z()));
        }

        return snapPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }
}

}
