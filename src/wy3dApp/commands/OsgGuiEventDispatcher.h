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

#ifndef WY3DAPP_COMMAND_EVENT_DISPATCHER_H
#define WY3DAPP_COMMAND_EVENT_DISPATCHER_H

#include <osgGA/GUIEventHandler>
#include "commands/GuiEventDispatcher.h"

class OsgGuiEventDispatcher : public GuiEventDispatcher, public osgGA::GUIEventHandler
{
public:
    OsgGuiEventDispatcher();

    // 事件处理函数
    virtual bool handle(
        const osgGA::GUIEventAdapter& ea,
        osgGA::GUIActionAdapter& aa,
        osg::Object*,
        osg::NodeVisitor*) override;

    // The cursor left the viewport: drop the ViewCube's hover highlight and the selection
    // preview. OSG 3.6 delivers no LEAVE event, so the viewport widget reports this.
    void onCursorLeftViewport();

private:
    // 分发 Gizmo 事件
    bool handleGizmoEvent(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);
};

#endif // WY3DAPP_COMMAND_EVENT_DISPATCHER_H
