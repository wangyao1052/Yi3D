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

#ifndef WY3DAPP_VIEW_COMMANDS_H
#define WY3DAPP_VIEW_COMMANDS_H

#include "SimpleCommand.h"

DEFINE_SIMPLE_CMD(FitViewCommand)
DEFINE_SIMPLE_CMD(IsometricViewCommand)
DEFINE_SIMPLE_CMD(FrontViewCommand)
DEFINE_SIMPLE_CMD(BackViewCommand)
DEFINE_SIMPLE_CMD(LeftViewCommand)
DEFINE_SIMPLE_CMD(RightViewCommand)
DEFINE_SIMPLE_CMD(TopViewCommand)
DEFINE_SIMPLE_CMD(BottomViewCommand)
// 草绘视图
DEFINE_SIMPLE_CMD(OrientToSketchCommand)
// 正视于
DEFINE_SIMPLE_CMD(ViewNormalToCommand)

// 显示模式
DEFINE_SIMPLE_CMD(ShadedDisplayCommand)
DEFINE_SIMPLE_CMD(WireframeDisplayCommand)

#endif // WY3DAPP_VIEW_COMMANDS_H