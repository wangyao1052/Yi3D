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

#ifndef WY3DAPP_COPY_PASTE_UTIL_H
#define WY3DAPP_COPY_PASTE_UTIL_H

#include <string>
#include <list>
#include <wydbElement.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class CopyPasteUtil
{
public:
    // 复制返回值
    enum class CopyReturn
    {
        Ok    = 0,
        Error = 1,
        ElemsNotSupported = 2, // 元素不支持复制
    };
    // 复制
    static CopyReturn copy();
    // 是否可以复制
    // 只要有一个元素可以复制就返回true
    static bool canCopy(const std::list<const wydb::Element*>& elemsToCopy, std::list<const wydb::Element*>& elemsCanCopy);
    static bool canCopy(const std::list<const wydb::Element*>& elemsToCopy);
    // 弹框提示复制错误
    static void showCopyErrorMsgBox(CopyReturn copyRet);

    // 粘贴返回值
    enum class PasteReturn
    {
        Ok = 0,
        Error = 1,
    };
    // 粘贴
    static PasteReturn paste();
    // 是否可以粘贴
    static bool canPaste();
    // 弹框提示粘贴错误
    static void showPasteErrorMsgBox(PasteReturn pasteRet);

    static std::string generateCopyPasteLabel();
};

} // namespace wy3dApp

#endif // WY3DAPP_COPY_PASTE_UTIL_H
