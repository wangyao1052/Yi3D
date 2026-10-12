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

#ifndef WY3DAPP_SELECT_FILTER_FUNCTOR_H
#define WY3DAPP_SELECT_FILTER_FUNCTOR_H

#include <vector>
#include <memory>
#include <wyrxClassInfo.h>
#include <wydbDatabase.h>
#include <wyapSelection.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

enum class SelectFilterStatus
{
    Ok = 0,       // 成功
    Continue = 1, // 失败但继续循环
    Break = 2,    // 失败但终止循环
};

enum class SelectAction
{
    Point    = 0, // 点选
    Window   = 1, // 矩形窗口(从左向右拖动鼠标框选对象)
    Crossing = 2, // 矩形窗口(从右向左拖动鼠标框选对象)
};

class SelectPreFilterFunctor
{
public:
    virtual ~SelectPreFilterFunctor() = default;

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database * pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const = 0;
};
typedef std::shared_ptr<SelectPreFilterFunctor> SelectPreFilterFunctorSPtr;

class SelectFilterFunctor
{
public:
    virtual ~SelectFilterFunctor() = default;

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wyap::Selection& sel,
        SelectAction selectAction) const = 0;
};
typedef std::shared_ptr<SelectFilterFunctor> SelectFilterFunctorSPtr;

}

#endif // WY3DAPP_SELECT_FILTER_FUNCTOR_H
