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
#include <set>
#include <vector>

#include <wyVector2.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>

#include <wy3dApp/Gizmo.h>
#include "gizmo/operation/SketchEntityDragOperation.h"
#include <wy3dApp/Snap2dContext.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchEntityGripGizmo : public Gizmo
{
public:
    explicit SketchEntityGripGizmo(const wy3d::SketchEntity* pSketchEntity);
    explicit SketchEntityGripGizmo(const std::vector<const wy3d::SketchEntity*>& sketchEnts);
    ~SketchEntityGripGizmo();

    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseDrag(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onEscapeKey() override;

protected:
    // 获取实时位置
    virtual wy::Vector2 getGripPosition() const = 0;

    virtual bool onBeginDrag(wydb::Database* pDb) = 0;
    virtual bool onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos) = 0;
    virtual void onEndDrag(wydb::Database* pDb, bool commit) { return; };

protected:
    // 获取草图图元
    const wy3d::SketchEntity* getSketchEntity() const;

    // 拖拽取点时排除的图元（默认：当前图元 + 所属草图）
    virtual std::set<wydb::ElementId> getSnapExcludeIds() const
    {
        std::set<wydb::ElementId> result(_ids.begin(), _ids.end());
        if (!_sketchId.isNull())
        {
            result.insert(_sketchId);
        }
        return result;
    }

    // added by wangyao 2025.04.10 {
    // 拖拽上下文
    virtual Snap2dContextSPtr newDragContext() const
    {
        return std::make_shared<Locate2dContext>();
    }
    // }

protected:
    wydb::ElementId _sketchId;
    wy3d::Plane _sketchPlane;
    Snap2dContextSPtr _pDragContext;

    friend class SketchEntityDragOperation;
    friend class SketchGripOsgRenderer;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_ENTITY_GRIP_GIZMO_H
