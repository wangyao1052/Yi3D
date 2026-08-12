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

#if defined(_WIN32)

#include <windows.h>
#include <dbghelp.h>

#include <QDateTime>
#include <QDir>

#include <wy3dAppDefs.h>
#include <wy3dAppVersion.h>

NS_WY3DAPP_BEG

typedef BOOL(__stdcall* tMDWD)(
    IN HANDLE hProcess,
    IN DWORD ProcessId,
    IN HANDLE hFile,
    IN MINIDUMP_TYPE DumpType,
    IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
    IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
    IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL
    );

static tMDWD s_pMDWD = nullptr;
static HMODULE s_hDbgHelpMod = nullptr;
static std::wstring s_miniDumpFileName;

static LONG __stdcall WINAPI wy3dappUnhandledExceptionFilter(EXCEPTION_POINTERS* pEx)
{
    HANDLE hFile = CreateFileW(s_miniDumpFileName.c_str(),
        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION info;
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = pEx;
        info.ClientPointers = true;

        if (s_pMDWD)
        {
            s_pMDWD(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &info, NULL, NULL);
        }
        CloseHandle(hFile);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void platformInitializeCrashDump(const QString& dumpDir)
{
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString fileName = QString("crash-%1-%2-%3.dmp").arg(YI3D_APP_VERSION_STRING).arg(timestamp).arg(GetCurrentProcessId());
    s_miniDumpFileName = QDir(dumpDir).absoluteFilePath(fileName).toStdWString();

    s_hDbgHelpMod = LoadLibraryA("dbghelp.dll");
    if (s_hDbgHelpMod != NULL)
    {
        s_pMDWD = (tMDWD)GetProcAddress(s_hDbgHelpMod, "MiniDumpWriteDump");
    }

    SetUnhandledExceptionFilter(wy3dappUnhandledExceptionFilter);
}

void platformShutdownCrashDump()
{
    if (s_hDbgHelpMod)
    {
        FreeLibrary(s_hDbgHelpMod);
        s_hDbgHelpMod = nullptr;
    }
    s_pMDWD = nullptr;
    s_miniDumpFileName.clear();
}

NS_WY3DAPP_END

#endif
