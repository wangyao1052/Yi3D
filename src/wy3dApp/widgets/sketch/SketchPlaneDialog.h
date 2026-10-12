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

#ifndef WY3DAPP_SKETCH_PLANE_DIALOG_H
#define WY3DAPP_SKETCH_PLANE_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <wy3dPlane.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SketchPlaneDialog : public QDialog
{
    Q_OBJECT
public:
    enum SystemSketchPlane
    {
        PLANE_XOY = 0,
        PLANE_YOZ = 1,
        PLANE_XOZ = 2
    };
public:
    explicit SketchPlaneDialog(QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(250, 120);
    }

    // 获取草图平面
    wy3d::Plane getSketchPlane() const
    {
        return _sketchPlane;
    }

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    QComboBox* _systemPlaneCombo;
    wy3d::Plane _sketchPlane;
};

}

#endif // WY3DAPP_SKETCH_PLANE_DIALOG_H
