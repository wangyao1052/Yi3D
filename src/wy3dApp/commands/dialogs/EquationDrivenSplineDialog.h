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

#ifndef WY3DAPP_EQUATION_DRIVEN_SPLINE_DIALOG_H
#define WY3DAPP_EQUATION_DRIVEN_SPLINE_DIALOG_H

#include <vector>
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <muParser.h>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class EquationDrivenSplineDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EquationDrivenSplineDialog(QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(400, 200);
    }

    // 获取插值点
    const std::vector<wy::Vector2>& getPoints() const
    {
        return _points;
    }

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    bool evalute();
    void initParser(mu::Parser& parser);

private:
    QLineEdit* _xtEdit;
    QLineEdit* _ytEdit;
    QLineEdit* _t1Edit;
    QLineEdit* _t2Edit;
    QSpinBox* _pointsNumSpinBox;
    int _minNum;
    int _maxNum;

    std::vector<wy::Vector2> _points;
};

}

#endif // WY3DAPP_EQUATION_DRIVEN_SPLINE_DIALOG_H
