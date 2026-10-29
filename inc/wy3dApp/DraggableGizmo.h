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

#ifndef WY3DAPP_DRAGGABLE_GIZMO_H
#define WY3DAPP_DRAGGABLE_GIZMO_H

#include <memory>
#include <vector>

#include <wyVector3.h>
#include <wy3dApp/Gizmo.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class DraggableGizmoImpl;

class WY3DAPP_EXPORT DraggableGizmo : public Gizmo
{
public:
    explicit DraggableGizmo(wydb::ElementId id);
    explicit DraggableGizmo(std::vector<wydb::ElementId>&& ids);
    explicit DraggableGizmo(const std::vector<wydb::ElementId>& ids);
    virtual ~DraggableGizmo();

    virtual void onLeftMouseDown(const MouseEvent& event) final;
    virtual void onLeftMouseDrag(const MouseEvent& event) final;
    virtual void onLeftMouseUp(const MouseEvent& event) final;
    virtual void onKeyDown(const KeyEvent& event) override;

protected:
    void onDeactivate() override;

    struct StepResult
    {
        bool result;
        wy::Vector3 position;
    };
    virtual StepResult onStartDrag(wydb::Database* pDb, const MouseEvent& event) = 0;
    virtual StepResult onDragging(wydb::Database* pDb, const MouseEvent& event) = 0;
    virtual StepResult onEndDrag(wydb::Database* pDb, const MouseEvent& event) = 0;
    virtual void onCancelDrag(wydb::Database* pDb) {}

    virtual void onDragStarted() {}
    virtual void onDragMoved() {}
    virtual void onDragEnded() {}
    virtual void onDragCancelled() {}

private:
    void cancelDrag();

    std::unique_ptr<DraggableGizmoImpl> _pImpl;
};

} // namespace wy3dApp

#endif // WY3DAPP_DRAGGABLE_GIZMO_H
