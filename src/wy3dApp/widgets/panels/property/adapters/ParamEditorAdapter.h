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

#ifndef WY3DAPP_PARAM_EDITOR_ADAPTER_H
#define WY3DAPP_PARAM_EDITOR_ADAPTER_H

#include <QWidget>
#include <string>
#include <map>
#include <typeindex>

#include <wydbParameter.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class PropertyEditorWidget;

class ParamEditorAdapter
{
public:
    virtual ~ParamEditorAdapter() = default;
    virtual QWidget* create(const std::string& className,
                            const std::string& paramName,
                            const wydb::ParameterValue& paramValue,
                            bool isTheSameValue,
                            bool readOnly,
                            PropertyEditorWidget* parent) const = 0;
};

class ParamEditorRegistry
{
public:
    static ParamEditorRegistry& instance();

    // 注册普通类型适配器（Boolean, Integer, Double, String...）
    void registerForType(wydb::ParameterValue::Type type, const ParamEditorAdapter* adapter);
    // 注册 Any<T> 类型适配器（Color, Plane...）
    void registerForAny(std::type_index ti, const ParamEditorAdapter* adapter);
    // 设置兜底适配器
    void setFallback(const ParamEditorAdapter* adapter);

    const ParamEditorAdapter* find(const wydb::ParameterValue& v) const;

private:
    ParamEditorRegistry() = default;
    std::map<wydb::ParameterValue::Type, const ParamEditorAdapter*> _typeMap;
    std::map<std::type_index, const ParamEditorAdapter*> _anyMap;
    const ParamEditorAdapter* _fallback = nullptr;
};

} // namespace wy3dApp

#endif // WY3DAPP_PARAM_EDITOR_ADAPTER_H
