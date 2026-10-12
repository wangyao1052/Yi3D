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

#include "ParamEditorAdapter.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

ParamEditorRegistry& ParamEditorRegistry::instance()
{
    static ParamEditorRegistry inst;
    return inst;
}

void ParamEditorRegistry::registerForType(
    wydb::ParameterValue::Type type, const ParamEditorAdapter* adapter)
{
    _typeMap[type] = adapter;
}

void ParamEditorRegistry::registerForAny(
    std::type_index ti, const ParamEditorAdapter* adapter)
{
    _anyMap[ti] = adapter;
}

void ParamEditorRegistry::setFallback(const ParamEditorAdapter* adapter)
{
    _fallback = adapter;
}

const ParamEditorAdapter* ParamEditorRegistry::find(
    const wydb::ParameterValue& v) const
{
    if (v.isAny())
    {
        const auto* pAny = dynamic_cast<const wydb::AnyParameterValue*>(&v);
        if (pAny)
        {
            auto it = _anyMap.find(std::type_index(pAny->getValueTypeInfo()));
            if (it != _anyMap.end())
                return it->second;
        }
    }
    else
    {
        auto it = _typeMap.find(v.getType());
        if (it != _typeMap.end())
            return it->second;
    }
    return _fallback;
}

}
