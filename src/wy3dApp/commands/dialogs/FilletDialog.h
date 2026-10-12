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

#ifndef WY3DAPP_FILLET_DIALOG_H
#define WY3DAPP_FILLET_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class FilletRadiusLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit FilletRadiusLineEdit(QWidget* parent = nullptr);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
};

class FilletDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FilletDialog(double radius, QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(200, 100);
    }

    // 获取圆角半径
    double getRadius() const
    {
        return _radius;
    }

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    FilletRadiusLineEdit* _radiusEdit;
    double _radius;
};

}

#endif // WY3DAPP_FILLET_DIALOG_H
