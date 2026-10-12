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

#pragma once

#include <QString>
#include <QLabel>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class StatusBarHelper
{
public:
    StatusBarHelper() : _pTipsLabel(nullptr) {}
    ~StatusBarHelper() = default;

    void init(QLabel* pTipsLabel)
    {
        _pTipsLabel = pTipsLabel;
        reset();
    }

    void setTips(const QString& tips)
    {
        if (_pTipsLabel) _pTipsLabel->setText(tips);
    }

    void reset()
    {
        if (_pTipsLabel) _pTipsLabel->setText("");
    }

private:
    QLabel* _pTipsLabel;
};

}
