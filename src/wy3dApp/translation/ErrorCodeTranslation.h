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

#ifndef WY3D_APP_ERROR_CODE_TRANSLATION_H
#define WY3D_APP_ERROR_CODE_TRANSLATION_H

#include <map>
#include <QObject>
#include <QString>
#include <wy3dErrorCode.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class ErrorCodeTranslation : public QObject
{
    Q_OBJECT
public:
    static ErrorCodeTranslation& instance();

    // 获取错误码描述
    QString getErrorCodeDescription(std::uint32_t code) const;
    inline QString getErrorCodeDescription(wy3d::ErrorCode errorCode) const
    {
        return getErrorCodeDescription(static_cast<std::uint32_t>(errorCode));
    }

private:
    // 错误码 <> 错误码描述
    std::map<std::uint32_t, QString> _code2Desc;

private:
    explicit ErrorCodeTranslation(QObject* parent = nullptr);
    ~ErrorCodeTranslation();
};

}

#endif // WY3D_APP_ERROR_CODE_TRANSLATION_H
