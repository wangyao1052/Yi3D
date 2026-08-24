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

#ifndef WY3DAPP_CURSOR_CENTER_H
#define WY3DAPP_CURSOR_CENTER_H

#include <QCursor>

enum class CursorType;

class CursorCenter
{
public:
    static CursorCenter& instance();

    QCursor getCursor(CursorType cursorType) const;

private:
    CursorCenter();

private:
    // 选择元素
    QCursor _cursorSelElems;
    // 定位
    QCursor _cursorLocate;
    // 删除
    QCursor _cursorDelete;
    // 禁用
    QCursor _cursorForbid;
    // 旋转
    QCursor _cursorRotate;
};

#endif // WY3DAPP_CURSOR_CENTER_H
