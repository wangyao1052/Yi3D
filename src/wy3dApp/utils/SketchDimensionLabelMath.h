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

#ifndef WY3DAPP_SKETCH_DIMENSION_LABEL_MATH_H
#define WY3DAPP_SKETCH_DIMENSION_LABEL_MATH_H

#include <cmath>

#include <wyVector2.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Where the label box sits relative to its geometry anchor
enum class SketchDimensionPlacement
{
    Below = 0,
    Above = 1,
    Right = 2,
    // Offset along the projected radial direction, so a circle/arc label always
    // ends up on the outside of the curve
    Radial = 3,
};

// On-screen centre offset of a label box from its anchor, in device pixels
// (+X right, +Y up). Radial places the near edge of the box `gapPx` away from
// the anchor along the projected direction; a degenerate direction falls back
// to Below so the label stays visible.
inline wy::Vector2 computeSketchDimensionPixelOffset(
    SketchDimensionPlacement placement,
    double gapPx,
    double labelWidthPx,
    double labelHeightPx,
    const wy::Vector2& radialDirectionPx)
{
    switch (placement)
    {
    case SketchDimensionPlacement::Below:
        return wy::Vector2(0.0, -(gapPx + 0.5 * labelHeightPx));
    case SketchDimensionPlacement::Above:
        return wy::Vector2(0.0, gapPx + 0.5 * labelHeightPx);
    case SketchDimensionPlacement::Right:
        return wy::Vector2(gapPx + 0.5 * labelWidthPx, 0.0);
    case SketchDimensionPlacement::Radial:
    {
        const double length = radialDirectionPx.length();
        if (length < 1e-12)
        {
            return wy::Vector2(0.0, -(gapPx + 0.5 * labelHeightPx));
        }

        const wy::Vector2 direction = radialDirectionPx / length;
        // Support function of the screen-aligned label box along `direction`
        const double support = 0.5 * labelWidthPx * std::fabs(direction.x())
            + 0.5 * labelHeightPx * std::fabs(direction.y());
        return direction * (gapPx + support);
    }
    }

    return wy::Vector2();
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DIMENSION_LABEL_MATH_H
