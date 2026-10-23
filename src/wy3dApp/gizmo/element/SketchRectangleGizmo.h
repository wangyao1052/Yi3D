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

#ifndef WY3DAPP_SKETCH_RECTANGLE_GIZMO_H
#define WY3DAPP_SKETCH_RECTANGLE_GIZMO_H

#include <array>
#include <memory>
#include <set>
#include <vector>

#include <wyVector2.h>
#include <wy3dSketchLine.h>

#include "SketchEntityGripGizmo.h"
#include "utils/SketchRectangleDragMath.h"
#include "utils/SketchRectangleRecognizer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 4 条直线构成的矩形 Gizmo：角点 / 边中点 / 中心
// 所有手柄共享同一个矩形参数化，拖动任一 basis 时 4 条线联动
class SketchRectangleGizmo : public SketchEntityGripGizmo
{
public:
    using Type = SketchRectangleHandle;

public:
    SketchRectangleGizmo(std::shared_ptr<const SketchRectangleInfo> rect,
                         const std::vector<const wy3d::SketchLine*>& lines,
                         Type type,
                         int index);
    virtual ~SketchRectangleGizmo();

    virtual std::set<wydb::ElementId> getSnapExcludeIds() const override;

protected:
    // 获取实时位置
    virtual wy::Vector2 getGripPosition() const override;
    // 开始
    virtual bool onBeginDrag(wydb::Database* pDb) override;
    // 更新
    virtual bool onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos) override;

private:
    wy::Vector2 gripPositionFromCorners(const std::array<wy::Vector2, 4>& corners) const;

private:
    std::array<wydb::ElementId, 4> _lineIds{};
    std::shared_ptr<const SketchRectangleInfo> _rect;
    Type _type = Type::Center;
    int _index = 0;

    // 拖拽开始快照
    std::array<wy::Vector2, 4> _dragCorners{};
    std::array<bool, 4> _dragLineStartAtCorner{};
    wy::Vector2 _dragU = wy::Vector2::kXAxis;
    wy::Vector2 _dragV = wy::Vector2::kYAxis;
    double _dragHalfWidth = 0.0;
    double _dragHalfHeight = 0.0;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_GIZMO_H
