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

#ifndef WY3DAPP_SELECTION_GIZMO_CREATOR_H
#define WY3DAPP_SELECTION_GIZMO_CREATOR_H

#include <list>

#include <wyapGizmo.h>
#include <wyapSelection.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Selection-set level gizmo creator. Unlike ElemGizmoCreator it sees the whole
// selection, so it can add gizmos for combinations of elements (for example a
// dimension label when exactly one sketch line is selected).
//
// Every returned gizmo must derive from BaseGizmo and its renderer must be an
// OsgGizmoRenderer (or a subclass); otherwise the scene cannot attach it.
// While createGizmos is running, addSelectionCreator returns
// NotCurrentlyAllowed, clearSelectionCreators is rejected, and reentrant
// createSelectionGizmos calls are rejected. Only the built-in select command
// registers selection creators; plugins do not. Do not modify the gizmo
// manager directly.
class WY3DAPP_EXPORT SelectionGizmoCreator
{
public:
    virtual ~SelectionGizmoCreator() {}

    // Append gizmos for the current selection.
    // Returns true when the creator handled the selection.
    virtual bool createGizmos(const wyap::SelectionSet& sels,
                              std::list<wyap::GizmoSPtr>& gizmos) const = 0;
};

} // namespace wy3dApp

#endif // WY3DAPP_SELECTION_GIZMO_CREATOR_H
