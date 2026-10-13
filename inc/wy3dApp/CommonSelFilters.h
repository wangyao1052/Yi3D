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

#ifndef WY3DAPP_COMMON_SEL_FILTERS_H
#define WY3DAPP_COMMON_SEL_FILTERS_H

#include "select/SelectFilterFunctors.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT SingleClassSelPostFilter : public SelectPostFilterFunctor
{
public:
    explicit SingleClassSelPostFilter(wyrx::ClassInfo* classInfo)
        : _classInfo(classInfo) {}

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override;

private:
    wyrx::ClassInfo* _classInfo;
};

class WY3DAPP_EXPORT MultiClassSelPostFilter : public SelectPostFilterFunctor
{
public:
    explicit MultiClassSelPostFilter(const std::vector<wyrx::ClassInfo*>& classInfos)
        : _classInfos(classInfos) {}

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override;

private:
    std::vector<wyrx::ClassInfo*> _classInfos;
};

class CommonSelPreFilter : public SelectPreFilterFunctor
{
public:
    explicit CommonSelPreFilter(
        const std::vector<wyrx::ClassInfo*>& classInfos,
        wydb::ElementId excludeId = wydb::ElementId::kNull)
        : _classInfos(classInfos), _excludeId(excludeId)
    {
    }

    template <typename... Classes>
    static std::shared_ptr<CommonSelPreFilter> create(
        wydb::ElementId excludeId = wydb::ElementId::kNull)
    {
        return std::make_shared<CommonSelPreFilter>(
            std::vector<wyrx::ClassInfo*>{ Classes::classInfo()... }, excludeId);
    }

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override;

private:
    std::vector<wyrx::ClassInfo*> _classInfos;
    wydb::ElementId _excludeId;
};

} // namespace wy3dApp

#endif // WY3DAPP_COMMON_SEL_FILTERS_H
