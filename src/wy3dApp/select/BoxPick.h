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

#ifndef WY3DAPP_BOX_PICK_H
#define WY3DAPP_BOX_PICK_H

#include <osgViewer/View>
#include <wydbDatabase.h>
#include <wyapSelection.h>
#include <wy3dSelectionType.h>
#include "scene/nodes/ElementNodeType.h"
#include "select/SelectFilterFunctors.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 框选配置项
struct BoxPickOption
{
    // 允许选择的元素类型(ElementNodeType 位集)
    unsigned int allowedElemTypes;
    // 选择类型
    wy3d::SelectionType selType;
    // 排除的元素ID(前置过滤)
    SelectPreFilterFunctorSPtr pPreFilter;
    // 自定义函数过滤器(后置过滤)
    SelectPostFilterFunctorSPtr pPostFilter;

    BoxPickOption() :
        allowedElemTypes(ElementNodeType::All),
        selType(wy3d::SelectionType::Element),
        pPreFilter(nullptr),
        pPostFilter(nullptr)
    {}
};

class BoxPick
{
public:
    static wyap::SelectionSet pick(
        const wydb::Database* pDb,
        osgViewer::View* pView,
        double xMin, double yMin, double xMax, double yMax,
        const BoxPickOption& option);
};

} // namespace wy3dApp

#endif // WY3DAPP_BOX_PICK_H
