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

#include "SnapTipWidget.h"
#include <QVBoxLayout>
#include <wy3dMath.h>
#include "SnapPixmap.h"
#include <wy3dApp/Application.h>
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SnapTipWidget_UseLabel::SnapTipWidget_UseLabel(QWidget* parent)
    : SnapTipWidget(parent), _label(nullptr)
{
    // 设置窗口无边框
    this->setWindowFlags(Qt::FramelessWindowHint);
    // 设置窗口背景为透明
    this->setAttribute(Qt::WA_TranslucentBackground);
    // 确保样式表背景透明
    this->setStyleSheet("background-color: transparent;");
    
    // 标签
    _label = new QLabel("123", this);
    _label->setAlignment(Qt::AlignCenter);
    _label->setStyleSheet("font-size: 12px; color: #333333;");
    
    // 布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(_label);
    this->setLayout(layout);
}

SnapTipWidget_UsePixmap::SnapTipWidget_UsePixmap(const QPixmap& pixmap, QWidget* parent)
    : SnapTipWidget(parent), _pixmap(pixmap)
{
    // 禁用自动背景填充,减少闪烁
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAutoFillBackground(false);

    // 设置窗口无边框
    this->setWindowFlags(Qt::FramelessWindowHint);
    // 始终置于最顶层
    //this->setWindowFlags(Qt::WindowStaysOnTopHint);
    // 工具提示窗口
    //this->setWindowFlags(Qt::ToolTip);

    // 禁用鼠标事件
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    // 禁用获取鼠标焦点
    this->setFocusPolicy(Qt::NoFocus);
    
    // 固定大小
    this->setFixedSize(SnapPixmap::WIDTH, SnapPixmap::HEIGHT);
}

void SnapTipWidget_UsePixmap::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, _pixmap);
}

void SnapTipWidget_UsePixmap::mouseMoveEvent(QMouseEvent* event)
{
    //event->ignore();
}

SnapTipWidgetMgr& SnapTipWidgetMgr::instance()
{
    static SnapTipWidgetMgr instance;
    return instance;
}

SnapTipWidgetMgr::SnapTipWidgetMgr() :
    _horizontal(nullptr),
    _vertical(nullptr),
    _parallel(nullptr),
    _perpendicular(nullptr), 
    _equal(nullptr),
    _tangent(nullptr),
    _intersection(nullptr),
    _end(nullptr),
    _middle(nullptr),
    _center(nullptr)
{
    QWidget* pMainWindow = Application::instance().getMainWindow();
    assert(pMainWindow);

    _horizontal = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getHorizontal(), pMainWindow);
    _horizontal->hide();
    _vertical = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getVertical(), pMainWindow);
    _vertical->hide();
    _parallel = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getParallel(), pMainWindow);
    _parallel->hide();
    _perpendicular = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getPerpendicular(), pMainWindow);
    _perpendicular->hide();
    _equal = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getEqual(), pMainWindow);
    _equal->hide();
    _tangent = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getTangent(), pMainWindow);
    _tangent->hide();
    _intersection = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getIntersection(), pMainWindow);
    _intersection->hide();
    _end = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getEnd(), pMainWindow);
    _end->hide();
    _middle = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getMiddle(), pMainWindow);
    _middle->hide();
    _center = new SnapTipWidget_UsePixmap(SnapPixmap::instance().getCenter(), pMainWindow);
    _center->hide();
}

SnapTipWidgetMgr::~SnapTipWidgetMgr()
{
}

SketchSnapTipMouseFollow::SketchSnapTipMouseFollow()
    : _timerId(0)
{
}

SketchSnapTipMouseFollow::~SketchSnapTipMouseFollow()
{
    this->stop();
}

void SketchSnapTipMouseFollow::stop()
{
    if (_timerId != 0)
    {
        this->killTimer(_timerId);
        _timerId = 0;
    }

    for (SnapTipWidget* pTipWidget : _tips)
    {
        assert(pTipWidget);
        if (pTipWidget) pTipWidget->hide();
    }
}

void SketchSnapTipMouseFollow::addTipWidget(SnapTipWidget* pTipWidget)
{
    if (!pTipWidget) return;
    for (SnapTipWidget* pTip : _tips)
    {
        if (pTip == pTipWidget) return;
    }
    _tips.emplace_back(pTipWidget);
}

void SketchSnapTipMouseFollow::start()
{
    if (_tips.empty()) return;

    // Global tip widgets retain their previous positions, so move them before
    // showing to avoid flashing once at the last snap location.
    this->updatePositions();

    for (SnapTipWidget* pTipWidget : _tips)
    {
        assert(pTipWidget);
        if (pTipWidget) pTipWidget->show();
    }

    if (_timerId == 0)
    {
        _timerId = this->startTimer(10);
    }
}

void SketchSnapTipMouseFollow::timerEvent(QTimerEvent* event)
{
    if (!event || event->timerId() != _timerId)
    {
        QObject::timerEvent(event);
        return;
    }

    this->updatePositions();
}

void SketchSnapTipMouseFollow::updatePositions()
{
    static const QPoint delta(12, 12);
    static const QPoint spacing(SnapPixmap::WIDTH + 5, 0);

    QPoint newPosGlobal = QCursor::pos() + delta;
    for (SnapTipWidget* pTipWidget : _tips)
    {
        assert(pTipWidget);
        if (!pTipWidget)
        {
            continue;
        }

        QWidget* pParentWidget = pTipWidget->parentWidget();
        assert(pParentWidget);
        if (pParentWidget)
        {
            const QPoint newPosLocal = pParentWidget->mapFromGlobal(newPosGlobal);
            if (newPosLocal != pTipWidget->pos())
            {
                pTipWidget->move(newPosLocal);
            }
        }

        newPosGlobal += spacing;
    }
}

}
