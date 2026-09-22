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

#ifndef WY3DAPP_SOLID_FACE_SEL_FILTER_H
#define WY3DAPP_SOLID_FACE_SEL_FILTER_H

#include <memory>
#include <cassert>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wyapSelection.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dSelectionType.h>
#include "select/SelectFilterFunctor.h"
#include "utils/GuiCommandUtil.h"

template<typename T>
class FaceSelFilterFunctor : public SelectFilterFunctor
{
public:
    inline virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override
    {
        assert(pDb);

        if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Face) return SelectFilterStatus::Continue;
        if (sel.getSubPath().empty()) return SelectFilterStatus::Continue;
        unsigned int faceIndex = std::stoul(sel.getSubPath());
        if (faceIndex == -1) return SelectFilterStatus::Continue;

        const wydb::Element* pElem = pDb->getElement(sel.getElementId());
        if (!pElem) return SelectFilterStatus::Continue;
        if (_excludeIds.find(pElem->getId()) != _excludeIds.cend())
        {
            return SelectFilterStatus::Continue;
        }
        if (!GuiCommandUtil::isTopLevelBody(pElem)) return SelectFilterStatus::Continue;
        TopoDS_Shape shape;
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
        {
            shape = pSolid->getShape();
        }
        else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem))
        {
            shape = pSheet->getShape();
        }
        else
        {
            return SelectFilterStatus::Continue;
        }
        if (shape.IsNull()) return SelectFilterStatus::Continue;

        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
        faceIndex += 1; // OCC中以1为起始序号
        if (faceMap.Size() < faceIndex) return SelectFilterStatus::Continue;
        const TopoDS_Shape& faceShape = faceMap.FindKey(faceIndex);

        TopoDS_Face face = TopoDS::Face(faceShape);
        if (face.IsNull()) return SelectFilterStatus::Continue;
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        Handle(T) derivedSurface = Handle(T)::DownCast(surface);
        if (derivedSurface.IsNull()) return SelectFilterStatus::Continue;

        return SelectFilterStatus::Ok;
    }

    void addExcludeId(const wydb::ElementId& excludeId)
    {
        _excludeIds.insert(excludeId);
    }

private:
    std::set<wydb::ElementId> _excludeIds;
};

#endif // WY3DAPP_SOLID_FACE_SEL_FILTER_H