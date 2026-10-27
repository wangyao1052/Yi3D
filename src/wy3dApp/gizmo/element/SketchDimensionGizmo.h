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

#ifndef WY3DAPP_SKETCH_DIMENSION_GIZMO_H
#define WY3DAPP_SKETCH_DIMENSION_GIZMO_H

#include <memory>
#include <vector>

#include <QString>
#include <QStringList>
#include <wyVector2.h>
#include <wy3dPlane.h>
#include <wydbElementId.h>

#include <wy3dApp/Gizmo.h>
#include "utils/SketchDimensionLabelMath.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup1_ChoiceRow;

// Snapshot of one dimension label as it should be drawn right now
struct SketchDimensionLabel
{
    bool isValid = false;
    double value = 0.0;
    QString text;
    // Anchor in sketch UV
    wy::Vector2 anchorUv;
    SketchDimensionPlacement placement = SketchDimensionPlacement::Below;
    // Radial only: sketch-plane direction from the anchor towards the outside
    wy::Vector2 radialDirectionUv;
};

// Click-only text Gizmo showing one dimension of sketch geometry. Clicking the
// text opens a value box; the new value is written back in a single transaction
// and the label refreshes from the database. It never drags.
class SketchDimensionGizmo : public Gizmo
{
public:
    virtual ~SketchDimensionGizmo();

    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    // Live label data; false when the geometry is degenerate or gone
    bool readLabel(SketchDimensionLabel& label) const;

    const wy3d::Plane& getSketchPlane() const { return _sketchPlane; }

    // "Width" / "Diameter" / ... in the user's language; also the value box label
    virtual QString getDimensionName() const = 0;

    // Optional mode switch shown above the value box: one radio per entry,
    // index 0 selects the first. The label shows only the name and the value.
    virtual QStringList getEditorChoices() const { return {}; }
    virtual int getEditorChoice() const { return 0; }
    virtual void setEditorChoice(int) {}

protected:
    SketchDimensionGizmo(const std::vector<wydb::ElementId>& ids, wydb::ElementId sketchId);

    // Reads the current value and where the label belongs; false when degenerate
    virtual bool readDimension(double& value,
                               wy::Vector2& anchorUv,
                               SketchDimensionPlacement& placement,
                               wy::Vector2& radialDirectionUv) const = 0;
    virtual bool applyValue(double value) = 0;

    // Angular labels get a degree sign
    virtual bool isAngular() const { return false; }
    // Values the editor refuses to apply; the box stays open for correction
    virtual bool isValidValue(double value) const;

    // Reads/writes one parameter of the first element through the element's
    // parameter API -- the same route the property panel takes
    bool readSingleParameter(const char* paramName, double& value) const;
    bool writeSingleParameter(const char* paramName, double value) const;

private:
    void openEditor();
    void hideEditor();
    void onEditorAccepted();
    void onEditorChoiceChanged(int index);
    QString formatText(double value) const;

private:
    wy3d::Plane _sketchPlane;

    std::unique_ptr<GuiCmdHoverInputPopup1> _pValuePopup;
    // Non-owning alias of _pValuePopup when the editor has a choice row
    GuiCmdHoverInputPopup1_ChoiceRow* _pChoiceRow = nullptr;

    float _mouseDownX = 0.0f;
    float _mouseDownY = 0.0f;
    bool _isMouseDown = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DIMENSION_GIZMO_H
