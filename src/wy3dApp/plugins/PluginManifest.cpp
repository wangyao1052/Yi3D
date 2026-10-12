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

#include "plugins/PluginManifest.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

std::string trim(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (std::string::npos == begin) return std::string();
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string toLower(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool parseBool(const std::string& value, bool& out)
{
    const std::string lowered = toLower(value);
    if ("1" == lowered || "true" == lowered || "yes" == lowered || "on" == lowered)
    {
        out = true;
        return true;
    }
    if ("0" == lowered || "false" == lowered || "no" == lowered || "off" == lowered)
    {
        out = false;
        return true;
    }
    return false;
}

// A library must be a bare file name: no path separators, no parent-directory
// traversal, no drive letters or NTFS stream separators.
bool isSafeLibraryName(const std::string& library)
{
    return std::string::npos == library.find("..")
        && std::string::npos == library.find('\\')
        && std::string::npos == library.find('/')
        && std::string::npos == library.find(':');
}

} // namespace

bool PluginManifestParser::parse(const std::string& iniText, PluginManifest& out, std::string& error)
{
    out = PluginManifest();
    error.clear();

    std::istringstream stream(iniText);
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || '#' == trimmed[0]) continue;

        const size_t eqIndex = trimmed.find('=');
        if (std::string::npos == eqIndex) continue;

        const std::string key = toLower(trim(trimmed.substr(0, eqIndex)));
        const std::string value = trim(trimmed.substr(eqIndex + 1));

        if ("id" == key) out.id = value;
        else if ("name" == key) out.name = value;
        else if ("library" == key) out.library = value;
        else if ("enabled" == key)
        {
            if (!parseBool(value, out.enabled))
            {
                error = "invalid enabled value: " + value;
                return false;
            }
        }
        // unknown keys are ignored
    }

    static const std::regex idPattern("[A-Za-z0-9][A-Za-z0-9._-]{0,127}");
    if (out.id.empty() || !std::regex_match(out.id, idPattern))
    {
        error = "invalid or missing id: " + out.id;
        return false;
    }

    if (out.library.empty() || !isSafeLibraryName(out.library))
    {
        error = "invalid or missing library: " + out.library;
        return false;
    }

    if (out.name.empty()) out.name = out.id;
    return true;
}

bool PluginManifestParser::loadFromFile(const std::string& u8FilePath, PluginManifest& out, std::string& error)
{
    std::ifstream file(std::filesystem::u8path(u8FilePath));
    if (!file.is_open())
    {
        out = PluginManifest();
        error = "cannot open manifest: " + u8FilePath;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str(), out, error);
}

} // namespace wy3dApp
