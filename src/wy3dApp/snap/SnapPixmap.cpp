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

#include "SnapPixmap.h"
#include <QPainter>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SnapPixmap& SnapPixmap::instance()
{
    static SnapPixmap instance;
    return instance;
}

#define PREPARE_PIXMAP QPixmap pixmap(WIDTH, HEIGHT); \
        pixmap.fill(background); \
        QPainter painter(&pixmap); \
        painter.setRenderHint(QPainter::Antialiasing, true); \
        painter.setPen(pen);

SnapPixmap::SnapPixmap()
{
    QColor background = Qt::white;
    QPen pen(Qt::black);
    pen.setWidth(2);

    // 水平
    {
        PREPARE_PIXMAP;
        painter.drawLine(2, 8, 14, 8);
        _horizontal = pixmap;
    }
    // 竖直
    {
        PREPARE_PIXMAP;
        painter.drawLine(8, 2, 8, 14);
        _vertical = pixmap;
    }
    // 平行
    {
        PREPARE_PIXMAP;
        painter.drawLine(9, 2, 1, 13);
        painter.drawLine(15, 2, 7, 13);
        _parallel = pixmap;
    }
    // 垂直
    {
        PREPARE_PIXMAP;
        painter.drawLine(2, 14, 14, 14);
        painter.drawLine(8, 2, 8, 14);
        _perpendicular = pixmap;
    }
    // 相等
    {
        PREPARE_PIXMAP;
        painter.drawLine(3, 10, 13, 10);
        painter.drawLine(3, 6, 13, 6);
        _equal = pixmap;
    }
    // 相切
    {
        PREPARE_PIXMAP;
        painter.drawEllipse(QPoint(7, 7), 4, 4);
        painter.drawLine(5, 16, 16, 5);
        _tanget = pixmap;
    }
    // 交点
    {
        PREPARE_PIXMAP;
        painter.drawLine(0, 11, 15, 4);
        painter.drawLine(0, 0, 15, 15);
        _intersection = pixmap;
    }
    // 端点
    {
        PREPARE_PIXMAP;
        painter.drawLine(3, 12, 13, 3);
        painter.drawEllipse(QPoint(12, 4), 2, 2);
        _end = pixmap;
    }
    // 中点
    {
        PREPARE_PIXMAP;
        painter.drawLine(2, 13, 14, 2);
        painter.drawEllipse(QPoint(8, 7), 2, 2);
        painter.drawPoint(QPoint(8, 6));
        _middle = pixmap;
    }
    // 中心点
    {
        PREPARE_PIXMAP;
        painter.drawEllipse(QPoint(8, 8), 5, 5);
        painter.drawEllipse(QPoint(8, 8), 1, 1);
        _center = pixmap;
    }
    // 原点
    {
        PREPARE_PIXMAP;
        QPen circlePen = pen;
        circlePen.setWidthF(1.5);
        painter.setPen(circlePen);
        painter.drawEllipse(QPoint(8, 8), 5, 5);
        painter.setPen(pen);
        painter.drawLine(8, 0, 8, 16);
        painter.drawLine(0, 8, 16, 8);
        _origin = pixmap;
    }
}

SnapPixmap::~SnapPixmap()
{
}

} // namespace wy3dApp
