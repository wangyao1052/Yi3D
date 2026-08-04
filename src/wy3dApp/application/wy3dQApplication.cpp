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

#include "wy3dQApplication.h"
#include <QMessageBox>
#include <QFile>
#include "Application.h"
#include "PythonScriptExecutor.h"
#include "widgets/panels/output/OutputWidget.h"

wy3dQApplication::wy3dQApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    this->initIpc();
}

bool wy3dQApplication::notify(QObject* receiver, QEvent* event)
{
    QEvent::Type evtType = event->type();
    if (evtType == ExecuteCommandEvent::eventType())
    {
        if (ExecuteCommandEvent* pExecuteCmdEvt = dynamic_cast<ExecuteCommandEvent*>(event))
        {
            Application::instance().getCmdManager()->executeCommand(pExecuteCmdEvt->getCommandName());
        }
        else
        {
            assert(false);
        }
        return true;
    }
    else if (evtType == RunPythonScriptEvent::eventType())
    {
        RunPythonScriptEvent* pRunPythonScriptEvent = dynamic_cast<RunPythonScriptEvent*>(event);
        if (pRunPythonScriptEvent)
        {
            const std::string& scriptFileFullPath = pRunPythonScriptEvent->getScriptFileFullPath();

            // Abort current modal command.
            wyap::CmdManager* pCmdMgr = Application::instance().getCmdManager();
            wyap::Command* pCurModalCmd = pCmdMgr->getCurrentModalCommand();
            if (pCurModalCmd)
            {
                pCmdMgr->abortCurrentModalCommand(
                    wyap::CmdExecution::AbortCause::ForceTerminate);
            }

            // Run script.
            PythonScriptExecutor executor;
            PythonScriptExecutor::Error error = executor.Run(scriptFileFullPath);
            bool isOk(false);
            QString message;
            switch (error)
            {
            case PythonScriptExecutor::Error::NoError:
            {
                isOk = true;
                message = QStringLiteral("NoError");
            }
            break;

            case PythonScriptExecutor::Error::PythonLibraryNotFound:
            {
                message = QStringLiteral("PythonLibraryNotFound");
            }
            break;

            case PythonScriptExecutor::Error::LoadPythonLibraryFailed:
            {
                message = QStringLiteral("LoadPythonLibraryFailed");
            }
            break;

            case PythonScriptExecutor::Error::InvalidPythonLibrary:
            {
                message = QStringLiteral("InvalidPythonLibrary");
            }
            break;

            case PythonScriptExecutor::Error::PythonScriptFileNotFound:
            {
                message = QStringLiteral("PythonScriptFileNotFound");
            }
            break;

            case PythonScriptExecutor::Error::OpenPythonScriptFileFailed:
            {
                message = QStringLiteral("OpenPythonScriptFileFailed");
            }
            break;

            case PythonScriptExecutor::Error::RunScriptError:
            {
                message = QStringLiteral("RunScriptError");
            }
            break;

            case PythonScriptExecutor::Error::UnknownError:
            default:
            {
                message = QStringLiteral("UnknownError");
            }
            break;
            }
            _ipc->completeCurrentRequest(isOk, message);
        }
        else
        {
            assert(false);
        }
        return true;
    }
    else if (evtType == RunGuiCommandEvent::eventType())
    {
        if (RunGuiCommandEvent* pRunGuiCmdEvt = dynamic_cast<RunGuiCommandEvent*>(event))
        {
            const std::string cmdName = pRunGuiCmdEvt->getCommandName();
            wyap::CmdStack* pCmdStack = Application::instance().getCmdStack();
            wyap::CmdManager* pCmdMgr = Application::instance().getCmdManager();

            wyap::Command* pCmd = pCmdStack->getCommand(cmdName);
            if (!pCmd)
            {
                _ipc->completeCurrentRequest(false, QStringLiteral("CommandNotFound"));
                return true;
            }

            if (true)
            {
                wyap::Command* pCurModalCmd = pCmdMgr->getCurrentModalCommand();
                if (pCurModalCmd)
                {
                    pCmdMgr->abortCurrentModalCommand(
                        wyap::CmdExecution::AbortCause::ForceTerminate);
                }
            }

            wy::ErrorStatus error = pCmdMgr->executeCommand(cmdName);
            bool isOk = (error == wy::ErrorStatus::Ok);
            QString message = isOk ? QStringLiteral("") : QString::number(static_cast<int>(error));
            _ipc->completeCurrentRequest(isOk, message);
        }
        else
        {
            assert(false);
        }
        return true;
    }

    return QApplication::notify(receiver, event);
}

void wy3dQApplication::initIpc()
{
    _ipc = new IpcServer(this);
    _ipc->setEventReceiver(this);
    _ipc->start(17999, QHostAddress::LocalHost);
}

ExecuteCommandEvent::ExecuteCommandEvent(const std::string& cmdName)
    : QEvent(eventType()), _strCmdName(cmdName)
{
}

RunPythonScriptEvent::RunPythonScriptEvent(const std::string& scriptFileFullPath)
    : QEvent(eventType()), _scriptFileFullPath(scriptFileFullPath)
{
}

RunGuiCommandEvent::RunGuiCommandEvent(const std::string& cmdName)
    : QEvent(eventType()), _strCmdName(cmdName)
{
}