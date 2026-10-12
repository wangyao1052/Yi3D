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

#ifndef WY3DAPP_SKETCH_POLYGON_DIALOG_H
#define WY3DAPP_SKETCH_POLYGON_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class PolygonSidesLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit PolygonSidesLineEdit(QWidget* parent = nullptr);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
};

class SketchPolygonDialog : public QDialog
{
    Q_OBJECT

public:
    SketchPolygonDialog(unsigned int numSides, unsigned int minSides, unsigned int maxSides,
        QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(200, 120);
    }

    // 获取多边形边数
    unsigned int getNumOfSides() const
    {
        return _numSides;
    }

    // 获取内接多边形还是外接多边形
    bool isInscribedPolygon() const;

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    PolygonSidesLineEdit* _sidesEdit;
    QRadioButton* _inscribedRadio; // 内接
    QRadioButton* _circumscribedRadio; // 外接
    unsigned int _numSides;
    unsigned int _minSides;
    unsigned int _maxSides;
};

}

#endif // WY3DAPP_SKETCH_POLYGON_DIALOG_H
