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

#include <memory>

#include <wyVector2.h>

#include "gizmo/element/SketchDimensionGizmo.h"
#include "utils/SketchRectangleDimensionMath.h"
#include "utils/SketchRectangleRecognizer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Width/height label of a rectangle: a click-only Gizmo that shows the dimension
// as text and opens a value box on click. It never drags.
class SketchRectangleDimensionGizmo : public SketchDimensionGizmo
{
public:
    SketchRectangleDimensionGizmo(std::shared_ptr<const SketchRectangleInfo> rect,
                                  SketchRectangleDimension dimension);
    virtual ~SketchRectangleDimensionGizmo();

    // "Width" / "Height" in the user's language
    virtual QString getDimensionName() const override;

protected:
    virtual bool readDimension(double& value,
                               wy::Vector2& anchorUv,
                               SketchDimensionPlacement& placement,
                               wy::Vector2& radialDirectionUv) const override;
    virtual bool applyValue(double value) override;

private:
    std::shared_ptr<const SketchRectangleInfo> _rect;
    SketchRectangleDimension _dimension = SketchRectangleDimension::Width;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_DIMENSION_GIZMO_H
