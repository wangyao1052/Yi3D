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

#include "MessageBoxUtil.h"
#include <cassert>
#include <QCoreApplication>
#include <QMessageBox>
#include <wy3dErrorCode.h>
#include <wy3dApp/Application.h>
#include "translation/ErrorCodeTranslation.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

void MessageBoxUtil::showError(unsigned int code)
{
    if (wy3d::isError(code))
    {
        QMessageBox::critical(nullptr, Application::instance().getTitle(),
            ErrorCodeTranslation::instance().getErrorCodeDescription(code));
    }
    else if (wy3d::isWarning(code))
    {
        QMessageBox::warning(nullptr, Application::instance().getTitle(),
            ErrorCodeTranslation::instance().getErrorCodeDescription(code));
    }
    else
    {
        assert(false);
    }
}

void MessageBoxUtil::showOpenFileError(wy::ErrorStatus error)
{
    switch (error)
    {
    case wy::ErrorStatus::Ok:
        assert(false);
        break;

    case wy::ErrorStatus::CantOpenFile:
        QMessageBox::critical(nullptr, Application::instance().getTitle(),
            QCoreApplication::translate("MessageBox",
                "Cannot open file!"));
        break;

    case wy::ErrorStatus::BadFile:
        QMessageBox::critical(nullptr, Application::instance().getTitle(),
            QCoreApplication::translate("MessageBox",
                "Invalid file!"));
        break;

    case wy::ErrorStatus::FileVersionTooNew:
        QMessageBox::critical(nullptr, Application::instance().getTitle(),
            QCoreApplication::translate("MessageBox",
                "The file cannot be opened because it was created with a newer version of Yi3D."));
        break;

    case wy::ErrorStatus::Error:
    case wy::ErrorStatus::DatabaseNotEmpty:
    default:
        QMessageBox::critical(nullptr, Application::instance().getTitle(),
            QCoreApplication::translate("MessageBox",
                "Open file error!"));
        break;
    }
}

void MessageBoxUtil::showInformation_NoAvailableSketches()
{
    showInformation(QCoreApplication::translate("MessageBox",
        "No sketches available. Please create a sketch using the sketch command first."));
}

void MessageBoxUtil::showInformation(const QString& qstr)
{
    QMessageBox::information(nullptr, Application::instance().getTitle(), qstr);
}

void MessageBoxUtil::showWarning(const QString& qstr)
{
    QMessageBox::warning(nullptr, Application::instance().getTitle(), qstr);
}

void MessageBoxUtil::showError(const QString& qstr)
{
    QMessageBox::critical(nullptr, Application::instance().getTitle(), qstr);
}

void MessageBoxUtil::showWarning_InvalidValue(double allowMin, double allowMax)
{
    QMessageBox::warning(nullptr, 
        QCoreApplication::translate("MessageBox", "Warning"),
        QCoreApplication::translate("MessageBox", "Please enter a number that is greater than or equal to ") +
        QString::number(allowMin) +
        QCoreApplication::translate("MessageBox", " and less than or equal to ") +
        QString::number(allowMax) +
        QCoreApplication::translate("MessageBox", "."));
}

void MessageBoxUtil::showWarning_PleaseInputNumber()
{
    QMessageBox::warning(nullptr,
        QCoreApplication::translate("MessageBox", "Warning"),
        QCoreApplication::translate("MessageBox", "Please input number."));
}

} // namespace wy3dApp
