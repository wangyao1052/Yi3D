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

#include "FeatureTreeView.h"
#include <QKeyEvent>
#include "utils/CopyPasteUtil.h"
#include "utils/TransactionUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

FeatureTreeView::FeatureTreeView(QWidget* parent) : QTreeView(parent)
{
}

FeatureTreeView::~FeatureTreeView()
{
}

void FeatureTreeView::keyPressEvent(QKeyEvent* event)
{
    if (TransactionUtil::hasActiveTransaction()) // 当前有激活事务
    {
        return QTreeView::keyPressEvent(event);
    }

    if (event->key() == Qt::Key_Delete)
    {
        emit eraseCurrentSelections();
        return;
    }
    else if (event->modifiers() & Qt::Modifier::CTRL)
    {
        // Ctrl+C
        if (event->key() == Qt::Key_C)
        {
            CopyPasteUtil::CopyReturn ret = CopyPasteUtil::copy();
            if (ret != CopyPasteUtil::CopyReturn::Ok)
            {
                CopyPasteUtil::showCopyErrorMsgBox(ret);
            }
            return;
        }
        // Ctrl+V
        else if (event->key() == Qt::Key_V)
        {
            CopyPasteUtil::paste();
            return;
        }
    }

    return QTreeView::keyPressEvent(event);
}

} // namespace wy3dApp
