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

#ifndef WY3DAPP_SNAP_PIXMAP_H
#define WY3DAPP_SNAP_PIXMAP_H

#include <QPixmap>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SnapPixmap
{
public:
    static SnapPixmap& instance();

    // 宽高
    static inline int WIDTH = 16;
    static inline int HEIGHT = 16;

    // 水平
    const QPixmap& getHorizontal() const { return _horizontal; }
    // 竖直
    const QPixmap& getVertical() const { return _vertical; }
    // 平行
    const QPixmap& getParallel() const { return _parallel; }
    // 垂直
    const QPixmap& getPerpendicular() const { return _perpendicular; }
    // 相等
    const QPixmap& getEqual() const { return _equal; }
    // 相切
    const QPixmap& getTangent() const { return _tanget; }
    // 交点
    const QPixmap& getIntersection() const { return _intersection; }
    // 端点
    const QPixmap& getEnd() const { return _end; }
    // 中点
    const QPixmap& getMiddle() const { return _middle; }
    // 中心点
    const QPixmap& getCenter() const { return _center; }
    // 象限
    const QPixmap& getQuadrant() const { return _quadrant; }
    // 原点
    const QPixmap& getOrigin() const { return _origin; }

private:
    SnapPixmap();
    ~SnapPixmap();
    
private:
    // 水平
    QPixmap _horizontal;
    // 竖直
    QPixmap _vertical;
    // 平行
    QPixmap _parallel;
    // 垂直
    QPixmap _perpendicular;
    // 相等
    QPixmap _equal;
    // 相切
    QPixmap _tanget;
    // 交点
    QPixmap _intersection;
    // 端点
    QPixmap _end;
    // 中点
    QPixmap _middle;
    // 中心点
    QPixmap _center;
    // 象限
    QPixmap _quadrant;
    // 原点
    QPixmap _origin;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP_TIP_WIDGET_H
