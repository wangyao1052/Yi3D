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

#ifndef WY3DAPP_SKETCH_SNAP_RESULT_H
#define WY3DAPP_SKETCH_SNAP_RESULT_H

#include <vector>
#include <list>
#include <memory>
#include <QString>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dPlane.h>
#include "SketchSnapContext.h"
#include "SketchSnapObject.h"
#include "SnapTipWidget.h"
#include "commands/transient/SketchCurveTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

struct SketchSnapResultItem
{
    SketchSnapObjectSPtr pSnapObject;
    std::list<GuiCmdTransientSPtr> transients;
    SnapTipWidget* pSnapTipWidget;
    explicit SketchSnapResultItem(SketchSnapObjectSPtr pArgSnapObj)
        : pSnapObject(pArgSnapObj), pSnapTipWidget(nullptr) {}
};

class SketchSnapResult
{
public:
    SketchSnapResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        SketchSnapContextSPtr pContext,
        SketchSnapObjectSPtr pSnapObject,
        bool drawPoint = true);
    SketchSnapResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        SketchSnapContextSPtr pContext,
        SketchSnapObjectSPtr pSnapObject,
        SketchSnapObjectSPtr pOtherSnapObject,
        bool drawPoint = true);
    ~SketchSnapResult();

    // 获取位置坐标
    wy::Vector2 getPosition() const
    {
        return _position;
    }
    // 设置坐标位置
    void setPosition(const wy::Vector2& position)
    {
        _position = position;
    }

    // 获取上下文
    SketchSnapContext* getContext() const
    {
        return _pContext.get();
    }

    // 获取捕捉到的项
    const std::vector<SketchSnapResultItem>& getItems() const
    {
        return _items;
    }

    // 显示
    void show(const std::map<wydb::ElementId, SketchCurveTransientSPtr>* pCache = nullptr);
    // 更新
    void update();

    // 是否弱等效(不比较具体的结果位置坐标,主要比较捕捉到的对象)
    bool isLooseEqual(const SketchSnapResult& rhs) const;
    
    // 获取所有的草图曲线临时渲染对象
    std::map<wydb::ElementId, SketchCurveTransientSPtr> getSketchCurveTransients() const;

private:
    wy3d::Plane _sketchPlane;
    wy::Vector2 _position;
    SketchSnapContextSPtr _pContext;
    // Whether the sketch layer draws the point marker for this result
    bool _isPointToDraw;
    std::vector<SketchSnapResultItem> _items;

    // 跟随鼠标移动
    std::shared_ptr<SketchSnapTipMouseFollow> _pSnapTipMouseFollow;
};
typedef std::shared_ptr<SketchSnapResult> SketchSnapResultSPtr;

}

#endif // WY3DAPP_SKETCH_SNAP_RESULT_H
