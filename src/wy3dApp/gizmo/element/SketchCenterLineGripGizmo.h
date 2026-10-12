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

#ifndef WY3DAPP_SKETCH_CENTER_LINE_GRIP_GIZMO_H
#define WY3DAPP_SKETCH_CENTER_LINE_GRIP_GIZMO_H

#include "SketchEntityGripGizmo.h"
#include <wyVector2.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchCenterLineGripGizmo : public SketchEntityGripGizmo
{
public:
    enum class Type
    {
        StartPoint = 0,
        EndPoint = 1,
        MiddlePoint = 2,
    };

public:
    SketchCenterLineGripGizmo(const wy3d::SketchCenterLine* pCenterLine, Type type);

protected:
    // 获取实时位置
    virtual wy::Vector2 getGripPosition() const override;
    // 开始
    virtual bool onBeginDrag(wydb::Database* pDb) override;
    // 更新
    virtual bool onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos) override;
    // 拖拽上下文
    virtual SketchSnapContextSPtr newDragContext() const override;

private:
    Type _type;
    wy::Vector2 _startSPnt;
    wy::Vector2 _startEPnt;
    wy::Vector2 _startMPnt;
};

}

#endif // WY3DAPP_SKETCH_CENTER_LINE_GRIP_GIZMO_H
