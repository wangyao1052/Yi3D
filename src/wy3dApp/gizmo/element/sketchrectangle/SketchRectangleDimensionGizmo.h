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

#ifndef WY3DAPP_SKETCH_RECTANGLE_DIMENSION_GIZMO_H
#define WY3DAPP_SKETCH_RECTANGLE_DIMENSION_GIZMO_H

#include <array>
#include <memory>

#include <wyVector2.h>
#include <wy3dPlane.h>
#include <wydbElementId.h>

#include "gizmo/BaseGizmo.h"
#include "utils/SketchRectangleDimensionMath.h"
#include "utils/SketchRectangleRecognizer.h"
#include <wy3dApp/wy3dAppDefs.h>

class QString;

namespace wy3dApp {

class GuiCmdHoverInputPopup1;

// Width/height label of a rectangle: a click-only Gizmo that shows the dimension
// as text and opens a value box on click. It never drags.
class SketchRectangleDimensionGizmo : public BaseGizmo
{
public:
    SketchRectangleDimensionGizmo(std::shared_ptr<const SketchRectangleInfo> rect,
                                  SketchRectangleDimension dimension);
    virtual ~SketchRectangleDimensionGizmo();

    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    SketchRectangleDimension getDimension() const { return _dimension; }
    const std::array<wydb::ElementId, 4>& getLineIds() const { return _lineIds; }
    const wy3d::Plane& getSketchPlane() const { return _sketchPlane; }
    const SketchRectangleInfo& getRectangleInfo() const { return *_rect; }

    // "Width" / "Height" in the user's language
    QString getDimensionName() const;

private:
    // Reads the live rectangle; false when it has degenerated.
    bool readDimensionLabel(RectangleDimensionLabel& label) const;

    void openEditor();
    void hideEditor();
    void onEditorAccepted();
    bool applyValue(double value);

private:
    std::array<wydb::ElementId, 4> _lineIds{};
    std::shared_ptr<const SketchRectangleInfo> _rect;
    SketchRectangleDimension _dimension = SketchRectangleDimension::Width;
    wydb::ElementId _sketchId;
    wy3d::Plane _sketchPlane;

    std::unique_ptr<GuiCmdHoverInputPopup1> _pValuePopup;

    float _mouseDownX = 0.0f;
    float _mouseDownY = 0.0f;
    bool _isMouseDown = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_DIMENSION_GIZMO_H
