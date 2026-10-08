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

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "plugins/PluginManifest.h"

namespace {

const std::string kGoodLibrary = "demo.dll";

} // namespace

TEST(PluginManifest, ParseFullManifest)
{
    const std::string text =
        std::string("id=com.yi3d.demo\n"
        "name=YI3D Demo Plugin\n"
        "library=") + kGoodLibrary + "\n"
        "enabled=1\n";

    PluginManifest manifest;
    std::string error;
    ASSERT_TRUE(PluginManifestParser::parse(text, manifest, error));
    EXPECT_EQ(manifest.id, "com.yi3d.demo");
    EXPECT_EQ(manifest.name, "YI3D Demo Plugin");
    EXPECT_EQ(manifest.library, kGoodLibrary);
    EXPECT_TRUE(manifest.enabled);
}

TEST(PluginManifest, CommentsSectionsAndUnknownKeysAreIgnored)
{
    const std::string text =
        std::string("# comment line\n"
        "; semicolon comment\n"
        "\n"
        "[plugin]\n"
        "id = com.yi3d.demo \n"
        "unknown_key=whatever\n"
        "loadOrder=100\n"
        "version=1.2.3\n"
        "library=") + kGoodLibrary + "\n";

    PluginManifest manifest;
    std::string error;
    ASSERT_TRUE(PluginManifestParser::parse(text, manifest, error));
    EXPECT_EQ(manifest.id, "com.yi3d.demo");
    EXPECT_EQ(manifest.library, kGoodLibrary);
    EXPECT_TRUE(manifest.enabled);
}

TEST(PluginManifest, KeysAreCaseInsensitiveAndNameDefaultsToId)
{
    PluginManifest manifest;
    std::string error;
    ASSERT_TRUE(PluginManifestParser::parse(
        "ID=com.yi3d.demo\nLIBRARY=" + kGoodLibrary + "\n", manifest, error));
    EXPECT_EQ(manifest.id, "com.yi3d.demo");
    EXPECT_EQ(manifest.name, "com.yi3d.demo");
}

TEST(PluginManifest, EnabledVariants)
{
    const char* falseVariants[] = { "0", "false", "no", "off", "FALSE" };
    for (const char* variant : falseVariants)
    {
        PluginManifest manifest;
        std::string error;
        const std::string text = std::string("id=com.yi3d.demo\nlibrary=") + kGoodLibrary
            + "\nenabled=" + variant + "\n";
        ASSERT_TRUE(PluginManifestParser::parse(text, manifest, error)) << variant;
        EXPECT_FALSE(manifest.enabled) << variant;
    }

    PluginManifest manifest;
    std::string error;
    EXPECT_FALSE(PluginManifestParser::parse(
        std::string("id=com.yi3d.demo\nlibrary=") + kGoodLibrary
            + "\nenabled=maybe\n",
        manifest, error));
    EXPECT_FALSE(error.empty());
}

TEST(PluginManifest, InvalidIdIsRejected)
{
    const char* badIds[] = { "", "com/yi3d", "-leading", "com yi3d" };
    for (const char* badId : badIds)
    {
        PluginManifest manifest;
        std::string error;
        const std::string text = std::string("id=") + badId + "\nlibrary=" + kGoodLibrary + "\n";
        EXPECT_FALSE(PluginManifestParser::parse(text, manifest, error)) << badId;
    }
}

TEST(PluginManifest, InvalidLibraryIsRejected)
{
    const std::vector<std::string> badLibraries = {
        "",
        "sub/" + kGoodLibrary,
        "sub\\" + kGoodLibrary,
        "c:" + kGoodLibrary,
        ".." + kGoodLibrary,
    };
    for (const std::string& badLibrary : badLibraries)
    {
        PluginManifest manifest;
        std::string error;
        const std::string text = std::string("id=com.yi3d.demo\nlibrary=") + badLibrary + "\n";
        EXPECT_FALSE(PluginManifestParser::parse(text, manifest, error)) << badLibrary;
    }
}

TEST(PluginManifest, LibraryNameShapeIsNotRestricted)
{
    // Any non-empty bare file name is accepted; loadability is the loader's call.
    const char* libraries[] = { "demo.dll", "demo", "DEMO.SO", "libfoo.so.1", "demo.bin" };
    for (const char* library : libraries)
    {
        PluginManifest manifest;
        std::string error;
        const std::string text = std::string("id=com.yi3d.demo\nlibrary=") + library + "\n";
        ASSERT_TRUE(PluginManifestParser::parse(text, manifest, error)) << library;
        EXPECT_EQ(manifest.library, library);
    }
}
