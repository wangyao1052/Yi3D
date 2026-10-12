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

#include "FeatureTreeHoverDelegate.h"
#include <cassert>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

FeatureTreeHoverDelegate::FeatureTreeHoverDelegate(QTreeView* treeView, QObject* parent)
    : QStyledItemDelegate(parent), _treeView(treeView)
{
    assert(_treeView);
}

void FeatureTreeHoverDelegate::setHoveredIndex(const QModelIndex& index)
{
    if (_hoveredIndex != index)
    {
        // 获取旧悬停项的矩形区域
        QRect oldRect;
        if (_hoveredIndex.isValid())
        {
            oldRect = _treeView->visualRect(_hoveredIndex);
        }

        // 更新悬停索引
        _hoveredIndex = index;

        // 获取新悬停项的矩形区域
        QRect newRect;
        if (_hoveredIndex.isValid())
        {
            newRect = _treeView->visualRect(_hoveredIndex);
        }

        // 只更新新旧两个区域
        if (oldRect.isValid())
        {
            _treeView->viewport()->update(oldRect);
        }
        if (newRect.isValid())
        {
            _treeView->viewport()->update(newRect);
        }
    }
}

void FeatureTreeHoverDelegate::paint(QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    // 在特征树控件不可选的情况下在Hover处绘制悬停矩形
    if (_treeView->selectionMode() == QAbstractItemView::NoSelection)
    {
        if (index == _hoveredIndex && option.state & QStyle::State_MouseOver)
        {
            painter->save();
            {
                painter->setBrush(Qt::NoBrush);
                QPen pen;
                pen.setColor(QColor(128, 128, 128));  // 经典灰色 (RGB 128,128,128)
                pen.setWidth(1);
                pen.setStyle(Qt::SolidLine);
                painter->setPen(pen);
                QRect rect = option.rect.adjusted(1, 1, -1, -1);
                painter->drawRect(rect);
            }
            painter->restore();
        }
    }
}

} // namespace wy3dApp
