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

#ifndef WY3DAPP_SOLID_TO_CUT_FILTER_H
#define WY3DAPP_SOLID_TO_CUT_FILTER_H

#include <cassert>
#include <wy3dSolid.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include "select/SelectFilterFunctors.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SolidToCutSelectPreFilter : public SelectPreFilterFunctor
{
public:
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        assert(pDb);

        if (id.isNull()) // 拾取到的不是Element
        {
            return SelectFilterStatus::Break;
        }

        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return SelectFilterStatus::Break;
        if (!pSolid->getParent().isNull()) return SelectFilterStatus::Break;

        
        if (pSolid->isCut())
        {
            return SelectFilterStatus::Break;
        }

        return SelectFilterStatus::Ok;
    }
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_FACE_SEL_FILTER_H
