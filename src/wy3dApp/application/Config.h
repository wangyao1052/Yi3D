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

#ifndef WY3DAPP_CONFIG_H
#define WY3DAPP_CONFIG_H

#include <cfloat>
#include <string>
#include <QString>
#include <QSettings>

class Config
{
public:
    Config();

    // 初始化
    void initialize();

    // 系统
    struct System
    {
        // 语言:中文
        const QString language = "zh-CN";

    } system;

    // 视图
    struct View
    {
        // 反转鼠标滚轮缩放方向:否
        const bool invertMouseWheelZoom = false;

    } view;

    // Auto save
    struct AutoSave
    {
        // Auto save interval (minutes): 15, 0 disables it
        const int intervalMinutes = 15;

    } autoSave;

private:
    void initializeImpl();

    bool saveToFile(const QString& fileFullPath);

    bool readInt(const QSettings& settings, const QString& key, int& out,
        int min = INT_MIN, int max = INT_MAX);

    bool readUInt(const QSettings& settings, const QString& key, unsigned int& out,
        unsigned int min = 0, unsigned int max = UINT_MAX);

    bool readFloat(const QSettings& settings, const QString& key, float& out,
        float min = FLT_MIN, float max = FLT_MAX);

    bool readDouble(const QSettings& settings, const QString& key, double& out,
        double min = DBL_MIN, double max = DBL_MAX);

private:
    // 文件名称:"config.ini"
    static const std::string kConfigFileName;
};

#endif // WY3DAPP_CONFIG_H