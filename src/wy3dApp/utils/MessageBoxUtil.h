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

#ifndef WY3DAPP_MESSAGE_BOX_UTIL_H
#define WY3DAPP_MESSAGE_BOX_UTIL_H

#include <QString>
#include <wyErrors.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MessageBoxUtil
{
public:
    // 显示错误:错误码
    static void showError(unsigned int code);
    // 显示打开文件错误
    static void showOpenFileError(wy::ErrorStatus error);
    // 显示信息:没有可用的草图
    static void showInformation_NoAvailableSketches();

    // 显示信息
    static void showInformation(const QString& qstr);
    // 显示警告
    static void showWarning(const QString& qstr);
    // 显示错误
    static void showError(const QString& qstr);

    // 警告:请输入有效值
    static void showWarning_InvalidValue(double allowMin, double allowMax);
    // 警告:请输入数值
    static void showWarning_PleaseInputNumber();
};

}

#endif // WY3DAPP_MESSAGE_BOX_UTIL_H
