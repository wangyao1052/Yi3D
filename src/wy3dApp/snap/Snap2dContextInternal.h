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

#ifndef WY3DAPP_SNAP2D_CONTEXT_INTERNAL_H
#define WY3DAPP_SNAP2D_CONTEXT_INTERNAL_H

#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dApp/Snap2dContext.h>

namespace wy3dApp {

// Tangent at the start point; empty when the id is null.
struct SnapStartTangent
{
    wydb::ElementId id;
    wy::Vector2 direction;

    SnapStartTangent() : id(wydb::ElementId::kNull), direction(1.0, 0.0) {}
    SnapStartTangent(const wydb::ElementId& argId, const wy::Vector2& argDir)
        : id(argId), direction(argDir) {}
};

// Line-draw context whose start point snapped to a curve, so the second point
// can snap tangent to that curve. Host-internal; not part of the SDK header.
class SketchDrawLineTangentContext final : public SketchDrawLineContext
{
    WYRX_DECLARE_ABSTRACT_MEMBERS(SketchDrawLineTangentContext, wy3dApp::SketchDrawLineTangentContext, SketchDrawLineContext)
public:
    SketchDrawLineTangentContext(
        const wy::Vector2& startPoint, const SnapStartTangent& startTangent)
        : SketchDrawLineContext(startPoint), _startTangent(startTangent) {}

    const SnapStartTangent& getStartTangent() const
    {
        return _startTangent;
    }

private:
    SnapStartTangent _startTangent;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_CONTEXT_INTERNAL_H
