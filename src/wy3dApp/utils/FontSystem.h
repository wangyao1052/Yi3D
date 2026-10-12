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

#ifndef WY3DAPP_FONT_SYSTEM_H
#define WY3DAPP_FONT_SYSTEM_H

#include <string>
#include <vector>
#include <map>

#include <QString>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include "wy3dAppDefs.h"

namespace wy3dApp {

class FontSystem
{
public:
    static FontSystem& instance();

    // 根据字体名获取字体文件路径
    // 如若没有找到则返回空字符串
    std::pair<std::string, FT_Long> getFontFilePath(const std::wstring& fontFullName) const;

private:
    FontSystem();
    ~FontSystem();

private:
    std::vector<QString> _fontFiles;
    std::map<std::wstring, std::pair<size_t, FT_Long>> _fontName2Index;
};

}

#endif // WY3DAPP_FONT_SYSTEM_H
