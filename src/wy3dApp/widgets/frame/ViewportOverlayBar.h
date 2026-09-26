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

#ifndef WY3DAPP_VIEWPORT_OVERLAY_BAR_H
#define WY3DAPP_VIEWPORT_OVERLAY_BAR_H

#include <QWidget>

class QAction;
class QEvent;
class QHBoxLayout;
class QShowEvent;

// A row of square icon-only buttons floating over the top-center of a viewport.
// Each button is bound to the caller's action, so icon, tooltip, enabled state and triggering
// stay the action's: the bar adds nothing of its own to a command.
// The bar never activates, so the viewport keeps the keyboard.
class ViewportOverlayBar : public QWidget
{
    Q_OBJECT
public:
    explicit ViewportOverlayBar(QWidget* pViewportWidget);
    virtual ~ViewportOverlayBar();

    // Appends one square icon-only button bound to pAction.
    void addAction(QAction* pAction);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;

private:
    void anchorToTopLeft();

private:
    QHBoxLayout* _pLayout;
};

#endif // WY3DAPP_VIEWPORT_OVERLAY_BAR_H
