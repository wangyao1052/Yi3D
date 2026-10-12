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

#include "BooleanSnapObjectCreator.h"
#include <cassert>
#include <TopoDS_Shape.hxx>
#include <wy3dBoolean.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> BooleanSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pElem);
    if (!pBoolean)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    wydb::ElementId id = pBoolean->getId();
    TopoDS_Shape resultShape = pBoolean->getShape();
    return this->newSnapObjects(id, resultShape);
}

}
