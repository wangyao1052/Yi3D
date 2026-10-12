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

#ifndef WY3DAPP_SKETCH_PLANE_EDITOR_H
#define WY3DAPP_SKETCH_PLANE_EDITOR_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <wy3dPlane.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SketchPlaneEditor : public QWidget
{
    Q_OBJECT
public:
    explicit SketchPlaneEditor(const wy3d::Plane& plane, bool isTheSameValue, QWidget* parent = nullptr);

    // 从控件读取平面数据，校验失败返回 ok=false
    wy3d::Plane getPlane(bool& ok) const;

signals:
    void applyRequested();

private:
    QLineEdit* _pOriginX;
    QLineEdit* _pOriginY;
    QLineEdit* _pOriginZ;
    QLineEdit* _pNormalX;
    QLineEdit* _pNormalY;
    QLineEdit* _pNormalZ;
    QLineEdit* _pXDirX;
    QLineEdit* _pXDirY;
    QLineEdit* _pXDirZ;
    QPushButton* _pApplyBtn;
};

}

#endif // WY3DAPP_SKETCH_PLANE_EDITOR_H
