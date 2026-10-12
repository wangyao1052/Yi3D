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

#ifndef WY3D_APP_PARAM_NAMES_TRANSLATION_H
#define WY3D_APP_PARAM_NAMES_TRANSLATION_H

#include <string>
#include <map>
#include <QObject>
#include <QString>

#include "wy3dAppDefs.h"

namespace wy3dApp {

class WY3DAPP_EXPORT ParamNamesTranslation : public QObject
{
    Q_OBJECT
public:
    static ParamNamesTranslation& instance();

    // 获取参数的显示名称
    QString getParamDisplayName(const std::string& className, const std::string& paramName) const;

private:
    // 参数名 <> 参数显示名
    std::map<std::string, QString> _paramName2DisplayName;

private:
    explicit ParamNamesTranslation(QObject* parent = nullptr);
    ~ParamNamesTranslation();
};

}

#endif // WY3D_APP_PARAM_NAMES_TRANSLATION_H
