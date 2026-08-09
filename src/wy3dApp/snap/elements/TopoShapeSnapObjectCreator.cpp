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

#include "TopoShapeSnapObjectCreator.h"
#include <cassert>
#include <TopoDS_Shape.hxx>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> TopoShapeSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    TopoDS_Shape resultShape;
    wydb::ElementId id = pElem->getId();
    if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
    {
        resultShape = pSolid->getShape();
    }
    else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem))
    {
        resultShape = pSheet->getShape();
    }
    else
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    return this->newSnapObjects(id, resultShape);
}