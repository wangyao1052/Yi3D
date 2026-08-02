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

#ifndef WY3D_PARAM_ENUM_DEF_H
#define WY3D_PARAM_ENUM_DEF_H

#include <string>
#include <vector>
#include <wy3dDefs.h>

NS_WY3D_BEG

// 枚举选项
struct ParamEnumOption
{
    ParamEnumOption() : value(0), label() {}
    ParamEnumOption(int v, const std::string& l) : value(v), label(l) {}

    int value;
    std::string label;

    bool operator==(const ParamEnumOption& other) const
    { return value == other.value && label == other.label; }
};

// 枚举参数定义 (用于属性面板下拉框)
struct ParamEnumDef
{
    ParamEnumDef() : currentValue(0) {}
    ParamEnumDef(const std::vector<ParamEnumOption>& opts, int cur)
        : options(opts), currentValue(cur) {}

    std::vector<ParamEnumOption> options;
    int currentValue;

    bool operator==(const ParamEnumDef& other) const
    { return currentValue == other.currentValue; }
};

NS_WY3D_END

#endif // WY3D_PARAM_ENUM_DEF_H
