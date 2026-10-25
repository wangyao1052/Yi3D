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
#include <vector>
#include <wydbElement.h>
#include <wyErrors.h>
#include <wyapGizmo.h>
#include <wy3dApp/ElemGizmoCreator.h>
#include <wy3dApp/SelectionGizmoCreator.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT GizmoFactory
{
public:
    GizmoFactory();
    GizmoFactory(const GizmoFactory&) = delete;
    GizmoFactory& operator=(const GizmoFactory&) = delete;
    GizmoFactory(GizmoFactory&&) = delete;
    GizmoFactory& operator=(GizmoFactory&&) = delete;

    std::list<wyap::GizmoSPtr> createGizmos(const wydb::Element* pElem) const;

    // Register a selection-set level creator. Only the built-in select command
    // uses this; plugins do not register selection creators. The registry is
    // cleared when the next GUI command starts, so there is no per-creator
    // removal.
    // Returns InvalidInput for a null creator, or NotCurrentlyAllowed while
    // createSelectionGizmos is running.
    // UI thread only.
    wy::ErrorStatus addSelectionCreator(std::shared_ptr<SelectionGizmoCreator> pCreator);

    // Append the gizmos of every registered selection creator to gizmos.
    // Must not be called reentrantly from a creator. UI thread only.
    void createSelectionGizmos(const wyap::SelectionSet& sels,
                               std::list<wyap::GizmoSPtr>& gizmos) const;

    // Remove all selection creators. GuiCommand::onStart calls this for every
    // GUI command, so creators from a previous command do not leak into the
    // next one. UI thread only.
    void clearSelectionCreators();

private:
    std::map<std::string, std::unique_ptr<ElemGizmoCreator>> _name2Creator;
    std::vector<std::shared_ptr<SelectionGizmoCreator>> _selectionCreators;

    // True while createSelectionGizmos is running: add/clear are rejected in
    // that window so the creator list cannot be mutated during iteration.
    mutable bool _isCreatingSelectionGizmos = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_GIZMO_FACTORY_H
