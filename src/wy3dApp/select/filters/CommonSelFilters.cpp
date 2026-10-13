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

#include <wy3dApp/CommonSelFilters.h>
#include <cassert>
#include <wydbDatabase.h>
#include <wydbElement.h>
#include <wyrxClassInfo.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SelectFilterStatus CommonSelPreFilter::operator()(
    const wydb::Database* pDb,
    const wydb::ElementId& id,
    SelectAction selectAction) const
{
    assert(pDb);

    // 拾取到的不是Element比如Gizmo;理论上除了Gizmo和Element以及后期可能会添加的尺寸;其它都不应被Pick;
    if (id.isNull())
    {
        return SelectFilterStatus::Break;
    }

    // 如果是排除的元素
    // 参照Creo会继续遍历;比如在新建草图需要选择实体面或基准面时;
    // 当鼠标在圆柱面上,会选中圆柱面之后的实体面或基准面.
    if (id == _excludeId)
    {
        return SelectFilterStatus::Continue;
    }

    const wydb::Element* pElem = pDb->getElement(id);
    if (!pElem)
    {
        assert(false);
        return SelectFilterStatus::Continue;
    }
    for (wyrx::ClassInfo* pClassInfo : _classInfos)
    {
        if (!pClassInfo) continue;
        if (pElem->isKindOf(pClassInfo))
        {
            return SelectFilterStatus::Ok;
        }
    }

    return SelectFilterStatus::Continue;
}

SelectFilterStatus SingleClassSelPostFilter::operator()(
    const wydb::Database* pDb,
    const wyap::Selection& sel,
    SelectAction selectAction) const
{
    assert(pDb);
    assert(!sel.getElementId().isNull());

    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    if (!pElem)
    {
        assert(false);
        return SelectFilterStatus::Continue;
    }
    if (pElem->isKindOf(_classInfo)) return SelectFilterStatus::Ok;
    else return SelectFilterStatus::Continue;
}

SelectFilterStatus MultiClassSelPostFilter::operator()(
    const wydb::Database* pDb,
    const wyap::Selection& sel,
    SelectAction selectAction) const
{
    assert(pDb);
    assert(!sel.getElementId().isNull());

    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    if (!pElem)
    {
        assert(false);
        return SelectFilterStatus::Continue;
    }
    for (wyrx::ClassInfo* pClassInfo : _classInfos)
    {
        if (pClassInfo && pElem->isKindOf(pClassInfo))
        {
            return SelectFilterStatus::Ok;
        }
    }
    return SelectFilterStatus::Continue;
}

} // namespace wy3dApp
