///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_CONTEXT_MENU_H
#define WY3DAPP_CONTEXT_MENU_H

#include <QMenu>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// The context menu of a GuiCommand.
// Swallows the outside click that closes the menu so that click does not reach the viewport.
class WY3DAPP_EXPORT ContextMenu : public QMenu
{
public:
    ContextMenu();

    // Complete / Clear Selection item visibility (hidden by default).
    void setCompleteSelectionVisible(bool visible);
    void setClearSelectionVisible(bool visible);

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

private:
    friend class GuiCommand;

    bool isCompleteSelectionVisible() const;
    bool isClearSelectionVisible() const;

    bool _isCompleteSelectionVisible = false;
    bool _isClearSelectionVisible = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_CONTEXT_MENU_H
