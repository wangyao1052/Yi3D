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

#ifndef WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_H
#define WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_H

#include <memory>
#include <vector>

#include <wyVector2.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>

#include <wy3dApp/DraggableGizmo.h>
#include <wy3dApp/Snap2dContext.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchEntityGripGizmoImpl;

class WY3DAPP_EXPORT SketchEntityGripGizmo : public DraggableGizmo
{
protected:
    SketchEntityGripGizmo(
        wydb::ElementId sketchId,
        wydb::ElementId entityId);
    SketchEntityGripGizmo(
        wydb::ElementId sketchId,
        std::vector<wydb::ElementId> ids);
    ~SketchEntityGripGizmo();

public:
    virtual wy::Vector2 getGripPosition() const = 0;

protected:
    DraggableGizmo::StepResult onStartDrag(wydb::Database* pDb, const MouseEvent& event) override;
    DraggableGizmo::StepResult onDragging(wydb::Database* pDb, const MouseEvent& event) override;
    DraggableGizmo::StepResult onEndDrag(wydb::Database* pDb, const MouseEvent& event) override;
    void onCancelDrag(wydb::Database* pDb) override;

    void onDragStarted() override;
    void onDragEnded() override;
    void onDragCancelled() override;

    virtual bool onStartEdit(wydb::Database* pDb) = 0;
    virtual bool onApplyEdit(wydb::Transaction* pTrans, const wy::Vector2& curPos) = 0;
    virtual void onEndEdit(wydb::Database* pDb, bool commit) { return; };

    virtual Snap2dContextSPtr createSnap2dContext() const
    {
        return std::make_shared<Locate2dContext>();
    }

private:
    friend class SketchEntityGripGizmoImpl;
    std::unique_ptr<SketchEntityGripGizmoImpl> _pImpl;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_H
