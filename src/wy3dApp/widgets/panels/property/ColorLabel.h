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

#ifndef WY3DAPP_COLOR_LABEL_H
#define WY3DAPP_COLOR_LABEL_H

#include <QLabel>
#include <wy3dColor.h>

// Read-only color display; color is changed by the Set Color command.
class ColorLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ColorLabel(const wy3d::Color& color, bool isTheSameValue, QWidget* parent = nullptr);
};

#endif // WY3DAPP_COLOR_LABEL_H
