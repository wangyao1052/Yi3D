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

#ifndef WY3DAPP_QAPPLICATION_H
#define WY3DAPP_QAPPLICATION_H

#include <string>
#include <QApplication>
#include <QEvent>

#include "IpcServer.h"

class wy3dQApplication : public QApplication
{
    Q_OBJECT
public:
    wy3dQApplication(int& argc, char** argv);

    virtual bool notify(QObject* receiver, QEvent* event) override;

    void initIpc();

private:
    IpcServer* _ipc;
};

// 执行命令事件
class ExecuteCommandEvent : public QEvent
{
public:
    static QEvent::Type eventType()
    {
        static QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
        return type;
    }

    ExecuteCommandEvent(const std::string& cmdName);

    // 获取命令名称
    const std::string& getCommandName() const { return _strCmdName; }

private:
    std::string _strCmdName;
};

// 执行Python脚本事件
class RunPythonScriptEvent : public QEvent
{
public:
    static QEvent::Type eventType()
    {
        static QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
        return type;
    }

    RunPythonScriptEvent(const std::string& scriptFileFullPath);

    // 获取脚本文件路径
    const std::string& getScriptFileFullPath() const
    {
        return _scriptFileFullPath;
    }

private:
    std::string _scriptFileFullPath;
};

// 执行命令事件
class RunGuiCommandEvent : public QEvent
{
public:
    static QEvent::Type eventType()
    {
        static QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
        return type;
    }

    RunGuiCommandEvent(const std::string& cmdName);

    // 获取命令名称
    const std::string& getCommandName() const { return _strCmdName; }

private:
    std::string _strCmdName;
};

#endif // WY3DAPP_QAPPLICATION_H