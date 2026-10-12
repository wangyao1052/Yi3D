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

#ifndef WY3DAPP_GUI_CMD_TRANSIENT_H
#define WY3DAPP_GUI_CMD_TRANSIENT_H

#include <memory>
#include <osg/Node>
#include <osg/Group>
#include <osg/Geometry>
#include <wy3dVector2.h>
#include <wy3dVector3.h>

#include "wy3dAppDefs.h"

namespace wy3dApp {

class WY3DAPP_EXPORT GuiCmdTransient
{
public:
    GuiCmdTransient();
    virtual ~GuiCmdTransient();

    void show() { _root->setNodeMask(0xFFFFFFFF); }
    void hide() { _root->setNodeMask(0x00000000); }

protected:
    osg::ref_ptr<osg::Group> _root;
};
typedef std::shared_ptr<GuiCmdTransient> GuiCmdTransientSPtr;

}

#endif // WY3DAPP_GUI_CMD_TRANSIENT_H
