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

#ifndef WY3DAPP_POINT_PICK_H
#define WY3DAPP_POINT_PICK_H

#include <set>
#include <memory>
#include <osgViewer/View>
#include <wyapSelection.h>
#include <wy3dSelectionType.h>
#include "scene/RenderConst.h"
#include "select/SelectFilterFunctor.h"

// 点选配置项
struct PointPickOption
{
    // 拾取框大小(边长的一般)
    double pickHalfSize;
    // 拾取掩码
    unsigned int pickMask;
    // 选择类型
    wy3d::SelectionType selType;
    // 排除的元素ID(前置过滤)
    SelectPreFilterFunctorSPtr pSelPreFilter;
    // 自定义函数过滤器(后置过滤)
    SelectFilterFunctorSPtr pSelFilter;

    PointPickOption() :
        pickHalfSize(6.0f),
        pickMask(PICK_MASK),
        selType(wy3d::SelectionType::Element),
        pSelPreFilter(nullptr),
        pSelFilter(nullptr)
    {}
};

class PointPick
{
public:
    static wyap::Selection pick(
        const wydb::Database* pDb,
        osgViewer::View* pView,
        float x, float y,
        const PointPickOption& option);
};

#endif // WY3DAPP_POINT_PICK_H