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

#ifndef WY3DAPP_GIZMO_FACTORY_H
#define WY3DAPP_GIZMO_FACTORY_H

#include <list>
#include <map>
#include <memory>
#include <string>
#include <wydbElement.h>
#include <wyErrors.h>
#include <wyapGizmo.h>
#include <wy3dApp/ElemGizmoCreator.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT GizmoFactory
{
public:
    GizmoFactory();
    ~GizmoFactory();
    GizmoFactory(const GizmoFactory&) = delete;
    GizmoFactory& operator=(const GizmoFactory&) = delete;
    GizmoFactory(GizmoFactory&&) = delete;
    GizmoFactory& operator=(GizmoFactory&&) = delete;

    std::list<wyap::GizmoSPtr> createGizmos(const wydb::Element* pElem) const;

    // Register a creator for an element class. className must be
    // Element::getClassInfo()->className() (a plugin class can pass its
    // ElementT::className()). The factory takes ownership of pCreator.
    // Plugins must remove the creator in Plugin::terminate(), before their DLL
    // is unloaded.
    // Returns InvalidInput for an empty class name or null creator, DuplicateKey
    // if the class already has a creator, or NotCurrentlyAllowed while gizmos
    // are being created. UI thread only.
    wy::ErrorStatus addElemGizmoCreator(
        const std::string& className,
        std::unique_ptr<ElemGizmoCreator> pCreator);

    // Remove the creator registered for className. Built-in and plugin
    // creators share the same table; the caller is responsible for not
    // removing creators it does not own.
    // Returns KeyNotFound when no creator is registered, or
    // NotCurrentlyAllowed while gizmos are being created. UI thread only.
    wy::ErrorStatus removeElemGizmoCreator(const std::string& className);

private:
    std::map<std::string, std::unique_ptr<ElemGizmoCreator>> _name2Creator;

    // True while createGizmos is running: creator add/remove are rejected in
    // that window.
    mutable bool _isCreatingElemGizmos = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_GIZMO_FACTORY_H
