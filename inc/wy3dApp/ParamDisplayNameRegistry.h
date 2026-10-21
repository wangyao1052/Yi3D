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
#include <map>
#include <QObject>
#include <QString>

#include <wyErrors.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wyrx { class ClassInfo; }

namespace wy3dApp {

class WY3DAPP_EXPORT ParamDisplayNameRegistry : public QObject
{
    Q_OBJECT
public:
    static ParamDisplayNameRegistry& instance();

    // Returns the registered display name, or paramName if not registered.
    QString lookup(const std::string& className, const std::string& paramName) const;

    // Returns NullInputPointer if pClassInfo is null, DuplicateKey if the
    // (class, paramName) pair is already registered.
    wy::ErrorStatus add(const wyrx::ClassInfo* pClassInfo,
                        const std::string& paramName,
                        const QString& displayName);

    // Header-only sugar for classes declared with WYRX_DECLARE_MEMBERS /
    // WYDB_DECLARE_MEMBERS. Pass the class that declares the parameter.
    template <typename Class>
    static wy::ErrorStatus add(const std::string& paramName,
                               const QString& displayName)
    {
        return instance().add(Class::classInfo(), paramName, displayName);
    }

private:
    // className::paramName -> display name
    std::map<std::string, QString> _paramName2DisplayName;

private:
    explicit ParamDisplayNameRegistry(QObject* parent = nullptr);
    ~ParamDisplayNameRegistry();
};

} // namespace wy3dApp

#endif // WY3D_APP_PARAM_DISPLAY_NAME_REGISTRY_H
