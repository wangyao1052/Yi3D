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

#ifndef WY3DAPP_SKETCH_ARC_DIMENSION_GIZMO_H
#define WY3DAPP_SKETCH_ARC_DIMENSION_GIZMO_H

#include <wyVector2.h>
#include <wy3dSketchArc.h>

#include "gizmo/element/SketchDimensionGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Radius label of a single sketch arc
class SketchArcDimensionGizmo : public SketchDimensionGizmo
{
public:
    explicit SketchArcDimensionGizmo(const wy3d::SketchArc* pSketchArc);
    virtual ~SketchArcDimensionGizmo();

    // "Radius" in the user's language
    virtual QString getDimensionName() const override;

protected:
    virtual bool readDimension(double& value,
                               wy::Vector2& anchorUv,
                               SketchDimensionPlacement& placement,
                               wy::Vector2& radialDirectionUv) const override;
    virtual bool applyValue(double value) override;
    virtual bool isValidValue(double value) const override;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_ARC_DIMENSION_GIZMO_H
