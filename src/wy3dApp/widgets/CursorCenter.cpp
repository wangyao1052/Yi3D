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

#include "CursorCenter.h"
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include "CursorType.h"

CursorCenter& CursorCenter::instance()
{
    static CursorCenter instance;
    return instance;
}

CursorCenter::CursorCenter()
{
    // CursorType::SelectElements
    {
        QPixmap pixmap(13, 13);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::black);
        painter.drawRect(0, 0, 12, 12);
        _cursorSelElems = QCursor(pixmap, 6, 6);
    }
    // CursorType::Locate
    {
        QPixmap pixmap(41, 41);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::black);
        painter.drawLine(20, 0, 20, 40);
        painter.drawLine(0, 20, 40, 20);
        _cursorLocate = QCursor(pixmap, 20, 20);
    }
    // CursorType::Delete
    {
        QPixmap pixmap(23, 23);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::black);
        painter.drawRect(0, 10, 12, 12);
        QPen redPen(Qt::red);
        redPen.setWidth(2);
        painter.setPen(redPen);
        painter.drawLine(13, 9, 18, 4);
        painter.drawLine(13, 4, 18, 9);
        _cursorDelete = QCursor(pixmap, 6, 16);
    }
    // CursorType::Forbid
    {
        QPixmap pixmap(23, 23);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::black);
        painter.drawRect(0, 10, 12, 12); // 绘制矩形选择框
        QPen pen(Qt::red);
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing, true); // 启用抗锯齿
        painter.drawEllipse(QPoint(16, 5), 4, 4);
        painter.drawLine(14, 8, 18, 2);
        _cursorForbid = QCursor(pixmap, 6, 16);
    }
    {
        QPixmap pixmap = QIcon(":/images/Cursor_Rotate.svg").pixmap(QSize(32, 32));
        _cursorRotate = QCursor(pixmap, 16, 16);
    }
}

QCursor CursorCenter::getCursor(CursorType cursorType) const
{
    switch (cursorType)
    {
    case CursorType::Select:
        return QCursor(Qt::ArrowCursor);

    case CursorType::SelectElements:
        return _cursorSelElems;

    case CursorType::Locate:
        return _cursorLocate;

    case CursorType::Delete:
        return _cursorDelete;

    case CursorType::Forbid:
        return _cursorForbid;

    case CursorType::Rotate:
        return _cursorRotate;

    case CursorType::Pan:
        return QCursor(Qt::SizeAllCursor);
    }

    return QCursor(Qt::ArrowCursor);
}
