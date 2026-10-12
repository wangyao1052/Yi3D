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

#ifndef WY3DAPP_OSG_SELECT_HANDLER_H
#define WY3DAPP_OSG_SELECT_HANDLER_H

#include <osgGA/GUIEventHandler>
#include <osgViewer/View>
#include "SelectHandler.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

// OSG 选择处理器
class OsgSelectHandler : public SelectHandler
{
public:
    OsgSelectHandler();
    ~OsgSelectHandler();

    // OSG 事件处理函数
    bool handle(
        const osgGA::GUIEventAdapter& ea,
        osgGA::GUIActionAdapter& aa,
        osg::Object*,
        osg::NodeVisitor*);

protected:
    // 拾取实现
    wyap::Selection pointPick(float x, float y) override;
    wyap::SelectionSet boxPick_cross(
        float xMin, float yMin,
        float xMax, float yMax) override;
    wyap::SelectionSet boxPick_window(
        float xMin, float yMin,
        float xMax, float yMax) override;

    // 框选矩形绘制
    void drawBoxRect(float xMin, float yMin,
                      float xMax, float yMax) override;
    void clearBoxRect() override;

private:
    // 当前事件对应的 OSG View
    osg::observer_ptr<osgViewer::View> _pOsgView;
};

}

#endif // WY3DAPP_OSG_SELECT_HANDLER_H
