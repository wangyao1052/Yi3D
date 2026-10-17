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

#ifndef WY3DAPP_SNAP_TIP_WIDGET_H
#define WY3DAPP_SNAP_TIP_WIDGET_H

#include <vector>
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SnapTipWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SnapTipWidget(QWidget* parent = nullptr) : QWidget(parent), _index(0) {}

    unsigned int getIndex() const { return _index; }

    void updatePosition()
    {
        static QPoint delta(15, 15);
        QPoint newPos = QCursor::pos() + delta;
        if (newPos != this->pos())
        {
            move(newPos);
        }
    }

private:
    unsigned int _index;
};

class SnapTipWidget_UseLabel : public SnapTipWidget
{
    Q_OBJECT
public:
    explicit SnapTipWidget_UseLabel(QWidget* parent = nullptr);

private:
    QLabel* _label;
};

class SnapTipWidget_UsePixmap : public SnapTipWidget
{
    Q_OBJECT
public:
    explicit SnapTipWidget_UsePixmap(const QPixmap& pixmap, QWidget* parent = nullptr);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;

private:
    const QPixmap& _pixmap;
};

class SnapTipWidgetMgr
{
public:
    static SnapTipWidgetMgr& instance();

    // 水平
    SnapTipWidget_UsePixmap* getHorizontal() const { return  _horizontal; }
    // 竖直
    SnapTipWidget_UsePixmap* getVertical() const { return _vertical; }
    // 平行
    SnapTipWidget_UsePixmap* getParallel() const { return _parallel; }
    // 垂直
    SnapTipWidget_UsePixmap* getPerpendicular() const { return _perpendicular; }
    // 相等
    SnapTipWidget_UsePixmap* getEqual() const { return _equal; }
    // 相切
    SnapTipWidget_UsePixmap* getTangent() const { return _tangent; }
    // 交点
    SnapTipWidget_UsePixmap* getIntersection() const { return _intersection; }
    // 端点
    SnapTipWidget_UsePixmap* getEnd() const { return _end; }
    // 中点
    SnapTipWidget_UsePixmap* getMiddle() const { return _middle; }
    // 中心点
    SnapTipWidget_UsePixmap* getCenter() const { return _center; }
    // 原点
    SnapTipWidget_UsePixmap* getOrigin() const { return _origin; }

private:
    SnapTipWidgetMgr();
    ~SnapTipWidgetMgr();

private:
    // 水平
    SnapTipWidget_UsePixmap* _horizontal;
    // 竖直
    SnapTipWidget_UsePixmap* _vertical;
    // 平行
    SnapTipWidget_UsePixmap* _parallel;
    // 垂直
    SnapTipWidget_UsePixmap* _perpendicular;
    // 相等
    SnapTipWidget_UsePixmap* _equal;
    // 相切
    SnapTipWidget_UsePixmap* _tangent;
    // 交点
    SnapTipWidget_UsePixmap* _intersection;
    // 端点
    SnapTipWidget_UsePixmap* _end;
    // 中点
    SnapTipWidget_UsePixmap* _middle;
    // 中心点
    SnapTipWidget_UsePixmap* _center;
    // 原点
    SnapTipWidget_UsePixmap* _origin;
};

class SnapTipMouseFollow : public QObject
{
public:
    SnapTipMouseFollow();
    ~SnapTipMouseFollow();

    void addTipWidget(SnapTipWidget* pTipWidget);
    void start();
    void stop();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    void updatePositions();

private:
    int _timerId;
    std::vector<SnapTipWidget*> _tips;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP_TIP_WIDGET_H
