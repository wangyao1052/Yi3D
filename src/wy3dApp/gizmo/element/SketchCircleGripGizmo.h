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

#ifndef WY3DAPP_SKETCH_CIRCLE_GRIP_GIZMO_H
#define WY3DAPP_SKETCH_CIRCLE_GRIP_GIZMO_H

#include <wy3dApp/SketchEntityGripGizmo.h>
#include <wyVector2.h>
#include <wy3dSketchCircle.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchCircleGripGizmo : public SketchEntityGripGizmo
{
public:
    enum class Type
    {
        Center   = 0,
        Quarter0 = 1,
        Quarter1 = 2,
        Quarter2 = 3,
        Quarter3 = 4,
    };

public:
    SketchCircleGripGizmo(const wy3d::SketchCircle* pSketchCircle, Type type);

protected:
    // 获取位置
    virtual wy::Vector2 getGripPosition() const override;
    // 开始
    virtual bool onStartEdit(wydb::Database* pDb) override;
    // 更新
    virtual bool onApplyEdit(wydb::Transaction* pTrans, const wy::Vector2& curPos) override;
    // 拖拽上下文
    virtual Snap2dContextSPtr createSnap2dContext() const override;

private:
    Type _type;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_CIRCLE_GRIP_GIZMO_H
