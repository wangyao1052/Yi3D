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

#include <wyVector3.h>
#include "SketchPlaneDialog.h"
#include <cassert>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchPlaneDialog::SketchPlaneDialog(QWidget* parent) : QDialog(parent)
{
    this->setWindowTitle(tr("Setting Sketch Plane"));
    this->setMinimumWidth(250);
    this->setMinimumHeight(100);
    this->setFixedSize(250, 120);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // 创建下拉列表
    _systemPlaneCombo = new QComboBox(this);
    _systemPlaneCombo->addItem("XOY", SystemSketchPlane::PLANE_XOY);
    _systemPlaneCombo->addItem("YOZ", SystemSketchPlane::PLANE_YOZ);
    _systemPlaneCombo->addItem("XOZ", SystemSketchPlane::PLANE_XOZ);
    layout->addWidget(_systemPlaneCombo);
   
    // 创建OK和Cancel按钮 
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton(tr("Ok"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // 信号槽
    this->connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    this->connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelBtnClicked()));
}

void SketchPlaneDialog::onOkBtnClicked()
{
    int selectedPlane = _systemPlaneCombo->currentData().toInt();
    switch (selectedPlane)
    {
    case SystemSketchPlane::PLANE_XOY:
        _sketchPlane = wy3d::Plane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
        break;
    case SystemSketchPlane::PLANE_YOZ:
        _sketchPlane = wy3d::Plane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 0.0, 0.0), wy::Vector3(0.0, 1.0, 0.0));
        break;
    case SystemSketchPlane::PLANE_XOZ:
        _sketchPlane = wy3d::Plane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, -1.0, 0.0), wy::Vector3(1.0, 0.0, 0.0));
        break;
    default:
        assert(false);
        break;
    }

    this->accept();
}

void SketchPlaneDialog::onCancelBtnClicked()
{
    this->reject();
}

} // namespace wy3dApp
