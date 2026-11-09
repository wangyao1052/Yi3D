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

#ifndef WY3DAPP_PLUGIN_MANAGER_DIALOG_H
#define WY3DAPP_PLUGIN_MANAGER_DIALOG_H

#include <QDialog>
#include <QPoint>
#include <QString>

#include <wy3dApp/wy3dAppDefs.h>

class QLabel;
class QPushButton;
class QTableWidget;

namespace wy3dApp {

// Plugin management dialog (Tools menu): lists discovered plugins and allows
// session-only unload / reload.
class PluginManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PluginManagerDialog(QWidget* parent = nullptr);
    ~PluginManagerDialog() override;

    // Whether a PluginManagerDialog instance currently exists. It counts as
    // open from construction until destruction, including while plugin code
    // opens its own modal dialog during plugin load/unload.
    static bool isActive();

private:
    void initUi();
    void refresh();
    void updateButtons();
    void unloadSelected();
    void loadSelected();
    void showContextMenu(const QPoint& pos);

    QString selectedKey() const;
    QString displayNameForKey(const QString& key) const;

private:
    QTableWidget* _pTable;
    QPushButton* _pUnloadButton;
    QPushButton* _pLoadButton;
    QPushButton* _pRefreshButton;
    QPushButton* _pCloseButton;
    QLabel* _pHintLabel;
};

} // namespace wy3dApp

#endif // WY3DAPP_PLUGIN_MANAGER_DIALOG_H
