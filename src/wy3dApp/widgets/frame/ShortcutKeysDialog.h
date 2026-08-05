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

#include <QDialog>

class QButtonGroup;
class QStackedWidget;
class QToolButton;

class ShortcutKeysDialog : public QDialog
{
    Q_OBJECT

public:
    ShortcutKeysDialog(QWidget *parent = Q_NULLPTR);
    ~ShortcutKeysDialog();

    virtual QSize sizeHint() const override { return QSize(600, 500); }

private:
    void addShortcut(QWidget* parent, const QString& key, const QString& description);
    QToolButton* createCategoryButton(const QString& text, int id, QButtonGroup* pGroup);

    QStackedWidget* _pStackedWidget;
};
