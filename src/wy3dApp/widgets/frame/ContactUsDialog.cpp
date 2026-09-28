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

#include "ContactUsDialog.h"

#include <cassert>

#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
// Half of the 344x344 source images.
constexpr int kQrCodeSize = 172;

QWidget* createQrCodeColumn(QWidget* pParent, const QString& title, const QString& imagePath)
{
    assert(pParent);

    QWidget* pColumn = new QWidget(pParent);
    QVBoxLayout* pColumnLayout = new QVBoxLayout(pColumn);
    pColumnLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* pLabelTitle = new QLabel(pColumn);
    pLabelTitle->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
    pLabelTitle->setFont(QFont("Microsoft YaHei", 12, QFont::Normal));
    pLabelTitle->setText(title);
    pColumnLayout->addWidget(pLabelTitle);

    QLabel* pLabelImage = new QLabel(pColumn);
    pLabelImage->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
    pLabelImage->setPixmap(QPixmap(imagePath).scaled(
        kQrCodeSize, kQrCodeSize, Qt::KeepAspectRatio, Qt::FastTransformation));
    pColumnLayout->addWidget(pLabelImage);

    return pColumn;
}
} // namespace

ContactUsDialog::ContactUsDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle(tr("Contact Us"));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    this->setLayout(mainLayout);
    {
        QHBoxLayout* qrCodeLayout = new QHBoxLayout();
        qrCodeLayout->addStretch();
        qrCodeLayout->addWidget(createQrCodeColumn(this, tr("Official QQ Group"),
            ":/images/Contact_QQGroup.jpg"));
        qrCodeLayout->addSpacing(40);
        qrCodeLayout->addWidget(createQrCodeColumn(this, tr("WeChat Official Account"),
            ":/images/Contact_WeChat.jpg"));
        qrCodeLayout->addStretch();
        mainLayout->addLayout(qrCodeLayout);

        QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
        pButtonBox->button(QDialogButtonBox::Close)->setText(tr("Close"));
        connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(pButtonBox);
    }

    mainLayout->setContentsMargins(30, 20, 30, 20);
}

ContactUsDialog::~ContactUsDialog()
{
}
