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

#ifndef WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_IMPL_H
#define WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_IMPL_H

#include <map>
#include <string>

#include <QCoreApplication>
#include <QString>

#include <wy3dApp/ParamDisplayNameRegistry.h>

namespace wy3dApp {

class ParamDisplayNameRegistryImpl final : public ParamDisplayNameRegistry
{
    // Keeps the translations under the interface's context.
    Q_DECLARE_TR_FUNCTIONS(wy3dApp::ParamDisplayNameRegistry)

public:
    ParamDisplayNameRegistryImpl();

    QString lookup(const std::string& className,
                   const std::string& paramName) const override;

    wy::ErrorStatus add(const wyrx::ClassInfo* pClassInfo,
                        const std::string& paramName,
                        const QString& displayName) override;

private:
    // className::paramName -> display name
    std::map<std::string, QString> _paramName2DisplayName;
};

} // namespace wy3dApp

#endif // WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_IMPL_H
