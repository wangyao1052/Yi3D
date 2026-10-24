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

#ifndef WY3DAPP_SKETCH_DIMENSION_GIZMO_CREATOR_H
#define WY3DAPP_SKETCH_DIMENSION_GIZMO_CREATOR_H

#include <list>

#include <wyapGizmo.h>
#include <wyapSelection.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Selection-set level creator: when exactly one line, circle or arc is selected,
// append its dimension text Gizmos next to the regular grip Gizmos.
class SketchDimensionGizmoCreator
{
public:
    static bool tryCreateGizmos(const wyap::SelectionSet& sels,
                                std::list<wyap::GizmoSPtr>& gizmos);
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DIMENSION_GIZMO_CREATOR_H
