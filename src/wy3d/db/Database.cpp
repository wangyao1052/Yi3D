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

#include <wy3dDatabase.h>

NS_WY3D_BEG

const wydb::FileFormatConfig& Database::fileFormatConfig()
{
    static const wydb::FileFormatConfig format = []() {
        wydb::FileFormatConfig fmt;
        fmt.version.major = 0;
        fmt.version.minor = 19;
        fmt.markers.fileHeader = "WY3D";
        fmt.markers.fileEnder = "EOF";
        fmt.markers.elementsHeader = "ELEMENTS";
        fmt.markers.elementsEnder = "ENDELEMS";
        fmt.markers.elemEnder = "#";
        return fmt;
    }();
    return format;
}

const std::string& Database::extension()
{
    static const std::string ext = "wy3dt";
    return ext;
}

const std::string& Database::extension(wydb::FileType fileType)
{
    static const std::string textExt = "wy3dt";
    static const std::string binaryExt = "wy3db";

    switch (fileType)
    {
    case wydb::FileType::Text:   return textExt;
    case wydb::FileType::Binary: return binaryExt;
    default:
        assert(false);
        return textExt;
    }
}

Database::Database() : wydb::Database(fileFormatConfig())
{
}

NS_WY3D_END