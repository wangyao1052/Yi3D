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

#ifndef WY3DAPP_GUI_EVENT_DISPATCHER_H
#define WY3DAPP_GUI_EVENT_DISPATCHER_H

#include <memory>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SelectHandler;

class GuiEventDispatcher
{
public:
    GuiEventDispatcher();
    virtual ~GuiEventDispatcher();

    // 获取选择处理器
    SelectHandler* getSelectHandler() const;

protected:
    // 子类在构造函数中创建具体的处理器（引擎相关）
    std::shared_ptr<SelectHandler> _pSelectHandler;
};

} // namespace wy3dApp

#endif // WY3DAPP_GUI_EVENT_DISPATCHER_H
