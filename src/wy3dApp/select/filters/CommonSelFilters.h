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

#include "select/SelectFilterFunctor.h"

class CommonPreSelFilterForPointPick : public SelectPreFilterFunctor
{
public:
    explicit CommonPreSelFilterForPointPick(
        wyrx::ClassInfo* classInfo,
        wydb::ElementId excludeId = wydb::ElementId::kNull)
        : _excludeId(excludeId)
    {
        _classInfos.emplace_back(classInfo);
    }

    CommonPreSelFilterForPointPick(
        wyrx::ClassInfo* classInfo1st,
        wyrx::ClassInfo* classInfo2nd,
        wydb::ElementId excludeId = wydb::ElementId::kNull)
        : _excludeId(excludeId)
    {
        _classInfos.emplace_back(classInfo1st);
        _classInfos.emplace_back(classInfo2nd);
    }

    explicit CommonPreSelFilterForPointPick(
        const std::vector<wyrx::ClassInfo*>& classInfos,
        wydb::ElementId excludeId = wydb::ElementId::kNull)
        : _classInfos(classInfos), _excludeId(excludeId)
    {
    }

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override;

private:
    std::vector<wyrx::ClassInfo*> _classInfos;
    wydb::ElementId _excludeId;
};

class SingleClassSelFilter : public SelectFilterFunctor
{
public:
    explicit SingleClassSelFilter(wyrx::ClassInfo* classInfo)
        : _classInfo(classInfo) {}

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override;

private:
    wyrx::ClassInfo* _classInfo;
};

class MultiClassSelFilter : public SelectFilterFunctor
{
public:
    explicit MultiClassSelFilter(const std::vector<wyrx::ClassInfo*>& classInfos)
        : _classInfos(classInfos) {}

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const override;

private:
    std::vector<wyrx::ClassInfo*> _classInfos;
};

#endif // WY3DAPP_COMMON_SEL_FILTERS_H