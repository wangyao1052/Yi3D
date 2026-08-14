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

#include "widgets/frame/MainWindow.h"
#include <QTranslator>
#include <QDir>

#include "wy3dAppDefs.h"

#include "application/wy3dQApplication.h"
#include "application/Application.h"
#include "application/AutoSave.h"
#include "application/Config.h"
#include "application/crashDump/CrashDump.h"
#include "commands/FileCommands.h"
#include "environments/gateway/GatewayEnvironment.h"

int main(int argc, char *argv[])
{
    // core dump
    wy3dApp::CrashDump::initialize(QDir::currentPath());

    // 初始化
    wy3dQApplication app(argc, argv);
    Application::instance().initialize();
    app.setWindowIcon(QIcon(":/images/Yi3D.svg"));

    // 语言
    {
        QString qstrLang = Application::instance().getConfig()->system.language;
        QTranslator* pTranslator = new QTranslator();
        if (pTranslator->load(":/translator/" + qstrLang))
        {
            app.installTranslator(pTranslator);
        }
        else
        {
            delete pTranslator;
        }
    }

    // 主窗口
    MainWindow mainWindow;
    mainWindow.showMaximized();

    // Enter gateway environment.
    std::unique_ptr<wyap::Environment> gatewayEnv = std::make_unique<GatewayEnvironment>();
    Application::instance().getEnvManager()->enterEnvironment(
        std::move(gatewayEnv),
        wyap::ExecutionMode::Sync);

    // added by wangyao 2025.08.23 {
    // 处理双击.wy3dt/.wy3db文件,第二个参数是文件路径
    if (argc == 2)
    {
#ifdef _WIN32
        QString filePath = QString::fromLocal8Bit(argv[1]);
#else
        // 其他平台(Linux&Macos)下argv是UTF-8编码
        QString filePath = QString::fromUtf8(argv[1]);
#endif
        QFileInfo fileInfo(filePath);
        if (fileInfo.exists() && fileInfo.isFile())
        {
            OpenFileCommand::openFile(filePath.toStdString());
        }
    }
    // }

    // Safety net on normal exit: delete all .autosave files produced by this
    // session (idempotent, closeEvent has already cleaned up).
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []()
    {
        Application::instance().getAutoSave()->cleanupOnExit();
    });

    return app.exec();
}
