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

#ifndef WY3DAPP_POINT_PICK_OPTION_H
#define WY3DAPP_POINT_PICK_OPTION_H

#include <memory>
#include <wy3dSelectionType.h>

#include "select/SelectFilterFunctors.h"
#include "scene/nodes/ElementNodeType.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 点选配置项
struct WY3DAPP_EXPORT PointPickOption
{
    // 拾取框大小(边长的一般)
    double pickHalfSize;
    // 允许选择的元素类型(ElementNodeType 位集)
    unsigned int allowedElemTypes;
    // 选择类型
    wy3d::SelectionType selType;
    // 排除的元素ID(前置过滤)
    SelectPreFilterFunctorSPtr pSelPreFilter;
    // 自定义函数过滤器(后置过滤)
    SelectPostFilterFunctorSPtr pSelPostFilter;

    PointPickOption() :
        pickHalfSize(6.0f),
        allowedElemTypes(ElementNodeType::All),
        selType(wy3d::SelectionType::Element),
        pSelPreFilter(nullptr),
        pSelPostFilter(nullptr)
    {}
};

} // namespace wy3dApp

#endif // WY3DAPP_POINT_PICK_OPTION_H
