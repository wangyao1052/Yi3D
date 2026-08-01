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

#include "PythonScriptExecutor.h"

#include <QCoreApplication>
#include <QFileInfo>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#include <sstream>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include "application/Application.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/output/OutputWidget.h"

typedef void (*Py_InitializeFunc)();
typedef void (*Py_FinalizeFunc)();
typedef int (*PyRun_SimpleFileExFunc)(FILE* fp, const char* filename, int closeit);
typedef int (*PyRun_SimpleStringFunc)(const char* command);

PythonScriptExecutor::Error _executePythonScript(void* hPythonLib, const std::string& scriptFullPath)
{
#if defined(_WIN32)
    Py_InitializeFunc Py_Initialize = (Py_InitializeFunc)GetProcAddress((HMODULE)hPythonLib, "Py_Initialize");
    Py_FinalizeFunc Py_Finalize = (Py_FinalizeFunc)GetProcAddress((HMODULE)hPythonLib, "Py_Finalize");
    PyRun_SimpleFileExFunc PyRun_SimpleFileEx = (PyRun_SimpleFileExFunc)GetProcAddress((HMODULE)hPythonLib, "PyRun_SimpleFileEx");
    PyRun_SimpleStringFunc PyRun_SimpleString = (PyRun_SimpleStringFunc)GetProcAddress((HMODULE)hPythonLib, "PyRun_SimpleString");
#else
    Py_InitializeFunc Py_Initialize = reinterpret_cast<Py_InitializeFunc>(dlsym(hPythonLib, "Py_Initialize"));
    Py_FinalizeFunc Py_Finalize = reinterpret_cast<Py_FinalizeFunc>(dlsym(hPythonLib, "Py_Finalize"));
    PyRun_SimpleFileExFunc PyRun_SimpleFileEx = reinterpret_cast<PyRun_SimpleFileExFunc>(dlsym(hPythonLib, "PyRun_SimpleFileEx"));
    PyRun_SimpleStringFunc PyRun_SimpleString = reinterpret_cast<PyRun_SimpleStringFunc>(dlsym(hPythonLib, "PyRun_SimpleString"));
#endif
    if (!Py_Initialize || !Py_Finalize || !PyRun_SimpleFileEx || !PyRun_SimpleString)
    {
        return PythonScriptExecutor::Error::InvalidPythonLibrary;
    }

    // 重定向 Python 标准输入输出到临时文件
    auto redirectPythonStdOut = [PyRun_SimpleString]()
    {
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("file = None");
        PyRun_SimpleString("try: file = open('python_output.txt', 'w', encoding='utf-8')\nexcept: pass");
        PyRun_SimpleString("initStdOut = sys.stdout");
        PyRun_SimpleString("sys.stdout = file");
        PyRun_SimpleString("initStdErr = sys.stderr");
        PyRun_SimpleString("sys.stderr = file");
    };

    // 恢复 Python 标准输入输出
    auto restorePythonStdOut = [PyRun_SimpleString]()
    {
        PyRun_SimpleString("sys.stdout = initStdOut");
        PyRun_SimpleString("sys.stderr = initStdErr");
        PyRun_SimpleString("if file: file.close()");
    };

    // 初始化 Python 运行时
    Py_Initialize();

#ifndef _WIN32
    // Linux/macOS: 将 YI3D 所在目录加入 sys.path
    {
        QString appDir = QCoreApplication::applicationDirPath();
        QString addPathCmd = QString("import sys; sys.path.insert(0, '%1')").arg(appDir);
        PyRun_SimpleString(addPathCmd.toUtf8().constData());
    }
#endif

    // 将 Python I/O 重定向到临时文件
    redirectPythonStdOut();

    // 执行脚本文件
    int runRet(0);
    {
#if defined(_WIN32)
        // Windows上fopen不支持UTF-8路径，先用MultiByteToWideChar转为宽字符再用_wfopen
        int wlen = MultiByteToWideChar(CP_UTF8, 0, scriptFullPath.c_str(), -1, NULL, 0);
        if (wlen <= 0)
        {
            restorePythonStdOut();
            Py_Finalize();
            return PythonScriptExecutor::Error::OpenPythonScriptFileFailed;
        }
        std::wstring wpath(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, scriptFullPath.c_str(), -1, wpath.data(), wlen);
        FILE* fp = _wfopen(wpath.c_str(), L"r");
#else
        FILE* fp = fopen(scriptFullPath.c_str(), "r");
#endif
        if (!fp)
        {
            restorePythonStdOut();
            Py_Finalize();
            return PythonScriptExecutor::Error::OpenPythonScriptFileFailed;
        }

        std::stringstream ssTitle;
        ssTitle << "----------" << scriptFullPath << "----------";
        Application::instance().getDockPanelManager()->findWidgetAs<OutputWidget>(
            DockPanelIds::Output)->info(ssTitle.str());

        runRet = PyRun_SimpleFileEx(fp, scriptFullPath.c_str(), 1);
    }

    // 还原标准输入输出
    restorePythonStdOut();

    // 结束 Python 运行时
    Py_Finalize();

    // 刷新输出窗口
    Application::instance().getDockPanelManager()->findWidgetAs<OutputWidget>(
        DockPanelIds::Output)->refresh();

    if (0 == runRet)
    {
        return PythonScriptExecutor::Error::NoError;
    }
    else
    {
        return PythonScriptExecutor::Error::RunScriptError;
    }
}

PythonScriptExecutor::PythonScriptExecutor()
{
}

PythonScriptExecutor::~PythonScriptExecutor()
{
}

PythonScriptExecutor::Error PythonScriptExecutor::Run(const std::string& scriptFileFullPath)
{
    // 加载 Python 动态库
    QString appDir = QCoreApplication::applicationDirPath();
#if defined(_WIN32)
#  ifdef _DEBUG
    const QString pythonLibraryName = QStringLiteral("python310_d.dll");
#  else
    const QString pythonLibraryName = QStringLiteral("python310.dll");
#  endif
#else
    const QString pythonLibraryName = QStringLiteral(PYTHON_LIBRARY_NAME);
#endif
    QString pythonLibraryFullPath = appDir + QStringLiteral("/python3/") + pythonLibraryName;
    QFileInfo fileInfo(pythonLibraryFullPath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        return Error::PythonLibraryNotFound;
    }
#if defined(_WIN32)
    void* hPythonLib = (void*)LoadLibraryW(pythonLibraryFullPath.toStdWString().c_str());
#else
    void* hPythonLib = dlopen(pythonLibraryFullPath.toUtf8().constData(), RTLD_NOW | RTLD_GLOBAL);
#endif
    if (!hPythonLib)
    {
        return Error::LoadPythonLibraryFailed;
    }

    // 执行脚本
    PythonScriptExecutor::Error error = _executePythonScript(hPythonLib, scriptFileFullPath);

    // 无论脚本执行是否成功，只要存在未提交事务就中止事务
    this->abortActiveTransaction();

    // 释放 Python 动态库
#if defined(_WIN32)
    FreeLibrary((HMODULE)hPythonLib);
#else
    dlclose(hPythonLib);
#endif

    return error;
}

void PythonScriptExecutor::abortActiveTransaction()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    // 如果有激活事务则中止事务
    if (pDb->getTransactionManager()->getActiveTransaction())
    {
        pDb->getTransactionManager()->abortTransaction();
    }
}
