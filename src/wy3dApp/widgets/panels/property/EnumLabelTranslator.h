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

#ifndef WY3DAPP_ENUM_LABEL_TRANSLATOR_H
#define WY3DAPP_ENUM_LABEL_TRANSLATOR_H

#include <QString>
#include <string>

// 枚举标签翻译工具 —— 将 ParamEnumDef 中的英文标签转为显示文本
// 新增枚举标签时，在 .cpp 文件末尾添加 tr() 调用即可（lupdate 扫描用）
class EnumLabelTranslator
{
public:
    static QString translate(const std::string& englishLabel);
};

#endif
