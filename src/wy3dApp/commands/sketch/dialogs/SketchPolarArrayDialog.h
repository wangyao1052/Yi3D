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

#ifndef WY3DAPP_SKETCH_POLAR_ARRAY_DIALOG_H
#define WY3DAPP_SKETCH_POLAR_ARRAY_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchPolarArrayDialog : public QDialog
{
    Q_OBJECT
public:
    struct Options
    {
        double minTotalAngle;  // 最小总角度
        double maxTotalAngle;  // 最大总角度
        unsigned int minCount; // 最小实例数量
        unsigned int maxCount; // 最大实例数量
    };
    SketchPolarArrayDialog(
        const QString& title,
        double totalAngle, unsigned int count, bool isCCW,
        const Options& options, QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(250, 150);
    }

    // 获取总的角度
    double getTotalAngle() const
    {
        return _totalAngle;
    }

    // 获取总的项目数
    unsigned int getCount() const
    {
        return _count;
    }

    // 是否是逆时针
    bool isCCW() const
    {
        return _isCCW;
    }

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    QLineEdit* _totalAngleEdit;
    QLineEdit* _countEdit;
    QRadioButton* _ccwRadio;
    QRadioButton* _cwRadio;
    double _totalAngle;
    unsigned int _count;
    bool _isCCW;

    Options _options;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLAR_ARRAY_DIALOG_H
