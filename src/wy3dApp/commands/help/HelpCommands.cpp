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

#include "HelpCommands.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include "widgets/frame/AboutDialog.h"
#include "widgets/frame/ShortcutKeysDialog.h"
#include "utils/MessageBoxUtil.h"


int AboutCommand::run()
{
    AboutDialog dlg;
    dlg.exec();

    return 0;
}

int HelpDocumentationCommand::run()
{
    bool success(false);
    try
    {
        QUrl url("https://www.wangyaosoft.com/help/");
        success = QDesktopServices::openUrl(url);
    }
    catch (...)
    {
        success = false;
    }

    if (!success)
    {
        MessageBoxUtil::showWarning(QCoreApplication::translate("Help",
            "Unable to open the browser or the help documentation URL."));
    }
    return 0;
}

int ShortcutKeysCommand::run()
{
    ShortcutKeysDialog dlg;
    dlg.exec();

    return 0;
}