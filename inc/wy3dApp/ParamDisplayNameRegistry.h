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

#ifndef WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_H
#define WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_H

#include <string>
#include <QString>

#include <wyErrors.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wyrx { class ClassInfo; }

namespace wy3dApp {

// Interface only; the implementation is internal. The application owns the
// single instance and creates it after the translators are installed; access
// it through Application::getParamDisplayNameRegistry().
class WY3DAPP_EXPORT ParamDisplayNameRegistry
{
public:
    virtual ~ParamDisplayNameRegistry();

    // Returns the registered display name, or paramName if not registered.
    virtual QString lookup(const std::string& className,
                           const std::string& paramName) const = 0;

    // Returns NullInputPointer if pClassInfo is null, DuplicateKey if the
    // (class, paramName) pair is already registered.
    virtual wy::ErrorStatus add(const wyrx::ClassInfo* pClassInfo,
                                const std::string& paramName,
                                const QString& displayName) = 0;

    // Header-only sugar for classes declared with WYRX_DECLARE_MEMBERS /
    // WYDB_DECLARE_MEMBERS. Pass the class that declares the parameter.
    template <typename Class>
    wy::ErrorStatus add(const std::string& paramName,
                        const QString& displayName)
    {
        return add(Class::classInfo(), paramName, displayName);
    }

protected:
    ParamDisplayNameRegistry() = default;
};

} // namespace wy3dApp

#endif // WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_H
