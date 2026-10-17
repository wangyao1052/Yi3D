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

#ifndef WY3DAPP_SNAP2D_RESULT_H
#define WY3DAPP_SNAP2D_RESULT_H

#include <vector>
#include <list>
#include <map>
#include <memory>
#include <QString>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dPlane.h>
#include <wy3dApp/Snap2dContext.h>
#include "SketchSnapObject.h"
#include "SnapTipWidget.h"
#include <wy3dApp/GuiCmdTransient.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

struct Snap2dResultItem
{
    SketchSnapObjectSPtr pSnapObject;
    std::list<GuiCmdTransientSPtr> transients;
    SnapTipWidget* pSnapTipWidget;
    explicit Snap2dResultItem(SketchSnapObjectSPtr pArgSnapObj)
        : pSnapObject(pArgSnapObj), pSnapTipWidget(nullptr) {}
};

class Snap2dResult
{
public:
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        Snap2dContextSPtr pContext,
        SketchSnapObjectSPtr pSnapObject,
        bool drawPoint = true);
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        Snap2dContextSPtr pContext,
        SketchSnapObjectSPtr pSnapObject,
        SketchSnapObjectSPtr pOtherSnapObject,
        bool drawPoint = true);
    // A hit with nothing for the sketch layer to display.
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        Snap2dContextSPtr pContext);
    ~Snap2dResult();

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

    // 获取捕捉到的项
    const std::vector<Snap2dResultItem>& getItems() const
    {
        return _items;
    }

    // 显示
    void show(std::map<wydb::ElementId, GuiCmdTransientSPtr> highlights);
    // 更新
    void update();

    // 是否弱等效(不比较具体的结果位置坐标,主要比较捕捉到的对象)
    bool isLooseEqual(const Snap2dResult& rhs) const;

    // Element highlight transients used by the last show() (key = element id)
    const std::map<wydb::ElementId, GuiCmdTransientSPtr>& getHighlightTransients() const
    {
        return _highlightTransients;
    }

private:
    wy3d::Plane _sketchPlane;
    wy::Vector2 _position;
    Snap2dContextSPtr _pContext;
    // Whether the sketch layer draws the point marker for this result
    bool _isPointToDraw;
    std::vector<Snap2dResultItem> _items;
    std::map<wydb::ElementId, GuiCmdTransientSPtr> _highlightTransients;

    // 跟随鼠标移动
    std::shared_ptr<SnapTipMouseFollow> _pSnapTipMouseFollow;
};
typedef std::shared_ptr<Snap2dResult> Snap2dResultSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_RESULT_H
