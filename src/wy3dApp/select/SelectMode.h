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

#ifndef WY3DAPP_SELECT_MODE_H
#define WY3DAPP_SELECT_MODE_H

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 选择模式
// 参考借鉴了SolidWorks&Creo的选择行为
enum class SelectMode
{
    // 全量式
    // 1.Ctrl键没有按下
    // 点选和框选都是全量更新选择集(直接覆盖);
    // 2.Ctrl键有按下
    // 点选和框选都是反转状态;
    // 全量式主要在选择命令中使用
    Full = 0,
    // 增量式
    // 1.Ctrl键没有按下
    // 点选和框选都是增量更新选择集(直接在现有选择集基础上添加);
    // 2.Ctrl键有按下
    // 点选和框选都是反转状态;
    // 增量式主要在移动镜像等具体需要选择元素的命令中使用
    Incremental = 1,
};

}

#endif // WY3DAPP_SELECT_MODE_H
