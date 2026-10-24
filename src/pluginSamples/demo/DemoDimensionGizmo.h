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

#ifndef DEMO_DIMENSION_GIZMO_H
#define DEMO_DIMENSION_GIZMO_H

#include <list>

#include <wydbElementId.h>
#include <wyapGizmo.h>
#include <wyapSelection.h>

#include <wy3dApp/BaseGizmo.h>
#include <wy3dApp/SelectionGizmoCreator.h>

// Minimal sketch-dimension gizmo written entirely against the public wy3dApp
// plugin SDK. It draws a dimension line next to the selected sketch line and
// lets the user edit the line length by clicking it.
class DemoLineDimensionGizmo : public wy3dApp::BaseGizmo
{
public:
    explicit DemoLineDimensionGizmo(const wydb::ElementId& lineId);
    virtual ~DemoLineDimensionGizmo();

    virtual void onLeftMouseDown(const wy3dApp::MouseEvent& event) override;
    virtual void onLeftMouseUp(const wy3dApp::MouseEvent& event) override;

private:
    float _mouseDownX = 0.0f;
    float _mouseDownY = 0.0f;
    bool _isMouseDown = false;
};

// Adds the demo dimension gizmo when exactly one whole sketch line is selected.
class DemoDimensionCreator : public wy3dApp::SelectionGizmoCreator
{
public:
    virtual bool createGizmos(const wyap::SelectionSet& sels,
                              std::list<wyap::GizmoSPtr>& gizmos) const override;
};

#endif // DEMO_DIMENSION_GIZMO_H
