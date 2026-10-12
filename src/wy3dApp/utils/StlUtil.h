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

#ifndef WY3DAPP_STL_UTIL_H
#define WY3DAPP_STL_UTIL_H

#include <vector>
#include <map>
#include <set>
#include <string>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class StlUtil
{
public:
    // 在map中查找第一个小于等于(<=)value的位置
    template<typename KeyType, typename ValueType>
    static auto findFirstLTE(const std::map<KeyType, ValueType>& map, const KeyType& value)
        -> typename std::map<KeyType, ValueType>::const_iterator
    {
        auto iter = map.upper_bound(value); // 第一个>value的位置
        if (iter == map.cbegin())
        {
            return map.cend();
        }
        return --iter;
    }

    // 在map中查找第一个大于等于(>=)value的位置
    template<typename KeyType, typename ValueType>
    static auto findFirstGTE(const std::map<KeyType, ValueType>& map, const KeyType& value)
        -> typename std::map<KeyType, ValueType>::const_iterator
    {
        return map.lower_bound(value);
    }
};

} // namespace wy3dApp

#endif // WY3DAPP_STL_UTIL_H
