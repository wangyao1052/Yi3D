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

#include "utils/FontSystem.h"

#include <QStandardPaths>
#include <QDir>

#include "wy3d/utils/FreeTypeUtil.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

FontSystem& FontSystem::instance()
{
    static FontSystem instance;
    return instance;
}

void recordFontInfo(FT_Library library, const std::vector<QString>& fontFiles, std::map<std::wstring, std::pair<size_t, FT_Long>>& fontName2Index)
{
    assert(library);

    for (size_t i = 0; i < fontFiles.size(); ++i)
    {
        const QString& fontFileFullPath = fontFiles[i];
        std::string strFontFilePath;
#ifdef _WIN32
        strFontFilePath = fontFileFullPath.toLocal8Bit();
#else
        strFontFilePath = fontFileFullPath.toUtf8().toStdString();
#endif // _Win32

        FT_Face face;
        FT_Error error = FT_New_Face(library, strFontFilePath.c_str(), 0, &face);
        if (0 != error || !face)
        {
            continue;
        }
        FT_Long numFaces = face->num_faces;
        std::vector<std::wstring> faceNames = wy3d::FreeTypeUtil::getFaceNames(face);
        for (const std::wstring& fontName : faceNames)
        {
            if (fontName2Index.find(fontName) == fontName2Index.cend()) // 不覆盖因为一般优先级高的字体文件会先列出来
                fontName2Index[fontName] = std::pair<size_t, FT_Long>(i, 0);
        }
        error = FT_Done_Face(face);
        assert(0 == error);

        if (numFaces > 1)
        {
            face = 0;
            for (FT_Long k = 1; k < numFaces; ++k)
            {
                error = FT_New_Face(library, strFontFilePath.c_str(), k, &face);
                if (0 != error || !face)
                {
                    continue;
                }
                std::vector<std::wstring> faceNames = wy3d::FreeTypeUtil::getFaceNames(face);
                for (const std::wstring& fontName : faceNames)
                {
                    if (fontName2Index.find(fontName) == fontName2Index.cend()) // 不覆盖因为一般优先级高的字体文件会先列出来
                        fontName2Index[fontName] = std::pair<size_t, FT_Long>(i, k);
                }
                error = FT_Done_Face(face);
                assert(0 == error);
            }
        }
    }
}

FontSystem::FontSystem()
{
    // 获取系统所有常用字体文件路径
    const QStringList filters = { "*.ttf", "*.otf", "*.ttc" };
    const QStringList fontDirs = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);
    for (const QString& fontDirPath : fontDirs)
    {
        QDir fontDir(fontDirPath);
        if (!fontDir.exists()) continue;

        // 遍历常见字体文件(.ttf/.otf/.ttc)的路径
        const QStringList fontFiles = fontDir.entryList(filters, QDir::Files);
        for (const QString& fileName : fontFiles)
        {
            QString fullPath = fontDir.filePath(fileName);
            _fontFiles.emplace_back(std::move(fullPath));
        }
    }

    // 使用FreeType库获取所有字体的名称
    FT_Library library;
    FT_Error error = FT_Init_FreeType(&library);
    if (error == 0 && library)
    {
        try
        {
            recordFontInfo(library, _fontFiles, _fontName2Index);
        }
        catch (...)
        {
            assert(false);
        }
        FT_Done_FreeType(library);
    }
}

FontSystem::~FontSystem()
{
}

std::pair<std::string, FT_Long> FontSystem::getFontFilePath(const std::wstring& fontFullName) const
{
    std::pair<std::string, FT_Long> ret("", 0);
    auto iter = _fontName2Index.find(fontFullName);
    if (iter == _fontName2Index.cend())
    {
        return ret;
    }
    auto indexPair = iter->second;
    if (indexPair.first >= _fontFiles.size())
    {
        assert(false);
        return ret;
    }

#ifdef _WIN32
    ret.first = _fontFiles[indexPair.first].toLocal8Bit();
#else
    ret.first = _fontFiles[indexPair.first].toUtf8().toStdString();
#endif // _Win32
    ret.second = indexPair.second;

    return ret;
}

}
