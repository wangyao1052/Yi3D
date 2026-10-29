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

#ifndef WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_IMPL_H
#define WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_IMPL_H

#include <memory>
#include <set>
#include <vector>

#include <wy3dApp/SketchEntityGripGizmo.h>

namespace wy3dApp {

class SketchEntityGripGizmoImpl
{
public:
    bool begin(SketchEntityGripGizmo& owner, wydb::Database* pDb);
    bool update(
        SketchEntityGripGizmo& owner,
        wydb::Transaction* pTrans,
        const wy::Vector2& curPos);
    void end(SketchEntityGripGizmo& owner, wydb::Database* pDb, bool commit);
    void clear();

    const std::set<wydb::ElementId>& snapExcludeIds() const;

    wydb::ElementId sketchId;
    Snap2dContextSPtr snap2dContext;

private:
    void collect(SketchEntityGripGizmo& owner, wydb::Database* pDb);

    std::vector<std::weak_ptr<SketchEntityGripGizmo>> _grips;
    std::set<wydb::ElementId> _snapExcludeIds;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_IMPL_H
