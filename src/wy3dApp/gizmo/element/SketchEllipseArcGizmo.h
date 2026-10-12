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

#ifndef WY3DAPP_SKETCH_ELLIPSE_ARC_GIZMO_H
#define WY3DAPP_SKETCH_ELLIPSE_ARC_GIZMO_H

#include <wyVector2.h>
#include <wy3dSketchEllipseArc.h>

#include "SketchEntityGripGizmo.h"
#include "gizmo/GhostGizmo.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class EllipseArcGhostGizmo : public GhostGizmo
{
public:
    explicit EllipseArcGhostGizmo(const wy3d::SketchEllipseArc* pSketchEllipseArc);
};

class SketchEllipseArcGripGizmo : public SketchEntityGripGizmo
{
public:
    enum class Type
    {
        Center        = 0,
        MajorPositive = 1,
        MajorNegtive  = 2,
        MinorPositive = 3,
        MinorNegtive  = 4,
        StartAngle    = 5,
        EndAngle      = 6,
    };

public:
    SketchEllipseArcGripGizmo(const wy3d::SketchEllipseArc* pSketchEllipseArc, Type type);

protected: 
    // 获取位置
    virtual wy::Vector2 getGripPosition() const override;
    // 开始
    virtual bool onBeginDrag(wydb::Database* pDb) override;
    // 更新
    virtual bool onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos) override;
    // 结束
    virtual void onEndDrag(wydb::Database* pDb, bool commit) override;

private:
    Type _type;
    double _startMajorRadius;
    double _startMinorRadius;
};

}

#endif // WY3DAPP_SKETCH_ELLIPSE_ARC_GIZMO_H
