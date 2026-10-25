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

#ifndef WY3DAPP_GIZMO_FACTORY_IMPL_H
#define WY3DAPP_GIZMO_FACTORY_IMPL_H

#include <map>
#include <memory>

#include <wy3dApp/GizmoFactory.h>

namespace wy3dApp {

// Implementation of GizmoFactory. Internal to wy3dApp; not part of the plugin SDK.
class GizmoFactoryImpl final : public GizmoFactory
{
public:
    GizmoFactoryImpl();
    ~GizmoFactoryImpl() override = default;

    std::list<wyap::GizmoSPtr> createGizmos(
        const wydb::Element* pElem) const override;
    wy::ErrorStatus addElemGizmoCreator(
        const std::string& className,
        std::unique_ptr<ElemGizmoCreator> pCreator) override;
    wy::ErrorStatus removeElemGizmoCreator(const std::string& className) override;

private:
    std::map<std::string, std::unique_ptr<ElemGizmoCreator>> _name2Creator;

    // True while createGizmos is running: creator add/remove are rejected in
    // that window.
    mutable bool _isCreatingElemGizmos = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_GIZMO_FACTORY_IMPL_H
