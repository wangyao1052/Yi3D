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

#include "utils/IconUtil.h"

#include <QHash>

QIcon IconUtil::get(const QString& path)
{
    // Heap allocated and deliberately never freed: a QIcon must be destroyed before QApplication,
    // and the destruction order of function-local statics is not guaranteed.
    static QHash<QString, QIcon>* pIcons = new QHash<QString, QIcon>;
    auto iter = pIcons->find(path);
    if (pIcons->end() == iter)
        iter = pIcons->insert(path, QIcon(path));
    return iter.value();
}
