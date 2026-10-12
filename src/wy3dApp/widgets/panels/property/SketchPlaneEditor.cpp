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

#include "SketchPlaneEditor.h"
#include "ParamLineEdit.h"
#include "utils/MessageBoxUtil.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dMath.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchPlaneEditor::SketchPlaneEditor(const wy3d::Plane& plane, bool isTheSameValue, QWidget* parent)
    : QWidget(parent)
    , _pOriginX(nullptr), _pOriginY(nullptr), _pOriginZ(nullptr)
    , _pNormalX(nullptr), _pNormalY(nullptr), _pNormalZ(nullptr)
    , _pXDirX(nullptr), _pXDirY(nullptr), _pXDirZ(nullptr)
    , _pApplyBtn(nullptr)
{
    auto newLabel = [this](const QString& text) -> QLabel*
    {
        QLabel* pLabel = new QLabel(text, this);
        QFont font = pLabel->font();
        font.setPointSize(12);
        pLabel->setFont(font);
        return pLabel;
    };

    auto toStr = [isTheSameValue](double v) -> QString {
        return isTheSameValue ? QString::number(v) : QString("-");
    };

    auto newRow = [this, &newLabel, &toStr](const wy::Vector3& vec,
        QLineEdit*& pXEdit, QLineEdit*& pYEdit, QLineEdit*& pZEdit) -> QGridLayout*
    {
        QGridLayout* pLayout = new QGridLayout();

        QLabel* pLabelX = newLabel("X");
        QLineEdit* pLineEditX = new QLineEdit(this);
        pLineEditX->setText(toStr(vec.x()));
        pLayout->addWidget(pLabelX, 0, 0);
        pLayout->addWidget(pLineEditX, 0, 1);
        ParamLineEdit::setWidgetFontSize(pLineEditX);

        QLabel* pLabelY = newLabel("Y");
        QLineEdit* pLineEditY = new QLineEdit(this);
        pLineEditY->setText(toStr(vec.y()));
        pLayout->addWidget(pLabelY, 0, 2);
        pLayout->addWidget(pLineEditY, 0, 3);
        ParamLineEdit::setWidgetFontSize(pLineEditY);

        QLabel* pLabelZ = newLabel("Z");
        QLineEdit* pLineEditZ = new QLineEdit(this);
        pLineEditZ->setText(toStr(vec.z()));
        pLayout->addWidget(pLabelZ, 0, 4);
        pLayout->addWidget(pLineEditZ, 0, 5);
        ParamLineEdit::setWidgetFontSize(pLineEditZ);

        pXEdit = pLineEditX;
        pYEdit = pLineEditY;
        pZEdit = pLineEditZ;
        return pLayout;
    };

    QVBoxLayout* pMainLayout = new QVBoxLayout(this);
    QGridLayout* pGridLayout = new QGridLayout();

    int row = 0;

    // origin
    QLabel* pLabelOrigin = newLabel(tr("Origin "));
    pGridLayout->addWidget(pLabelOrigin, row, 0);
    QGridLayout* pOriginLayout = newRow(plane.getOrigin(), _pOriginX, _pOriginY, _pOriginZ);
    pGridLayout->addLayout(pOriginLayout, row, 1);
    ++row;

    // normal
    QLabel* pLabelNormal = newLabel(tr("Z Axis "));
    pGridLayout->addWidget(pLabelNormal, row, 0);
    QGridLayout* pNormalLayout = newRow(plane.getNormal(), _pNormalX, _pNormalY, _pNormalZ);
    pGridLayout->addLayout(pNormalLayout, row, 1);
    ++row;

    // x axis direction
    QLabel* pLabelXDir = newLabel(tr("X Axis "));
    pGridLayout->addWidget(pLabelXDir, row, 0);
    QGridLayout* pXDirLayout = newRow(plane.getXDir(), _pXDirX, _pXDirY, _pXDirZ);
    pGridLayout->addLayout(pXDirLayout, row, 1);
    ++row;

    // separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setLineWidth(1);
    separator->setFixedHeight(4);
    pGridLayout->addWidget(separator, row, 0, 1, 2);
    ++row;

    // apply button
    _pApplyBtn = new QPushButton(tr("Apply"), this);
    pGridLayout->addWidget(_pApplyBtn, row, 0, 1, 2, Qt::AlignHCenter | Qt::AlignVCenter);
    ParamLineEdit::setWidgetFontSize(_pApplyBtn);

    // 边框包裹
    QFrame* pFrame = new QFrame(this);
    pFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    pFrame->setLayout(pGridLayout);
    pMainLayout->addWidget(pFrame);
    this->setLayout(pMainLayout);

    connect(_pApplyBtn, &QPushButton::clicked, this, &SketchPlaneEditor::applyRequested);
}

wy3d::Plane SketchPlaneEditor::getPlane(bool& ok) const
{
    QString qstrInvalidInput = tr("Invalid input.");

    auto toDouble = [&qstrInvalidInput](QLineEdit* pEdit, double& value) -> bool
    {
        if (!pEdit) { assert(false); return false; }
        bool convertOk(true);
        value = pEdit->text().toDouble(&convertOk);
        if (!convertOk)
        {
            MessageBoxUtil::showError(qstrInvalidInput);
            pEdit->setFocus();
            return false;
        }
        return true;
    };

    // origin
    double originX, originY, originZ;
    if (!toDouble(_pOriginX, originX)) return wy3d::Plane();
    if (!toDouble(_pOriginY, originY)) return wy3d::Plane();
    if (!toDouble(_pOriginZ, originZ)) return wy3d::Plane();
    wy::Vector3 origin(originX, originY, originZ);

    // normal
    double nx, ny, nz;
    if (!toDouble(_pNormalX, nx)) return wy3d::Plane();
    if (!toDouble(_pNormalY, ny)) return wy3d::Plane();
    if (!toDouble(_pNormalZ, nz)) return wy3d::Plane();
    wy::Vector3 normal(nx, ny, nz);
    normal.normalize();
    if (normal.length() < 0.5)
    {
        MessageBoxUtil::showError(tr("Invalid Normal data."));
        return wy3d::Plane();
    }

    // xDir
    double xx, xy, xz;
    if (!toDouble(_pXDirX, xx)) return wy3d::Plane();
    if (!toDouble(_pXDirY, xy)) return wy3d::Plane();
    if (!toDouble(_pXDirZ, xz)) return wy3d::Plane();
    wy::Vector3 xDir(xx, xy, xz);
    xDir.normalize();
    if (xDir.length() < 0.5)
    {
        MessageBoxUtil::showError(tr("Invalid X Axis data."));
        return wy3d::Plane();
    }

    wy::Vector3 yDir = normal.cross(xDir);
    yDir.normalize();
    if (yDir.length() < 0.5)
    {
        MessageBoxUtil::showError(qstrInvalidInput);
        return wy3d::Plane();
    }

    xDir = yDir.cross(normal);
    xDir.normalize();
    if (xDir.length() < 0.5)
    {
        MessageBoxUtil::showError(qstrInvalidInput);
        return wy3d::Plane();
    }

    wy3d::Plane plane(origin, normal, xDir);
    if (!plane.isValid())
    {
        MessageBoxUtil::showError(qstrInvalidInput);
        return wy3d::Plane();
    }

    ok = true;
    return plane;
}

} // namespace wy3dApp
