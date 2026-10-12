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

#ifndef WY3DAPP_LINEAR_PATTERN_DIALOG_H
#define WY3DAPP_LINEAR_PATTERN_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class LinearPatternDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LinearPatternDialog(
        unsigned int count1st, double spacing1st,
        unsigned int count2nd, double spacing2nd,
        QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(250, 120);
    }

    // 获取方向1上的实例数量
    unsigned int getCount1st() const
    {
        return _count1st;
    }

    // 获取方向1上的实例间距
    double getSpacing1st() const
    {
        return _spacing1st;
    }

    // 获取方向2上的实例数量
    unsigned int getCount2nd() const
    {
        return _count2nd;
    }

    // 获取方向2上的实例间距
    double getSpacing2nd() const
    {
        return _spacing2nd;
    }

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    QLineEdit* _count1stEdit;
    QLineEdit* _spacing1stEdit;
    QLineEdit* _count2ndEdit;
    QLineEdit* _spacing2ndEdit;

    unsigned int _count1st;
    double _spacing1st;
    unsigned int _count2nd;
    double _spacing2nd;
};

}

#endif // WY3DAPP_LINEAR_PATTERN_DIALOG_H
