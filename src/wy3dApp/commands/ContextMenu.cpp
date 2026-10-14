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

#include <wy3dApp/ContextMenu.h>

#include <cassert>

#include <QCoreApplication>
#include <QMouseEvent>

namespace wy3dApp {

ContextMenu::ContextMenu()
    : QMenu()
{
    this->connect(this, &QMenu::aboutToShow, this,
        [this]()
        {
            if (QCoreApplication* app = QCoreApplication::instance())
            {
                app->installEventFilter(this);
            }
        });
    this->connect(this, &QMenu::aboutToHide, this,
        [this]()
        {
            if (QCoreApplication* app = QCoreApplication::instance())
            {
                app->removeEventFilter(this);
            }
        });
}

void ContextMenu::setCompleteSelectionVisible(bool visible)
{
    _isCompleteSelectionVisible = visible;
}

void ContextMenu::setClearSelectionVisible(bool visible)
{
    _isClearSelectionVisible = visible;
}

bool ContextMenu::isCompleteSelectionVisible() const
{
    return _isCompleteSelectionVisible;
}

bool ContextMenu::isClearSelectionVisible() const
{
    return _isClearSelectionVisible;
}

bool ContextMenu::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj);

    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        assert(mouseEvent);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QPoint globalPos = mouseEvent->globalPos();
            if (!this->geometry().contains(globalPos) && this->isVisible())
            {
                this->hide();
                return true;
            }
        }
    }

    return QMenu::eventFilter(obj, event);
}

} // namespace wy3dApp
