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

#include "Config.h"
#include <QString>
#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QLocale>

const std::string Config::kConfigFileName = "config.ini";

Config::Config()
{
}

void Config::initialize()
{
    try
    {
        this->initializeImpl();
    }
    catch (...)
    {
        assert(false);
        return;
    }
}

void Config::initializeImpl()
{
    QString qstrAppDir = QCoreApplication::applicationDirPath();
    QString configFileFullPath = qstrAppDir + QString("/") + QString(kConfigFileName.c_str());
    QFileInfo fileInfo(configFileFullPath);
    if (!fileInfo.exists())
    {
        // 获取系统语言
        QString langCode = QLocale::system().name();
        if (langCode == "zh_CN")
        {
            langCode = "zh-CN";
        }
        else
        {
            langCode = "en-US";
        }
        const_cast<QString&>(this->system.language) = langCode; // 中文或英文

        this->saveToFile(configFileFullPath);
        return;
    }

    QSettings settings(configFileFullPath, QSettings::Format::IniFormat);
    bool ok(false);

    // system/language
    QString qstrLanguage = settings.value("system/language").toString();
    const_cast<QString&>(this->system.language) = qstrLanguage;

    // view/invertMouseWheelZoom
    int nInvertMouseWheelZoom(0);
    this->readInt(settings, "view/invertMouseWheelZoom", nInvertMouseWheelZoom, 0, 1);
    const_cast<bool&>(this->view.invertMouseWheelZoom) = (0 == nInvertMouseWheelZoom) ? false : true;

    // autoSave/intervalMinutes (0 disables auto save)
    int nAutoSaveIntervalMinutes(10);
    this->readInt(settings, "autoSave/intervalMinutes", nAutoSaveIntervalMinutes, 0, 60);

    // Backward compatibility: older config.ini versions disabled auto save
    // via autoSave/enabled; honor it as intervalMinutes = 0.
    int nAutoSaveEnabled(1);
    if (this->readInt(settings, "autoSave/enabled", nAutoSaveEnabled, 0, 1) && (0 == nAutoSaveEnabled))
    {
        nAutoSaveIntervalMinutes = 0;
    }

    const_cast<int&>(this->autoSave.intervalMinutes) = nAutoSaveIntervalMinutes;

    return;
}

bool Config::saveToFile(const QString& fileFullPath)
{
    QSettings settings(fileFullPath, QSettings::IniFormat);

    // 保存系统配置
    settings.beginGroup("system");
    settings.setValue("language", this->system.language);
    settings.endGroup();

    // 保存视图配置
    settings.beginGroup("view");
    settings.setValue("invertMouseWheelZoom", this->view.invertMouseWheelZoom ? "1" : "0");
    settings.endGroup();

    // Save auto save settings
    settings.beginGroup("autoSave");
    settings.setValue("intervalMinutes", this->autoSave.intervalMinutes);
    settings.endGroup();

    // 检查是否有错误发生
    settings.sync();
    return (settings.status() == QSettings::NoError);
}

bool Config::readInt(const QSettings& settings, const QString& key, int& out,
    int min, int max)
{
    if (!settings.contains(key))
    {
        return false;
    }

    QString qstr = settings.value(key).toString();
    bool ok(false);
    int value = qstr.toInt(&ok);
    if (ok)
    {
        if (value >= min && value <= max)
        {
            out = value;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool Config::readUInt(const QSettings& settings, const QString& key, unsigned int& out,
    unsigned int min, unsigned int max)
{
    if (!settings.contains(key))
    {
        return false;
    }

    QString qstr = settings.value(key).toString();
    bool ok(false);
    unsigned int value = qstr.toUInt(&ok);
    if (ok)
    {
        if (value >= min && value <= max)
        {
            out = value;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool Config::readFloat(const QSettings& settings, const QString& key, float& out,
    float min, float max)
{
    if (!settings.contains(key))
    {
        return false;
    }

    QString qstr = settings.value(key).toString();
    bool ok(false);
    float value = qstr.toFloat(&ok);
    if (ok)
    {
        if (value >= min && value <= max)
        {
            out = value;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool Config::readDouble(const QSettings& settings, const QString& key, double& out,
    double min, double max)
{
    if (!settings.contains(key))
    {
        return false;
    }

    QString qstr = settings.value(key).toString();
    bool ok(false);
    double value = qstr.toDouble(&ok);
    if (ok)
    {
        if (value >= min && value <= max)
        {
            out = value;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}